#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <optional>
#include "core/point.h"

// Records an A* search run and serializes it to the v1 JSON trace format
class SearchTrace {
public:
    struct FrontierEntry {
        Point p;
        int f;
    };

    struct Step {
        Point expanded;
        int g;
        int f;
        std::vector<FrontierEntry> frontier;
    };

    SearchTrace(int width, int height,
                std::string algorithm = "astar",
                std::string heuristic = "manhattan",
                int connectivity = 4)
        : w(width), h(height),
          algo(std::move(algorithm)), heur(std::move(heuristic)), conn(connectivity),
          costs(static_cast<std::size_t>(height),
                std::vector<int>(static_cast<std::size_t>(width), 1)) {}

    // Grid setup
    void set_cost(const Point& p, int cost) { costs[p.row][p.col] = cost; }
    void set_start(const Point& p) { start = p; }
    void set_goal(const Point& p)  { goal = p; }

    // Called once per expanded node in expansion order.
    void record_step(const Point& expanded, int g, int f,
                     std::vector<FrontierEntry> frontier) {
        steps.push_back({expanded, g, f, std::move(frontier)});
    }

    // Empty if unreachable.
    void set_path(std::vector<Point> p) { path = std::move(p); }

    // Serialize to a .json file in the v1 trace format.
    void save(const std::string& filepath) const;

private:
    int w, h;
    std::string algo, heur;
    int conn;
    std::vector<std::vector<int>> costs;
    std::optional<Point> start, goal;
    std::vector<Step> steps;
    std::vector<Point> path;
};