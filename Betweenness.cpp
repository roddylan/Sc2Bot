#include <iostream>
#include <vector>
#include <queue>
#include <utility>
#include <unordered_map>
#include <algorithm>
#include "Utilities.h"
#include "Betweenness.h"
#include <stack>
#include <random>
#include <set>
#include <functional>

using namespace std;

// Directions for moving in the grid (up, down, left, right)
vector<sc2::Point2DI> directions = { sc2::Point2DI(0, 1), sc2::Point2DI(0, -1), sc2::Point2DI(-1, 0), sc2::Point2DI(1, 0) };

/**
* @brief Check if a point is inside the grid boundaries
*/
bool isValid(sc2::Point2DI p, sc2::ImageData data) {
    int rows = data.height;
    int cols = data.width;
    return (p.x >= 0 && p.x < rows && p.y >= 0 && p.y < cols && GetDataValueBit(data, p.x, p.y) == 1);
}

/**
* Implementation of compare for Point2DI instances
*/
struct CompareBetweenness {
    bool operator()(const std::pair<sc2::Point2DI, double>& a, const std::pair<sc2::Point2DI, double>& b) {
        return a.second < b.second;  // Max-heap: largest betweenness first
    }
};

// Custom comparator for sc2::Point2DI to be used in std::set
struct PointComparator {
    bool operator()(const sc2::Point2DI& a, const sc2::Point2DI& b) const {
        // Compare x first, then y if x values are equal
        if (a.x != b.x) {
            return a.x < b.x;
        }
        return a.y < b.y;
    }
};

// Function to randomly sample nodes for the Monte Carlo approximation
vector<sc2::Point2DI> SampleNodes(int numSamples, int rows, int cols, sc2::ImageData data) {
    vector<sc2::Point2DI> sampledNodes;
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> disX(0, rows - 1);
    uniform_int_distribution<> disY(0, cols - 1);

    set<sc2::Point2DI, PointComparator> uniqueNodes;  // Using custom comparator

    int count = 0;
    while (sampledNodes.size() < numSamples) {
        sc2::Point2DI node(disX(gen), disY(gen));
        // Ensure the node is not already in the set and is valid
        if (uniqueNodes.find(node) == uniqueNodes.end() && isValid(node, data)) {
            uniqueNodes.insert(node);
            sampledNodes.push_back(node);
        }
		if (count > 10 * numSamples) {
			break;
		}
		count++;
    }
    return sampledNodes;
}

// Brandes' Algorithm For Betweenness Centrality:
// https://snap.stanford.edu/class/cs224w-readings/brandes01centrality.pdf
/**
* @brief does Monte-Carlo Brandes' search for betweenness
* @param data: the bitmap
* @param betweenness: vector to populate
* @param numSamples: number of random samples for Monte Carlo approximation
*/
void MonteCarloBrandesBetweenness(sc2::ImageData data, vector<vector<double>>& betweenness, int numSamples) {
    int rows = data.height;
    int cols = data.width;

    // Randomly sample nodes for BFS runs
    vector<sc2::Point2DI> sampledNodes = SampleNodes(numSamples, rows, cols, data);

    // Loop over the sampled nodes
    for (const auto& src : sampledNodes) {

        // Reset for each source node
        vector<vector<int>> dist(rows, vector<int>(cols, -1));
        vector<vector<double>> numPaths(rows, vector<double>(cols, 0));
        vector<vector<double>> dependency(rows, vector<double>(cols, 0));
        vector<vector<vector<sc2::Point2DI>>> predecessors(rows, vector<vector<sc2::Point2DI>>(cols));

        // BFS initialization
        queue<sc2::Point2DI> q;
        stack<sc2::Point2DI> stackOrder;
        q.push(src);
        dist[src.x][src.y] = 0;
        numPaths[src.x][src.y] = 1;

        // BFS to calculate shortest paths
        while (!q.empty()) {
            sc2::Point2DI u = q.front();
            q.pop();
            stackOrder.push(u);

            // Process each neighbour of u
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

            if (u != src) {
                betweenness[u.x][u.y] += dependency[u.x][u.y];
            }
        }
    }
}

/**
* @brief Calculates betweenness centrality for a grid using Monte Carlo method
* @param data: the bitmap
* @return tuple containing the point with highest betweenness value and its betweenness value
*/
tuple<sc2::Point2DI, double> CalculateBetweenness(sc2::ImageData data) {
    int numSamples = 15;  // Number of samples for Monte Carlo approximation
    int rows = data.height;
    int cols = data.width;
    vector<vector<double>> betweenness(rows, vector<double>(cols, 0));

    // Calculate Betweenness using Brandes' Algorithm
    MonteCarloBrandesBetweenness(data, betweenness, numSamples);

    // Use a priority queue to find the top betweenness points efficiently
    std::priority_queue<std::pair<sc2::Point2DI, double>, std::vector<std::pair<sc2::Point2DI, double>>, CompareBetweenness> topPoints;

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (betweenness[i][j] > 0) {
                topPoints.push({ {i, j}, betweenness[i][j] });
            }
        }
    }

    // Collect top betweenness points in a vector
    vector<pair<sc2::Point2DI, double>> betweennessList;
    while (!topPoints.empty()) {
        betweennessList.push_back(topPoints.top());
        topPoints.pop();
    }
    std::reverse(betweennessList.begin(), betweennessList.end());  // to sort in descending order

    // Return the top point with the highest betweenness centrality
    return std::make_tuple(sc2::Point2DI(betweennessList[0].first.x, betweennessList[0].first.y), betweennessList[0].second);
}


/**
* @brief Collects multiple betweenness points in a vector
* @return vector of best betweenness points
*/
vector<pair<sc2::Point2DI, double>> GetBetweennessList(sc2::ImageData data) {
    int numSamples = 15;  // Number of samples for Monte Carlo approximation
    int rows = data.height;
    int cols = data.width;
    vector<vector<double>> betweenness(rows, vector<double>(cols, 0));

    // Calculate Betweenness using Brandes' Algorithm
    MonteCarloBrandesBetweenness(data, betweenness, numSamples);

    // Use a priority queue to find the top betweenness points efficiently
    std::priority_queue<std::pair<sc2::Point2DI, double>, std::vector<std::pair<sc2::Point2DI, double>>, CompareBetweenness> topPoints;

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (betweenness[i][j] > 0) {
                topPoints.push({ {i, j}, betweenness[i][j] });
            }
        }
    }

    // Collect top betweenness points in a vector
    vector<pair<sc2::Point2DI, double>> betweennessList;
    while (!topPoints.empty()) {
        betweennessList.push_back(topPoints.top());
        topPoints.pop();
    }
    std::reverse(betweennessList.begin(), betweennessList.end());  // to sort in descending order

    return betweennessList;
}