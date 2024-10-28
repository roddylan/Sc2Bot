#include <iostream>
#include <vector>
#include <queue>
#include <utility>
#include <unordered_map>
#include <algorithm>
#include "Utilities.h"
#include "Betweenness.h"
#include <stack>

using namespace std;

// Directions for moving in the grid (up, down, left, right)
vector<sc2::Point2DI> directions = { sc2::Point2DI(0, 1), sc2::Point2DI(0, -1), sc2::Point2DI(-1, 0), sc2::Point2DI(1, 0) };

// Check if a point is inside the grid boundaries
bool isValid(sc2::Point2DI p, sc2::ImageData data) {
	int rows = data.height;
	int cols = data.width;
    return (p.x >= 0 && p.x < rows && p.y >= 0 && p.y < cols && GetDataValueBit(data, p.x, p.y) == 1);
}

// Brandes' algorithm to calculate betweenness centrality efficiently
void brandesBetweenness(sc2::ImageData data, vector<vector<double>>& betweenness) {
	int rows = data.height;
	int cols = data.width;
    for (int sx = 0; sx < rows; ++sx) {
        for (int sy = 0; sy < cols; ++sy) {
			if (isValid(sc2::Point2DI(sx, sy), data) == false) {
				//std::cout << "Invalid starting point" << std::endl;
				continue;
			}
			//std::cout << "Calculating betweenness for point (" << sx << ", " << sy << ")" << std::endl;
            vector<vector<int>> dist(rows, vector<int>(cols, -1));  // Distance from source
            vector<vector<double>> numPaths(rows, vector<double>(cols, 0));  // Number of shortest paths
            vector<vector<double>> dependency(rows, vector<double>(cols, 0));  // Dependency scores
            vector<vector<vector<sc2::Point2DI>>> predecessors(rows, vector<vector<sc2::Point2DI>>(cols));  // Predecessor list

            // BFS initialization
            queue<sc2::Point2DI> q;
            stack<sc2::Point2DI> stackOrder;
            q.push({ sx, sy });
            dist[sx][sy] = 0;
            numPaths[sx][sy] = 1;

            // BFS to calculate shortest paths
            while (!q.empty()) {
                sc2::Point2DI u = q.front();
                q.pop();
                stackOrder.push(u);

                for (sc2::Point2DI dir : directions) {
                    sc2::Point2DI v = { u.x + dir.x, u.y + dir.y };
                    if (isValid(v, data)) {
                        if (dist[v.x][v.y] == -1) {
                            dist[v.x][v.y] = dist[u.x][u.y] + 1;
                            q.push(v);
                        }
                        if (dist[v.x][v.y] == dist[u.x][u.y] + 1) {
                            numPaths[v.x][v.y] += numPaths[u.x][u.y];
                            predecessors[v.x][v.y].push_back(u);
                        }
                    }
                }
            }

            // Accumulate dependencies
            while (!stackOrder.empty()) {
                sc2::Point2DI u = stackOrder.top();
                stackOrder.pop();

                for (sc2::Point2DI pred : predecessors[u.x][u.y]) {
                    double ratio = numPaths[pred.x][pred.y] / numPaths[u.x][u.y];
                    dependency[pred.x][pred.y] += (1 + dependency[u.x][u.y]) * ratio;
                }

                if (u != sc2::Point2DI(sx, sy)) {
                    betweenness[u.x][u.y] += dependency[u.x][u.y];
                }
            }
        }
    }
}

// Function to calculate betweenness centrality for a grid and sort them
tuple<sc2::Point2DI, double> calculateBetweenness(sc2::ImageData data) {
	int rows = data.height;
	int cols = data.width;
    vector<vector<double>> betweenness(rows, vector<double>(cols, 0));

    // Calculate Betweenness using Brandes' Algorithm
    brandesBetweenness(data, betweenness);

    // Store points and their betweenness centrality in a list
    vector<pair<sc2::Point2DI, double>> betweennessList;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            betweennessList.push_back({ {i, j}, betweenness[i][j] });
        }
    }

    // Sort the list by betweenness centrality in descending order
    sort(betweennessList.begin(), betweennessList.end(),
        [](pair<sc2::Point2DI, double>& a, pair<sc2::Point2DI, double>& b) {
            return a.second > b.second;
        });

    return std::make_tuple(sc2::Point2DI(betweennessList[0].first.x, betweennessList[0].first.y), betweennessList[0].second);
}