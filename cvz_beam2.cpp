#include <bits/stdc++.h>
using namespace std;
using Clock = chrono::steady_clock;

static const int       MAXN      = 100;
static const double    ASH_SPEED = 1000.0;
static const double    ZOM_SPEED = 400.0;
static const long long RANGE2    = 2000LL * 2000LL;   // 사정거리의 제곱
static const int       MAX_DEPTH = 30;                // 탐색 깊이 상한

static const double TIME_BUDGET       = 50.0;    // 턴당 사용 시간 (제한 100ms)
static const double TIME_BUDGET_FIRST = 900.0;   // 첫 턴 (제한 1000ms)
static const int    ROLLOUT_STEPS     = 60;
static const bool   USE_LURE_MOVES    = true;    // 8방향 이동 후보 사용 여부

// 점수 = 10 * (생존자 수)^2 * (1+2+3+5+8+... , 같은 턴에 잡은 수만큼)
// 뒤쪽 피보나치 누적합을 미리 표로 만들어 둔다.
static long long fibSum[MAXN + 1];

static void initFib() {
    long long a = 1, b = 2, sum = 0;
    fibSum[0] = 0;
    for (int k = 1; k <= MAXN; k++) {
        sum += a;
        fibSum[k] = sum;
        if (b < (long long)1e15) { long long next = a + b; a = b; b = next; }
    }
}

// 빔 탐색은 상태를 수십만 번 복사하므로 vector 대신 고정 배열을 쓴다.
struct State {
    int ax, ay;
    int hn; int hx[MAXN], hy[MAXN];
    int zn; int zx[MAXN], zy[MAXN];   // 좌표는 항상 phase 1 이후 기준
    long long score;
    int status;                       // 0 진행중, 1 승리, -1 패배
};

static inline long long d2(int x1, int y1, int x2, int y2) {
    long long dx = x1 - x2, dy = y1 - y2;
    return dx * dx + dy * dy;
}

// speed만큼 목표 방향으로 이동. 도달하면 정확히 목표, 아니면 내림 처리.
static inline void moveTowards(int &x, int &y, int tx, int ty, double speed) {
    long long dist2 = d2(x, y, tx, ty);
    if ((double)dist2 <= speed * speed) { x = tx; y = ty; return; }
    double dist = sqrt((double)dist2);
    x = (int)floor(x + speed * (tx - x) / dist);
    y = (int)floor(y + speed * (ty - y) / dist);
}

// 좀비가 가장 가까운 사람(Ash 포함)쪽으로 이동.
// 거리가 같으면 먼저 비교한 쪽(Ash -> 작은 id)이 우선.
static void moveZombies(State &s) {
    for (int i = 0; i < s.zn; i++) {
        int targetX = s.ax, targetY = s.ay;
        long long bestDist = d2(s.zx[i], s.zy[i], s.ax, s.ay);
        for (int j = 0; j < s.hn; j++) {
            long long dist = d2(s.zx[i], s.zy[i], s.hx[j], s.hy[j]);
            if (dist < bestDist) {
                bestDist = dist;
                targetX = s.hx[j]; targetY = s.hy[j];
            }
        }
        moveTowards(s.zx[i], s.zy[i], targetX, targetY, ZOM_SPEED);
    }
}

// 한 턴 진행: phase 2(이동) -> 3(사격) -> 4(식사) -> 다음 턴의 phase 1
static void simulate(State &s, int tx, int ty) {
    moveTowards(s.ax, s.ay, tx, ty, ASH_SPEED);

    int killed = 0, w = 0;
    int survivorsBefore = s.hn;              // 점수는 사격 시점 생존자 기준
    for (int i = 0; i < s.zn; i++) {
        if (d2(s.ax, s.ay, s.zx[i], s.zy[i]) <= RANGE2) killed++;
        else { s.zx[w] = s.zx[i]; s.zy[w] = s.zy[i]; w++; }
    }
    s.zn = w;
    if (killed > 0)
        s.score += 10LL * survivorsBefore * survivorsBefore * fibSum[killed];

    w = 0;
    for (int i = 0; i < s.hn; i++) {
        bool eaten = false;
        for (int j = 0; j < s.zn; j++)
            if (s.zx[j] == s.hx[i] && s.zy[j] == s.hy[i]) { eaten = true; break; }
        if (!eaten) { s.hx[w] = s.hx[i]; s.hy[w] = s.hy[i]; w++; }
    }
    s.hn = w;

    if (s.hn == 0) { s.status = -1; s.score = 0; return; }   // 전멸 = 0점
    if (s.zn == 0) { s.status =  1; return; }
    moveZombies(s);
}

// 16000x9000 전체를 볼 수 없으므로 의미 있는 지점만 후보로 만든다. 최대 39개.
static int genMoves(const State &s, int *moveX, int *moveY) {
    static pair<long long, int> dist[MAXN];
    int n = 0;

    moveX[n] = s.ax; moveY[n] = s.ay; n++;               // 제자리(대기)

    int kz = min(s.zn, 10);                              // 가까운 좀비로 직진
    for (int i = 0; i < s.zn; i++) dist[i] = {d2(s.ax, s.ay, s.zx[i], s.zy[i]), i};
    partial_sort(dist, dist + kz, dist + s.zn);
    int nearZ[10];
    for (int i = 0; i < kz; i++) {
        nearZ[i] = dist[i].second;
        moveX[n] = s.zx[nearZ[i]]; moveY[n] = s.zy[nearZ[i]]; n++;
    }

    int kp = min(kz, 5);                                 // 좀비 2마리의 중점
    for (int i = 0; i < kp; i++)
        for (int j = i + 1; j < kp; j++) {
            moveX[n] = (s.zx[nearZ[i]] + s.zx[nearZ[j]]) / 2;
            moveY[n] = (s.zy[nearZ[i]] + s.zy[nearZ[j]]) / 2;
            n++;
        }

    int kt = min(kz, 4);                                 // 좀비 3마리의 무게중심
    for (int i = 0; i < kt; i++)
        for (int j = i + 1; j < kt; j++)
            for (int k = j + 1; k < kt; k++) {
                moveX[n] = (s.zx[nearZ[i]] + s.zx[nearZ[j]] + s.zx[nearZ[k]]) / 3;
                moveY[n] = (s.zy[nearZ[i]] + s.zy[nearZ[j]] + s.zy[nearZ[k]]) / 3;
                n++;
            }

    int kh = min(s.hn, 6);                               // 가까운 사람으로 직진(구조)
    for (int i = 0; i < s.hn; i++) dist[i] = {d2(s.ax, s.ay, s.hx[i], s.hy[i]), i};
    partial_sort(dist, dist + kh, dist + s.hn);
    for (int i = 0; i < kh; i++) {
        int j = dist[i].second;
        moveX[n] = s.hx[j]; moveY[n] = s.hy[j]; n++;
    }

    // 8방향 한 칸 이동. 사정권에 들어온 좀비는 자동으로 죽으므로, 여러 마리를
    // 모아서 한 번에 잡으려면 사거리 밖에서 거리를 조절해야 한다. 위 후보들은
    // 전부 "유닛 좌표로 직진"이라 그 움직임을 표현하지 못한다.
    if (USE_LURE_MOVES) {
        static const int DIR8[8][2] = {
            { 1000, 0 }, { -1000, 0 }, { 0, 1000 }, { 0, -1000 },
            { 707, 707 }, { 707, -707 }, { -707, 707 }, { -707, -707 }
        };
        for (int i = 0; i < 8; i++) {
            moveX[n] = min(15999, max(0, s.ax + DIR8[i][0]));
            moveY[n] = min(8999,  max(0, s.ay + DIR8[i][1]));
            n++;
        }
    }
    return n;
}

// 남은 좀비를 몇 덩어리로 묶어 잡을 수 있는지 추정해 기대 배율을 돌려준다.
// 그냥 zn마리로 세면(배율 1) 좀비를 뭉치게 만드는 수를 알아보지 못한다.
static double gridMultiplier(const State &s, int offset) {
    const int CELL = 2800;
    const int GW = 16000 / CELL + 2, GH = 9000 / CELL + 2;
    static int count[32 * 32];
    int cells = GW * GH;
    for (int i = 0; i < cells; i++) count[i] = 0;

    for (int i = 0; i < s.zn; i++) {
        int cx = min(GW - 1, max(0, (s.zx[i] + offset) / CELL));
        int cy = min(GH - 1, max(0, (s.zy[i] + offset) / CELL));
        count[cy * GW + cx]++;
    }
    long long total = 0;
    for (int i = 0; i < cells; i++)
        if (count[i] > 0) total += fibSum[count[i]];
    return (double)total;
}

static double estimateGroupMultiplier(const State &s) {
    if (s.zn <= 1) return (double)s.zn;

    if (s.zn <= 32) {                        // 적을 때는 거리 기준 그리디가 정확
        static bool used[MAXN];
        for (int i = 0; i < s.zn; i++) used[i] = false;
        const long long CLUSTER_R2 = 3200LL * 3200LL;
        long long total = 0;
        for (int i = 0; i < s.zn; i++) {
            if (used[i]) continue;
            used[i] = true;
            int size = 1;
            for (int j = i + 1; j < s.zn; j++) {
                if (used[j]) continue;
                if (d2(s.zx[i], s.zy[i], s.zx[j], s.zy[j]) <= CLUSTER_R2) {
                    used[j] = true; size++;
                }
            }
            total += fibSum[size];
        }
        return (double)total;
    }
    // 많을 때는 O(zn) 격자로 근사. 경계에 걸린 덩어리가 갈라지지 않도록
    // 반 칸 어긋난 격자도 계산해 더 잘 뭉친 쪽을 쓴다.
    return max(gridMultiplier(s, 0), gridMultiplier(s, 1400));
}

static double evaluate(const State &s) {
    if (s.status == -1) return -1e18;
    double value = (double)s.score;
    if (s.status == 1) return value;

    // 사람마다 (좀비가 도달하는 턴 수) - (Ash가 그 좀비를 사정권에 넣는 턴 수)를
    // 구해 0~1 가중치로 환산해 더한다. 계단 함수로 판정하면 Ash가 한 걸음
    // 다가가도 값이 변하지 않아 탐색이 구조 경로를 알아보지 못한다.
    double expectedSurvivors = 0.0;
    for (int i = 0; i < s.hn; i++) {
        long long bestDist2 = LLONG_MAX;
        int bestZombie = -1;
        for (int j = 0; j < s.zn; j++) {
            long long dist = d2(s.hx[i], s.hy[i], s.zx[j], s.zy[j]);
            if (dist < bestDist2) { bestDist2 = dist; bestZombie = j; }
        }
        if (bestZombie < 0) { expectedSurvivors += 1.0; continue; }

        double turnsZombie = sqrt((double)bestDist2) / ZOM_SPEED;
        double ashToZombie = sqrt((double)d2(s.ax, s.ay,
                                             s.zx[bestZombie], s.zy[bestZombie]));
        double turnsAsh = max(0.0, ashToZombie - 2000.0) / ASH_SPEED;
        double weight = (turnsZombie - turnsAsh + 1.0) / 3.0;
        expectedSurvivors += min(1.0, max(0.0, weight));
    }

    value += 10.0 * expectedSurvivors * expectedSurvivors * estimateGroupMultiplier(s);

    long long nearest = LLONG_MAX;           // 동점 방지용 미세 보정
    for (int i = 0; i < s.zn; i++)
        nearest = min(nearest, d2(s.ax, s.ay, s.zx[i], s.zy[i]));
    value -= 0.0005 * sqrt((double)nearest);
    return value;
}

static inline uint64_t mix(uint64_t h, uint64_t v) {
    h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    return h;
}

static uint64_t hashState(const State &s) {
    uint64_t h = 1469598103934665603ULL;
    h = mix(h, (uint64_t)s.ax * 16001ULL + (uint64_t)s.ay);
    h = mix(h, (uint64_t)s.hn);
    for (int i = 0; i < s.zn; i++)
        h = mix(h, (uint64_t)s.zx[i] * 16001ULL + (uint64_t)s.zy[i]);
    return h;
}

// 가장 가까운 덩어리의 무게중심을 향해 끝까지 플레이하고 실제 점수를 돌려준다.
// evaluate()는 값이 거의 평평해서 빔 폭을 넓혀도 고르는 수가 같았다. 롤아웃은
// 실제로 끝까지 가 본 점수라 변별력이 있고, 그 자체가 둘 수 있는 수열이다.
static long long greedyRollout(State s, int maxSteps) {
    for (int step = 0; step < maxSteps && s.status == 0; step++) {
        int seed = 0;
        long long bestDist = LLONG_MAX;
        for (int i = 0; i < s.zn; i++) {
            long long d = d2(s.ax, s.ay, s.zx[i], s.zy[i]);
            if (d < bestDist) { bestDist = d; seed = i; }
        }
        const long long R2 = 3200LL * 3200LL;
        long long sumX = 0, sumY = 0;
        int cnt = 0;
        for (int i = 0; i < s.zn; i++) {
            if (d2(s.zx[seed], s.zy[seed], s.zx[i], s.zy[i]) <= R2) {
                sumX += s.zx[i]; sumY += s.zy[i]; cnt++;
            }
        }
        simulate(s, cnt ? (int)(sumX / cnt) : s.zx[seed],
                    cnt ? (int)(sumY / cnt) : s.zy[seed]);
    }
    return (s.status == -1) ? 0 : s.score;
}

// fx, fy는 이 노드로 이어진 "이번 턴의 첫 수". 부모 포인터 없이 답을 꺼낼 수 있다.
struct Node { State s; int fx, fy; double val; };

static Clock::time_point turnStart;
static inline double elapsedMs() {
    return chrono::duration<double, milli>(Clock::now() - turnStart).count();
}

struct SearchResult {
    long long winScore = -1;      // 승리로 끝난 수열의 실제 점수 (>=0 이면 찾음)
    int winX = 0, winY = 0;
    double heurVal = -1e18;       // 끝까지 못 갔을 때 쓰는 보조값
    int heurX = 0, heurY = 0;
};

static void beamSearch(const State &root, int beamWidth, double limitMs,
                       SearchResult &out) {
    static vector<Node> layer, nextLayer;
    static vector<char> keep;
    layer.clear(); nextLayer.clear();
    layer.push_back({root, root.ax, root.ay, evaluate(root)});

    int moveX[64], moveY[64];
    unordered_set<uint64_t> seen;
    seen.reserve(beamWidth * 4);

    for (int depth = 1; depth <= MAX_DEPTH; depth++) {
        nextLayer.clear();
        seen.clear();

        for (size_t p = 0; p < layer.size(); p++) {
            if (layer[p].s.status != 0) continue;

            int m = genMoves(layer[p].s, moveX, moveY);
            for (int i = 0; i < m; i++) {
                Node child;
                child.s = layer[p].s;
                simulate(child.s, moveX[i], moveY[i]);
                child.fx = (depth == 1) ? moveX[i] : layer[p].fx;
                child.fy = (depth == 1) ? moveY[i] : layer[p].fy;

                if (child.s.status == 1) {            // 실제 점수끼리 비교
                    if (child.s.score > out.winScore) {
                        out.winScore = child.s.score;
                        out.winX = child.fx; out.winY = child.fy;
                    }
                    continue;
                }
                if (child.s.status == -1) continue;

                child.val = evaluate(child.s);
                if (child.val > out.heurVal) {
                    out.heurVal = child.val;
                    out.heurX = child.fx; out.heurY = child.fy;
                }
                if (seen.insert(hashState(child.s)).second)
                    nextLayer.push_back(child);
            }
            if (elapsedMs() > limitMs) return;        // anytime
        }

        if (nextLayer.empty()) return;

        // 상위 beamWidth개만 남긴다. 점수순으로만 자르면 비슷한 상태만 남는
        // 빔 붕괴가 생기므로, 2000x2000 격자 칸마다 뽑는 수를 제한한다.
        if ((int)nextLayer.size() > beamWidth) {
            sort(nextLayer.begin(), nextLayer.end(),
                 [](const Node &a, const Node &b) { return a.val > b.val; });

            int perCell = max(2, beamWidth / 10);
            int cellCount[8 * 5] = {0};
            keep.assign(nextLayer.size(), 0);

            int taken = 0;
            for (size_t i = 0; i < nextLayer.size() && taken < beamWidth; i++) {
                int cell = min(7, nextLayer[i].s.ax / 2000) * 5
                         + min(4, nextLayer[i].s.ay / 2000);
                if (cellCount[cell] < perCell) {
                    cellCount[cell]++; keep[i] = 1; taken++;
                }
            }
            for (size_t i = 0; i < nextLayer.size() && taken < beamWidth; i++)
                if (!keep[i]) { keep[i] = 1; taken++; }

            vector<Node> picked;
            picked.reserve(taken);
            for (size_t i = 0; i < nextLayer.size(); i++)
                if (keep[i]) picked.push_back(nextLayer[i]);
            nextLayer.swap(picked);
        }

        // 살아남은 노드를 롤아웃 점수로 다시 순위 매긴다.
        for (size_t i = 0; i < nextLayer.size(); i++) {
            long long rolled = greedyRollout(nextLayer[i].s, ROLLOUT_STEPS);
            nextLayer[i].val = (double)rolled;
            if (rolled > out.winScore) {
                out.winScore = rolled;
                out.winX = nextLayer[i].fx; out.winY = nextLayer[i].fy;
            }
            if ((i & 15) == 0 && elapsedMs() > limitMs) return;
        }

        layer.swap(nextLayer);
    }
}

// 시간이 남는 동안 빔 폭을 2배씩 넓혀가며 반복 탐색한다.
static void chooseMove(const State &root, double limitMs, int &outX, int &outY) {
    outX = root.ax; outY = root.ay;               // 기본값: 제자리

    SearchResult best;
    for (int width = 32; width <= 4096; width *= 2) {
        SearchResult cur;
        beamSearch(root, width, limitMs, cur);

        if (cur.winScore > best.winScore) {
            best.winScore = cur.winScore;
            best.winX = cur.winX; best.winY = cur.winY;
        }
        if (cur.heurVal > best.heurVal) {
            best.heurVal = cur.heurVal;
            best.heurX = cur.heurX; best.heurY = cur.heurY;
        }
        if (elapsedMs() > limitMs * 0.5) break;   // 다음 폭은 비용이 2배
    }

    if (best.winScore >= 0)            { outX = best.winX;  outY = best.winY; }
    else if (best.heurVal > -1e17)     { outX = best.heurX; outY = best.heurY; }
}

int main() {
    initFib();

    long long gameScore = 0;
    bool firstTurn = true;
    bool hasPrediction = false;
    State predicted;

    while (true) {
        int x, y;
        if (!(cin >> x >> y)) break;
        turnStart = Clock::now();

        State s;
        s.ax = x; s.ay = y;
        s.score = gameScore;
        s.status = 0;

        cin >> s.hn;
        for (int i = 0; i < s.hn; i++) { int id; cin >> id >> s.hx[i] >> s.hy[i]; }

        cin >> s.zn;
        for (int i = 0; i < s.zn; i++) {
            // 현재 좌표는 버리고 "phase 1 이후" 좌표를 쓴다. 사격(phase 3)이
            // phase 1 다음에 일어나므로 이쪽이 올바른 기준이다.
            int id, curX, curY;
            cin >> id >> curX >> curY >> s.zx[i] >> s.zy[i];
        }

        // 지난 턴 예측과 실제 입력을 비교해 엔진이 규칙과 맞는지 확인한다.
        if (hasPrediction) {
            bool match = (predicted.ax == s.ax && predicted.ay == s.ay &&
                          predicted.hn == s.hn && predicted.zn == s.zn);
            for (int i = 0; match && i < s.zn; i++)
                if (predicted.zx[i] != s.zx[i] || predicted.zy[i] != s.zy[i])
                    match = false;
            if (!match) cerr << "ENGINE MISMATCH!" << endl;
        }

        int tx, ty;
        chooseMove(s, firstTurn ? TIME_BUDGET_FIRST : TIME_BUDGET, tx, ty);
        cout << tx << " " << ty << " BRAINS" << endl;

        predicted = s;
        simulate(predicted, tx, ty);
        gameScore = predicted.score;
        hasPrediction = (predicted.status == 0);
        firstTurn = false;
    }
    return 0;
}
