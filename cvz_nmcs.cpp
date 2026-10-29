// The score is 133140.

#include <bits/stdc++.h>
using namespace std;
using Clock = chrono::steady_clock;

static const int       MAXN      = 100;
static const double    ASH_SPEED = 1000.0;
static const double    ZOM_SPEED = 400.0;
static const long long RANGE2    = 2000LL * 2000LL;
static const int       MAX_PLY   = 150;   // 플레이아웃 최대 턴 수 (무한 도주 방지용 안전장치)
static const int       NMCS_LEVEL = 1;    // 중첩 깊이 (0 = 순수 랜덤, 1 = 1단계 중첩)
static long long fibSum[MAXN + 1];

static void initFib() {
    long long a = 1, b = 2, sum = 0;
    fibSum[0] = 0;
    for (int k = 1; k <= MAXN; k++) {
        sum += a;
        fibSum[k] = sum;
        if (b < (long long)1e15) {
            long long next = a + b;
            a = b; b = next;
        }
    }
}

struct State {
    int ax, ay;
    int hn; int hx[MAXN], hy[MAXN];
    int zn; int zx[MAXN], zy[MAXN];      // 항상 "이번 턴 phase 1 이후" 좌표
    long long score;
    int status;                          // 0 진행 중, 1 승리, -1 패배(전멸)
};

static inline long long d2(int x1, int y1, int x2, int y2) {
    long long dx = x1 - x2, dy = y1 - y2;
    return dx * dx + dy * dy;
}

// (x,y)를 목표 (tx,ty) 방향으로 최대 speed만큼 이동 (도착 시 정확히 착지, 아니면 내림)
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

// [phase 1] 모든 좀비가 "가장 가까운 사람(Ash 포함)" 쪽으로 400만큼 이동
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

// 한 턴 전체 진행: phase 2(Ash 이동) -> 3(사격) -> 4(식사) -> 다음 턴 phase 1
static void simulate(State &s, int tx, int ty) {
    moveTowards(s.ax, s.ay, tx, ty, ASH_SPEED);

    int killed = 0, w = 0;
    int survivorsBefore = s.hn;
    for (int i = 0; i < s.zn; i++) {
        if (d2(s.ax, s.ay, s.zx[i], s.zy[i]) <= RANGE2) {
            killed++;
        } else {
            s.zx[w] = s.zx[i]; s.zy[w] = s.zy[i]; w++;
        }
    }
    s.zn = w;
    if (killed > 0) {
        s.score += 10LL * survivorsBefore * survivorsBefore * fibSum[killed];
    }

    w = 0;
    for (int i = 0; i < s.hn; i++) {
        bool eaten = false;
        for (int j = 0; j < s.zn; j++) {
            if (s.zx[j] == s.hx[i] && s.zy[j] == s.hy[i]) { eaten = true; break; }
        }
        if (!eaten) { s.hx[w] = s.hx[i]; s.hy[w] = s.hy[i]; w++; }
    }
    s.hn = w;

    if (s.hn == 0) { s.status = -1; s.score = 0; return; }
    if (s.zn == 0) { s.status =  1; return; }
    moveZombies(s);
}

static int genMoves(const State &s, int *moveX, int *moveY) {
    static pair<long long, int> dist[MAXN];
    int n = 0;

    moveX[n] = s.ax; moveY[n] = s.ay; n++;

    int kz = min(s.zn, 10);
    for (int i = 0; i < s.zn; i++) dist[i] = {d2(s.ax, s.ay, s.zx[i], s.zy[i]), i};
    partial_sort(dist, dist + kz, dist + s.zn);
    int nearZombie[10];
    for (int i = 0; i < kz; i++) {
        nearZombie[i] = dist[i].second;
        moveX[n] = s.zx[nearZombie[i]]; moveY[n] = s.zy[nearZombie[i]]; n++;
    }

    int kp = min(kz, 4);
    for (int i = 0; i < kp; i++) {
        for (int j = i + 1; j < kp; j++) {
            moveX[n] = (s.zx[nearZombie[i]] + s.zx[nearZombie[j]]) / 2;
            moveY[n] = (s.zy[nearZombie[i]] + s.zy[nearZombie[j]]) / 2;
            n++;
        }
    }

    int kh = min(s.hn, 5);
    for (int i = 0; i < s.hn; i++) dist[i] = {d2(s.ax, s.ay, s.hx[i], s.hy[i]), i};
    partial_sort(dist, dist + kh, dist + s.hn);
    for (int i = 0; i < kh; i++) {
        int j = dist[i].second;
        moveX[n] = s.hx[j]; moveY[n] = s.hy[j]; n++;
    }

    return n;
}

static Clock::time_point turnStart;
static double turnLimitMs;

static inline bool timeUp() {
    return chrono::duration<double, milli>(Clock::now() - turnStart).count() > turnLimitMs;
}

static mt19937 rng(12345);

struct Move { int x, y; };
using Seq = vector<Move>;

static long long playout(State s, Seq &outSeq) {
    outSeq.clear();
    int moveX[32], moveY[32];
    while (s.status == 0 && (int)outSeq.size() < MAX_PLY) {
        int m = genMoves(s, moveX, moveY);
        int pick = (int)(rng() % m);
        simulate(s, moveX[pick], moveY[pick]);
        outSeq.push_back({moveX[pick], moveY[pick]});
    }
    return s.score;    // 전멸로 끝났다면 simulate가 이미 0으로 만들어 둔 값
}

static long long nested(State s, int level, Seq &outSeq) {
    if (level == 0) return playout(s, outSeq);

    Seq played;              // 이 레벨에서 지금까지 확정해서 둔 수들
    Seq bestSeq;              // 지금까지 찾은 최선의 "전체" 수열 (played + 이어지는 수들)
    long long bestScore = -1;

    int moveX[32], moveY[32];

    while (s.status == 0 && (int)played.size() < MAX_PLY) {
        if (timeUp()) break;

        int m = genMoves(s, moveX, moveY);
        for (int i = 0; i < m; i++) {
            if (timeUp()) break;

            State child = s;
            simulate(child, moveX[i], moveY[i]);

            Seq subSeq;
            long long score = nested(child, level - 1, subSeq);

            if (score > bestScore) {
                bestScore = score;
                bestSeq = played;                                   // 이미 확정된 앞부분
                bestSeq.push_back({moveX[i], moveY[i]});             // 지금 시도한 수
                bestSeq.insert(bestSeq.end(), subSeq.begin(), subSeq.end()); // 하위 탐색이 찾은 나머지
            }
        }

        size_t ply = played.size();
        if (bestSeq.size() <= ply) break;      // 시간 초과 등으로 아무 수도 못 찾은 경우

        // 이번 단계에서 가장 좋았던 수를 실제로 두고, 다음 턴으로 넘어간다 (memorization).
        Move chosen = bestSeq[ply];
        simulate(s, chosen.x, chosen.y);
        played.push_back(chosen);
    }

    outSeq = bestSeq;
    return (bestScore < 0) ? s.score : bestScore;   // 탐색을 전혀 못했다면 현재 점수 그대로 반환
}

int main() {
    initFib();

    long long gameScore = 0;
    bool firstTurn = true;

    while (true) {
        int x, y;
        if (!(cin >> x >> y)) break;

        turnStart = Clock::now();
        // 첫 턴은 1000ms, 이후는 100ms가 제한 시간이므로 안전 마진을 두고 사용한다.
        turnLimitMs = firstTurn ? 900.0 : 85.0;

        State root;
        root.ax = x; root.ay = y;
        root.score = gameScore;
        root.status = 0;

        cin >> root.hn;
        for (int i = 0; i < root.hn; i++) {
            int id;
            cin >> id >> root.hx[i] >> root.hy[i];
        }

        cin >> root.zn;
        for (int i = 0; i < root.zn; i++) {
            int id, curX, curY;
            // 현재 좌표는 버리고 "phase 1 이후" 좌표를 사용한다 (cvz_beam.cpp와 동일한 이유)
            cin >> id >> curX >> curY >> root.zx[i] >> root.zy[i];
        }

        // anytime: 시간이 남는 동안 NMCS를 반복 실행하고, 그중 전역 최고 점수의 수열만 기억한다.
        long long bestScore = -1;
        Seq bestSeq;
        while (!timeUp()) {
            Seq seq;
            long long score = nested(root, NMCS_LEVEL, seq);
            if (!seq.empty() && score > bestScore) {
                bestScore = score;
                bestSeq = seq;
            }
        }

        int tx, ty;
        if (!bestSeq.empty()) {
            tx = bestSeq[0].x; ty = bestSeq[0].y;
        } else {
            // 비상용 대비책: 시간이 전혀 없어 한 번도 탐색을 못했다면 가장 가까운 좀비로 직진
            tx = root.ax; ty = root.ay;
            long long best = LLONG_MAX;
            for (int i = 0; i < root.zn; i++) {
                long long dist = d2(root.ax, root.ay, root.zx[i], root.zy[i]);
                if (dist < best) { best = dist; tx = root.zx[i]; ty = root.zy[i]; }
            }
        }
        cout << tx << " " << ty << " NMCS" << endl;

        State predicted = root;
        simulate(predicted, tx, ty);
        gameScore = predicted.score;
        firstTurn = false;
    }
    return 0;
}
