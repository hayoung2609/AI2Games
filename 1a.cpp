#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>

using namespace std;
using namespace std::chrono;

// encoding 
struct Robot {
    int x = 0, y = 0;
    char dir;
    bool isAlive = 1;
};

struct Game {
    string grid[10];
    vector<Robot> robots;
};

int simulate(Game game) {
    int score = 0;
    bool visited[10][10][19][4] = {false};

    while(1) {
        int count = 0;
        for(int i = 0; i < game.robots.size(); i++) {
            if(game.robots[i].isAlive) {
                count++;
            }
        }
        if (count == 0) break;

        score += count;
        for(int i = 0; i < game.robots.size(); i++) {
            if(!game.robots[i].isAlive) continue;
            char dir = game.robots[i].dir;
            if(dir == 'U') {
                game.robots[i].y = (game.robots[i].y - 1 + 10) % 10;
            }
            else if(dir == 'D') {
                game.robots[i].y = (game.robots[i].y + 1 + 10) % 10;
            }
            else if(dir == 'L') {
                game.robots[i].x = (game.robots[i].x - 1 + 19) % 19;
            }
            else if(dir == 'R') {
                game.robots[i].x = (game.robots[i].x + 1 + 19) % 19;
            }
            int nx = game.robots[i].x;
            int ny = game.robots[i].y;
            char tile = game.grid[ny][nx];

            if(tile == '#') {
                game.robots[i].isAlive = 0;
            }
            else if(tile == 'U' || tile == 'D' || tile == 'L' || tile == 'R') {
                game.robots[i].dir = tile;
            }
            
            if(game.robots[i].isAlive) {
                int dir_num = 0;
                char n_dir = game.robots[i].dir;
                if(n_dir == 'U') dir_num = 0;
                else if(n_dir == 'D') dir_num = 1;
                else if(n_dir == 'L') dir_num = 2;
                else if(n_dir == 'R') dir_num = 3;

                if(visited[i][ny][nx][dir_num] == 1) {
                    game.robots[i].isAlive = 0;
                } 
                else {
                    visited[i][ny][nx][dir_num] = 1;
                }
            }
        }
    }

    return score;
}

int main()
{
    Game game;

    for (int i = 0; i < 10; i++) {
        string line;
        getline(cin, game.grid[i]);
    }
    int robot_count;
    cin >> robot_count; cin.ignore();
    for (int i = 0; i < robot_count; i++) {
        Robot robot;
        cin >> robot.x >> robot.y >> robot.dir; cin.ignore();

        game.robots.push_back(robot);
    }

    // Write an action using cout. DON'T FORGET THE "<< endl"
    // To debug: cerr << "Debug messages..." << endl;

    vector<pair<int, int>> empty_cells;
    for (int y = 0; y < 10; y++) {
        for (int x = 0; x < 19; x++) {
            if (game.grid[y][x] == '.') {
                empty_cells.push_back({x, y});
            }
        }
    }
    char dirs[4] = {'U', 'D', 'L', 'R'};
    int best_score = -1;
    string best_action = "";

    auto start_time = high_resolution_clock::now();

    while (1) {
        auto current_time = high_resolution_clock::now();
        int elapsed = duration_cast<milliseconds>(current_time - start_time).count();
        if (elapsed > 900) break;

        Game test_game = game;
        string answer = "";
        
        int num_arrows = rand() % 5 + 1; 
        for (int i = 0; i < num_arrows; i++) {
            if (empty_cells.empty()) break;
            
            int rand_idx = rand() % empty_cells.size();
            int px = empty_cells[rand_idx].first;
            int py = empty_cells[rand_idx].second;
            char pdir = dirs[rand() % 4];

            test_game.grid[py][px] = pdir;
            answer += to_string(px) + " " + to_string(py) + " " + string(1, pdir) + " ";
        }

        int score = simulate(test_game);

        if (score > best_score) {
            best_score = score;
            best_action = answer;
        }
    }

    if (best_action == "") {
        cout << "0 0 U" << endl;
    } else {
        cout << best_action << endl;
    }
}