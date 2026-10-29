// The score is 92670.

#include <bits/stdc++.h>
using namespace std;
using Clock = chrono::steady_clock;

static const int       MAXN      = 100;           // 사람/좀비 수는 100 미만
static const double    ASH_SPEED = 1000.0;        // Ash가 한 턴에 이동하는 거리
static const double    ZOM_SPEED = 400.0;          // 좀비가 한 턴에 이동하는 거리
static const long long RANGE2    = 2000LL * 2000LL; // 사정거리(2000)의 제곱 — 거리 비교는 항상 제곱으로
static long long fibSum[MAXN + 1];

static void initFib() {
    long long a = 1, b = 2, sum = 0;
    fibSum[0] = 0;
    for (int k = 1; k <= MAXN; k++) {
        sum += a;
        fibSum[k] = sum;
        if (b < (long long)1e15) {                 // long long 오버플로 방지
            long long next = a + b;
            a = b; b = next;
        }
    }
}

struct State {
    int ax, ay;                         // Ash의 현재 좌표
    int hn;                             // 생존한 사람 수
    int hx[MAXN], hy[MAXN];             // 생존한 사람들의 좌표
    int zn;                             // 생존한 좀비 수
    int zx[MAXN], zy[MAXN];             // 생존한 좀비들의 좌표 (항상 "이번 턴 phase 1 이후" 기준)
    long long score;                    // 게임 시작부터 지금까지 누적 점수
    int status;                         // 0 = 진행 중, 1 = 승리, -1 = 패배(전멸)
};

// 두 점 사이 거리의 제곱 (sqrt를 쓰지 않아 빠르고 오차가 없다)
static inline long long d2(int x1, int y1, int x2, int y2) {
    long long dx = x1 - x2, dy = y1 - y2;
    return dx * dx + dy * dy;
}

static inline void moveTowards(int &x, int &y, int tx, int ty, double speed) {
    long long dist2 = d2(x, y, tx, ty);
    if ((double)dist2 <= speed * speed) {
        x = tx; y = ty;
        return;
    }
    double dist = sqrt((double)dist2);
    x = (int)floor(x + speed * (tx - x) / dist);
    y = (int)floor(y + speed * (ty - y) / dist);
}

// [phase 1] 모든 좀비가 "가장 가까운 사람(Ash 포함)" 쪽으로 400만큼 이동한다.
// 동률일 때는 먼저 비교한 쪽(=Ash, 그다음 id가 작은 사람)이 우선권을 갖는다.
static void moveZombies(State &s) {
    for (int i = 0; i < s.zn; i++) {
        int targetX = s.ax, targetY = s.ay;                 // 기본 타깃 = Ash
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

static void simulate(State &s, int tx, int ty) {
    // --- phase 2: Ash 이동 ---
    moveTowards(s.ax, s.ay, tx, ty, ASH_SPEED);

    // --- phase 3: 사정거리(2000) 안의 좀비를 모두 제거하고 점수를 더한다 ---
    int killed = 0, w = 0;
    int survivorsBefore = s.hn;                     // 사격 시점의 생존자 수 (점수 계산 기준)
    for (int i = 0; i < s.zn; i++) {
        if (d2(s.ax, s.ay, s.zx[i], s.zy[i]) <= RANGE2) {
            killed++;
        } else {
            s.zx[w] = s.zx[i]; s.zy[w] = s.zy[i]; w++;   // 순서를 유지하며 생존 좀비만 압축
        }
    }
    s.zn = w;
    if (killed > 0) {
        s.score += 10LL * survivorsBefore * survivorsBefore * fibSum[killed];
    }

    // --- phase 4: 좀비와 같은 좌표에 있는 사람은 잡아먹힌다 ---
    w = 0;
    for (int i = 0; i < s.hn; i++) {
        bool eaten = false;
        for (int j = 0; j < s.zn; j++) {
            if (s.zx[j] == s.hx[i] && s.zy[j] == s.hy[i]) { eaten = true; break; }
        }
        if (!eaten) { s.hx[w] = s.hx[i]; s.hy[w] = s.hy[i]; w++; }
    }
    s.hn = w;

    // --- 승패 판정 ---
    if (s.hn == 0) { s.status = -1; s.score = 0; return; }  // 전멸 = 이 판은 0점
    if (s.zn == 0) { s.status =  1; return; }                // 좀비 전멸 = 승리

    moveZombies(s);
}

static int genMoves(const State &s, int *moveX, int *moveY) {
    static pair<long long, int> dist[MAXN];
    int n = 0;

    // (1) 제자리 — Ash가 움직이지 않는 것도 합법적인 수다
    moveX[n] = s.ax; moveY[n] = s.ay; n++;

    // (2) 가장 가까운 좀비 최대 10마리를 향해 직진
    int kz = min(s.zn, 10);
    for (int i = 0; i < s.zn; i++) dist[i] = {d2(s.ax, s.ay, s.zx[i], s.zy[i]), i};
    partial_sort(dist, dist + kz, dist + s.zn);
    int nearZombie[10];
    for (int i = 0; i < kz; i++) {
        nearZombie[i] = dist[i].second;
        moveX[n] = s.zx[nearZombie[i]]; moveY[n] = s.zy[nearZombie[i]]; n++;
    }

    // (3) 가까운 좀비 4마리끼리의 쌍 중점 — 한 턴에 2마리를 동시에 잡으려는 시도
    //     (피보나치 배율 보너스를 노리는 수)
    int kp = min(kz, 4);
    for (int i = 0; i < kp; i++) {
        for (int j = i + 1; j < kp; j++) {
            moveX[n] = (s.zx[nearZombie[i]] + s.zx[nearZombie[j]]) / 2;
            moveY[n] = (s.zy[nearZombie[i]] + s.zy[nearZombie[j]]) / 2;
            n++;
        }
    }

    // (4) 가장 가까운 사람 최대 5명을 향해 직진 — 구조하러 가는 수
    int kh = min(s.hn, 5);
    for (int i = 0; i < s.hn; i++) dist[i] = {d2(s.ax, s.ay, s.hx[i], s.hy[i]), i};
    partial_sort(dist, dist + kh, dist + s.hn);
    for (int i = 0; i < kh; i++) {
        int j = dist[i].second;
        moveX[n] = s.hx[j]; moveY[n] = s.hy[j]; n++;
    }

    return n;
}

static double evaluate(const State &s) {
    if (s.status == -1) return -1e18;      // 전멸은 절대 선택하면 안 되므로 최하값
    double value = (double)s.score;
    if (s.status == 1) return value;       // 이미 승리했다면 점수가 확정이다

    int doomed = 0;
    for (int i = 0; i < s.hn; i++) {
        long long bestZombieDist2 = LLONG_MAX;
        int bestZombie = -1;
        for (int j = 0; j < s.zn; j++) {
            long long dist = d2(s.hx[i], s.hy[i], s.zx[j], s.zy[j]);
            if (dist < bestZombieDist2) { bestZombieDist2 = dist; bestZombie = j; }
        }
        if (bestZombie < 0) continue;

        double turnsUntilZombieArrives = sqrt((double)bestZombieDist2) / ZOM_SPEED;
        double ashToZombie = sqrt((double)d2(s.ax, s.ay, s.zx[bestZombie], s.zy[bestZombie]));
        double turnsUntilAshCanShoot = max(0.0, ashToZombie - 2000.0) / ASH_SPEED;

        if (turnsUntilAshCanShoot > turnsUntilZombieArrives + 1.0) doomed++;
    }
    double expectedSurvivors = max(0, s.hn - doomed);

    value += 10.0 * expectedSurvivors * expectedSurvivors * s.zn;

    // (2) 동점 방지용 아주 작은 보정항: 가장 가까운 좀비 쪽에 붙어 있을수록 조금 더 유리하게
    long long nearest = LLONG_MAX;
    for (int i = 0; i < s.zn; i++) nearest = min(nearest, d2(s.ax, s.ay, s.zx[i], s.zy[i]));
    value -= 0.0005 * sqrt((double)nearest);

    return value;
}

struct Node {
    State s;
    int fx, fy;
    double val;
};

static Clock::time_point turnStart;
static inline double elapsedMs() {
    return chrono::duration<double, milli>(Clock::now() - turnStart).count();
}

// root 상태에서 limitMs 안에 가장 좋아 보이는 첫 수를 찾아 (outX, outY)에 담는다.
static void chooseMove(const State &root, double limitMs, int &outX, int &outY) {
    const int BEAM_WIDTH = 50;
    const int MAX_PER_CELL = 4;         // 같은 격자칸에서 살아남을 수 있는 최대 후보 수

    vector<Node> layer, nextLayer;
    layer.push_back({root, root.ax, root.ay, evaluate(root)});

    double bestVal = -1e18;
    outX = root.ax; outY = root.ay;     // 최악의 경우를 대비한 기본값: 제자리

    int moveX[32], moveY[32];

    for (int depth = 1; depth <= 100; depth++) {
        nextLayer.clear();

        for (size_t p = 0; p < layer.size(); p++) {
            if (layer[p].s.status != 0) continue;    // 이미 끝난 상태는 더 펼치지 않는다

            int m = genMoves(layer[p].s, moveX, moveY);
            for (int i = 0; i < m; i++) {
                Node child;
                child.s = layer[p].s;
                simulate(child.s, moveX[i], moveY[i]);
                child.fx = (depth == 1) ? moveX[i] : layer[p].fx;   // 첫 수는 depth 1에서만 갱신
                child.fy = (depth == 1) ? moveY[i] : layer[p].fy;
                child.val = evaluate(child.s);

                if (child.val > bestVal) {
                    bestVal = child.val;
                    outX = child.fx; outY = child.fy;
                }
                if (child.s.status == 0) nextLayer.push_back(child);
            }

            // anytime: 시간이 끊기는 순간 바로 지금까지의 최선을 반환한다.
            // (child를 만들 때마다 bestVal을 갱신해 두었으므로 언제 끊겨도 답이 있다)
            if (elapsedMs() > limitMs) return;
        }

        if (nextLayer.empty()) break;    // 더 뻗어나갈 상태가 없으면 종료

        if ((int)nextLayer.size() > BEAM_WIDTH) {
            sort(nextLayer.begin(), nextLayer.end(),
                 [](const Node &a, const Node &b) { return a.val > b.val; });

            int cellCount[8 * 5] = {0};
            vector<Node> picked;
            for (size_t i = 0; i < nextLayer.size() && (int)picked.size() < BEAM_WIDTH; i++) {
                int cx = min(7, nextLayer[i].s.ax / 2000);
                int cy = min(4, nextLayer[i].s.ay / 2000);
                int cell = cx * 5 + cy;
                if (cellCount[cell] < MAX_PER_CELL) {
                    cellCount[cell]++;
                    picked.push_back(nextLayer[i]);
                }
            }
            // 격자 제한 때문에 모자라면 점수 순으로 남은 후보를 채운다.
            for (size_t i = 0; i < nextLayer.size() && (int)picked.size() < BEAM_WIDTH; i++) {
                picked.push_back(nextLayer[i]);
            }
            nextLayer.swap(picked);
        }

        layer.swap(nextLayer);
    }
}

int main() {
    initFib();

    long long gameScore = 0;
    bool firstTurn = true;

    // 지난 턴에 내가 계산해 둔 "다음 상태" 예측값. 이번 턴 입력과 비교해서
    // 엔진이 게임 규칙과 정확히 일치하는지 매 턴 검증한다 (디버깅용, 점수에는 영향 없음).
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
        for (int i = 0; i < s.hn; i++) {
            int id;
            cin >> id >> s.hx[i] >> s.hy[i];
        }

        cin >> s.zn;
        for (int i = 0; i < s.zn; i++) {
            int id, curX, curY;
            // 현재 좌표(curX, curY)는 버리고 "phase 1 이후" 좌표를 상태에 저장한다.
            // 이번 턴의 사격(phase 3)은 phase 1 다음에 일어나므로 이게 맞는 기준이다.
            cin >> id >> curX >> curY >> s.zx[i] >> s.zy[i];
        }

        // --- 엔진 검증: 지난 턴 예측과 이번 턴 실제 입력이 일치하는지 확인 ---
        if (hasPrediction) {
            bool match = (predicted.ax == s.ax && predicted.ay == s.ay &&
                          predicted.hn == s.hn && predicted.zn == s.zn);
            for (int i = 0; match && i < s.zn; i++) {
                if (predicted.zx[i] != s.zx[i] || predicted.zy[i] != s.zy[i]) match = false;
            }
            if (!match) cerr << "ENGINE MISMATCH!" << endl;
        }

        // 첫 턴은 1000ms, 이후는 100ms가 제한 시간이므로 안전 마진을 두고 사용한다.
        int tx, ty;
        chooseMove(s, firstTurn ? 900.0 : 85.0, tx, ty);
        cout << tx << " " << ty << " BRAINS" << endl;

        // 방금 낸 수를 적용한 결과를 다음 턴 검증 및 점수 추적에 쓴다.
        predicted = s;
        simulate(predicted, tx, ty);
        gameScore = predicted.score;
        hasPrediction = (predicted.status == 0);
        firstTurn = false;
    }
    return 0;
}
