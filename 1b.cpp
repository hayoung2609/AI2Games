#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <chrono>

using namespace std;
using namespace std::chrono;

const int W = 19;
const int H = 10;

struct Robot {
    int x = 0, y = 0;
    char dir;
    bool isAlive = 1;
};

struct Game {
    string grid[10];
    vector<Robot> robots;
};

int dirIndex(char d) {
    if (d == 'U') return 0;
    if (d == 'D') return 1;
    if (d == 'L') return 2;
    if (d == 'R') return 3;
}

int DX[4] = {0, 0, -1, 1};
int DY[4] = {-1, 1, 0, 0};
char DIRS[4] = {'U', 'D', 'L', 'R'};

bool isArrow(char c) {
    return c == 'U' || c == 'D' || c == 'L' || c == 'R';
}

int simulate(Game game) {
    int score = 0;
    bool visited[10][10][19][4] = {false};

    for (int i = 0; i < game.robots.size(); i++) {
        Robot &r = game.robots[i];
        char tile = game.grid[r.y][r.x];
        if (isArrow(tile)) r.dir = tile;
        visited[i][r.y][r.x][dirIndex(r.dir)] = 1;
    }

    while (1) {
        int count = 0;
        for (int i = 0; i < game.robots.size(); i++) {
            if (game.robots[i].isAlive) count++;
        }
        if (count == 0) break;
        score += count;

        for (int i = 0; i < game.robots.size(); i++) {
            Robot &r = game.robots[i];
            if (!r.isAlive) continue;

            int d = dirIndex(r.dir);
            r.x = (r.x + DX[d] + W) % W;
            r.y = (r.y + DY[d] + H) % H;

            char tile = game.grid[r.y][r.x];
            if (tile == '#') {
                r.isAlive = 0;
                continue;
            }
            if (isArrow(tile)) r.dir = tile;

            int nd = dirIndex(r.dir);
            if (visited[i][r.y][r.x][nd]) r.isAlive = 0;
            else visited[i][r.y][r.x][nd] = 1;
        }
    }
    return score;
}

bool isPlatform(Game &game, int x, int y, int d) {
    int nx = (x + DX[d] + W) % W;
    int ny = (y + DY[d] + H) % H;
    return game.grid[ny][nx] != '#';
}

double randomDouble() {
    return (double)rand() / RAND_MAX;
}

int main()
{
    Game game;
    for (int i = 0; i < 10; i++) {
        getline(cin, game.grid[i]);
    }
    int robot_count;
    cin >> robot_count; cin.ignore();
    for (int i = 0; i < robot_count; i++) {
        Robot robot;
        cin >> robot.x >> robot.y >> robot.dir; cin.ignore();
        game.robots.push_back(robot);
    }

    auto start_time = high_resolution_clock::now();

    string original[10];
    for (int i = 0; i < 10; i++) original[i] = game.grid[i];

    // Search space reduction
    vector<pair<int, int>> cells;
    vector<vector<char>> options;

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            if (game.grid[y][x] != '.') continue;   // 빈 플랫폼에만 놓을 수 있음

            vector<char> dirsOk;
            for (int d = 0; d < 4; d++) {
                if (isPlatform(game, x, y, d)) dirsOk.push_back(DIRS[d]);
            }

            if (dirsOk.size() == 0) continue;

            bool robotStart = false;
            for (int i = 0; i < game.robots.size(); i++) {
                if (game.robots[i].x == x && game.robots[i].y == y) robotStart = true;
            }

            if (dirsOk.size() == 1 && !robotStart) {
                game.grid[y][x] = dirsOk[0];
                continue;
            }

            bool upDown = isPlatform(game, x, y, 0) && isPlatform(game, x, y, 1);
            bool leftRight = isPlatform(game, x, y, 2) && isPlatform(game, x, y, 3);
            if (dirsOk.size() == 2 && (upDown || leftRight)) continue;   // 직선 복도

            vector<char> opt = dirsOk;
            opt.push_back('.');
            cells.push_back({x, y});
            options.push_back(opt);
        }
    }

    // Simulated Annealing (time based)
    const double TEMP_START = 10.0;
    const double TEMP_END = 0.5;
    const double TIME_LIMIT = 900.0;   // ms

    Game current = game;
    int currentScore = simulate(current);
    Game best = current;
    int bestScore = currentScore;

    while (!cells.empty()) {
        double elapsed = duration_cast<microseconds>(high_resolution_clock::now() - start_time).count() / 1000.0;
        if (elapsed > TIME_LIMIT) break;
        double temp = TEMP_START * pow(TEMP_END / TEMP_START, elapsed / TIME_LIMIT);

        for (int it = 0; it < 100; it++) {
            // 1) 변이: 칸 하나를 골라 값을 바꾼다
            int k = rand() % cells.size();
            int x = cells[k].first;
            int y = cells[k].second;
            char oldValue = current.grid[y][x];
            char newValue = options[k][rand() % options[k].size()];
            if (newValue == oldValue) continue;

            current.grid[y][x] = newValue;

            // 2) 평가
            int newScore = simulate(current);
            int diff = newScore - currentScore;

            // 3) 받아들일지 결정
            if (diff >= 0 || randomDouble() < exp(diff / temp)) {
                currentScore = newScore;
                if (currentScore > bestScore) {
                    bestScore = currentScore;
                    best = current;
                }
            } else {
                current.grid[y][x] = oldValue;   // 되돌리기
            }
        }
    }

    string answer = "";
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            if (original[y][x] == '.' && isArrow(best.grid[y][x])) {
                answer += to_string(x) + " " + to_string(y) + " " + best.grid[y][x] + " ";
            }
        }
    }

    cout << answer << endl;

    cerr << "best score = " << bestScore << endl;
}
