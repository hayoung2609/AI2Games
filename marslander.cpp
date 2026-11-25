#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <bits/stdc++.h>

using namespace std;
using Clock = chrono::steady_clock;

const double G = 3.711;
const int H = 150;              // 염색체 길이 (계획하는 턴 수)
const int POP = 100;             // 개체 수
const int ELITE = 10;            // 그대로 다음 세대로 가는 상위 개체 수
const int TOURNAMENT = 3;
const double MUT_RATE = 0.04;   // 유전자 하나가 바뀔 확률
const double TIME_LIMIT = 80.0;
const int MAX_STEPS = 600;

struct Lander { double x, y, vx, vy; int fuel, rot, pow; };
struct Gene { int dr, dp; };                 // 각도 변화 -15..15, 추력 변화 -1..1
struct Individual { Gene g[H]; double fit; };

int N;
double sx[64], sy[64], cumLen[64];
int landSeg;
double landL, landR, landY;
double SIN[181], COS[181];

mt19937 rng(2026);
int randInt(int a, int b) { return uniform_int_distribution<int>(a, b)(rng); }
double rand01() { return uniform_real_distribution<double>(0.0, 1.0)(rng); }

// forward model
void step(Lander &s, int targetRot, int targetPow) {
    targetRot = max(-90, min(90, targetRot));
    targetPow = max(0, min(4, targetPow));
    s.rot = max(s.rot - 15, min(s.rot + 15, targetRot));
    s.pow = max(s.pow - 1, min(s.pow + 1, targetPow));
    if (s.fuel < s.pow) s.pow = s.fuel;
    s.fuel -= s.pow;

    double ax = -s.pow * SIN[s.rot + 90];
    double ay =  s.pow * COS[s.rot + 90] - G;
    s.x += s.vx + 0.5 * ax;
    s.y += s.vy + 0.5 * ay;
    s.vx += ax;
    s.vy += ay;
}

// 이동 경로 (x0,y0)->(x1,y1) 와 지면의 교차 검사
bool hitGround(double x0, double y0, double x1, double y1, int &seg, double &hx, double &hy) {
    double dx = x1 - x0, dy = y1 - y0;
    for (int i = 0; i + 1 < N; i++) {
        double ex = sx[i + 1] - sx[i], ey = sy[i + 1] - sy[i];
        double den = dx * ey - dy * ex;
        if (den == 0) continue;
        double t = ((sx[i] - x0) * ey - (sy[i] - y0) * ex) / den;
        double u = ((sx[i] - x0) * dy - (sy[i] - y0) * dx) / den;
        if (t >= 0 && t <= 1 && u >= 0 && u <= 1) {
            seg = i; hx = x0 + t * dx; hy = y0 + t * dy;
            return true;
        }
    }
    return false;
}

// 충돌 지점에서 착륙장까지 지면을 따라 잰 거리
double surfaceDist(int seg, double hx, double hy) {
    double pos = cumLen[seg] + hypot(hx - sx[seg], hy - sy[seg]);
    if (pos < cumLen[landSeg]) return cumLen[landSeg] - pos;
    if (pos > cumLen[landSeg + 1]) return pos - cumLen[landSeg + 1];
    return 0;
}

// evaluation
double finalScore(const Lander &s, int seg, double hx, double hy) {
    if (seg == landSeg) {
        double ex = max(0.0, fabs(s.vx) - 18), ey = max(0.0, fabs(s.vy) - 38);
        if (s.rot == 0 && ex == 0 && ey == 0) return 20000 + s.fuel;
        return 10000 - 10 * ex - 10 * ey - 10 * abs(s.rot);
    }
    double speed = max(0.0, fabs(s.vx) - 20) + max(0.0, fabs(s.vy) - 40);
    return -surfaceDist(seg, hx, hy) - speed;
}

void decode(const Lander &s, const Gene &g, int &rot, int &pow) {
    rot = max(-90, min(90, s.rot + g.dr));
    pow = max(0, min(4, s.pow + g.dp));
    // 착륙장 바로 위에서 낮게 내려오면 각도를 0으로 (탐색 공간 축소)
    if (s.x > landL && s.x < landR && s.y - landY < 80 + 3 * fabs(s.vy)) rot = 0;
}

double evaluate(const Individual &ind, Lander s) {
    for (int t = 0; t < MAX_STEPS; t++) {
        Gene g = (t < H) ? ind.g[t] : Gene{0, 0};
        int rot, pow;
        decode(s, g, rot, pow);
        double px = s.x, py = s.y;
        step(s, rot, pow);
        int seg; double hx, hy;
        if (hitGround(px, py, s.x, s.y, seg, hx, hy)) return finalScore(s, seg, hx, hy);
        if (s.x < 0 || s.x > 6999 || s.y < 0 || s.y > 2999)
            return -50000 - hypot(s.x - (landL + landR) / 2, s.y - landY);
    }
    return -100000;
}

// RHEA
Individual pop[POP], nextPop[POP];

Gene randomGene() { return Gene{randInt(-15, 15), randInt(-1, 1)}; }

const Individual &tournament() {
    int best = randInt(0, POP - 1);
    for (int i = 1; i < TOURNAMENT; i++) {
        int c = randInt(0, POP - 1);
        if (pop[c].fit > pop[best].fit) best = c;
    }
    return pop[best];
}

void sortPop() {
    sort(pop, pop + POP, [](const Individual &a, const Individual &b) { return a.fit > b.fit; });
}

int evolve(const Lander &root, Clock::time_point start, double limit) {
    int generations = 0;
    while (chrono::duration<double, milli>(Clock::now() - start).count() < limit) {
        for (int i = 0; i < ELITE; i++) nextPop[i] = pop[i];
        for (int i = ELITE; i < POP; i++) {
            const Individual &a = tournament();
            const Individual &b = tournament();
            int cut = randInt(0, H - 1);
            for (int k = 0; k < H; k++) {
                nextPop[i].g[k] = (k < cut) ? a.g[k] : b.g[k];
                if (rand01() < MUT_RATE) nextPop[i].g[k] = randomGene();
            }
            nextPop[i].fit = evaluate(nextPop[i], root);
        }
        for (int i = 0; i < POP; i++) pop[i] = nextPop[i];
        sortPop();
        generations++;
    }
    return generations;
}

// 지난 턴의 계획을 한 칸 당겨서 재사용
void shiftPopulation(const Lander &root) {
    for (int i = 0; i < POP; i++) {
        for (int k = 0; k + 1 < H; k++) pop[i].g[k] = pop[i].g[k + 1];
        pop[i].g[H - 1] = Gene{0, 0};
        pop[i].fit = evaluate(pop[i], root);
    }
    sortPop();
}

int main() {
    for (int a = -90; a <= 90; a++) {
        SIN[a + 90] = sin(a * M_PI / 180.0);
        COS[a + 90] = cos(a * M_PI / 180.0);
    }
    cin >> N;
    for (int i = 0; i < N; i++) cin >> sx[i] >> sy[i];
    cumLen[0] = 0;
    for (int i = 0; i + 1 < N; i++) {
        cumLen[i + 1] = cumLen[i] + hypot(sx[i + 1] - sx[i], sy[i + 1] - sy[i]);
        if (sy[i] == sy[i + 1] && sx[i + 1] - sx[i] >= 1000) {
            landSeg = i; landL = sx[i]; landR = sx[i + 1]; landY = sy[i];
        }
    }

    Lander predicted;
    bool firstTurn = true;
    while (true) {
        Lander in;
        if (!(cin >> in.x >> in.y >> in.vx >> in.vy >> in.fuel >> in.rot >> in.pow)) break;
        auto start = Clock::now();

        // 엔진 검증: 입력(반올림된 값)과 내 시뮬레이션 비교
        Lander s = in;
        if (!firstTurn) {
            bool ok = fabs(predicted.x - in.x) <= 1 && fabs(predicted.y - in.y) <= 1 &&
                      fabs(predicted.vx - in.vx) <= 1 && fabs(predicted.vy - in.vy) <= 1 &&
                      predicted.fuel == in.fuel && predicted.rot == in.rot && predicted.pow == in.pow;
            if (ok) s = predicted;
            else cerr << "MISMATCH" << endl;
        }

        if (firstTurn) {
            for (int i = 0; i < POP; i++) {
                for (int k = 0; k < H; k++) pop[i].g[k] = randomGene();
                pop[i].fit = evaluate(pop[i], s);
            }
            sortPop();
        } else {
            shiftPopulation(s);
        }

        int gens = evolve(s, start, TIME_LIMIT);
        cerr << "gens " << gens << " best " << pop[0].fit << endl;

        int rot, pow;
        decode(s, pop[0].g[0], rot, pow);
        cout << rot << " " << pow << endl;

        predicted = s;
        step(predicted, rot, pow);
        firstTurn = false;
    }
}
