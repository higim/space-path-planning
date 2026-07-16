#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <optional>
#include "core/point.h"

// Records a search run and serializes it to the JSON trace format (see
// docs/trace-format.md). A trace holds an initial search (steps/path) plus an
// optional series of replans: each is a fresh search from the robot's current
// cell after some grid cells changed cost. A* Part 1 uses only the initial
// phase; D*/D* Lite in Part 2 add replans. The schema stays backward compatible.
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

    // A grid cell whose cost changed just before a replan (a revealed obstacle).
    struct Change {
        Point p;
        int cost;
    };

    SearchTrace(int width, int height,
                std::string algorithm = "astar",
                std::string heuristic = "manhattan",
                int connectivity = 4)
        : w(width), h(height),
          algo(std::move(algorithm)), heur(std::move(heuristic)), conn(connectivity),
          costs(static_cast<std::size_t>(height),
                std::vector<int>(static_cast<std::size_t>(width), 1)) {}

    // Grid setup (records the INITIAL cost map; later changes live in replans).
    void set_cost(const Point& p, int cost) { costs[p.row][p.col] = cost; }
    void set_start(const Point& p) { start = p; }
    void set_goal(const Point& p)  { goal = p; }

    // Open a new replan phase: the robot is now at `origin`, and `changes` are the
    // cells whose cost just changed. Subsequent record_step/set_path calls target
    // this phase until the next begin_replan.
    void begin_replan(const Point& origin, std::vector<Change> changes) {
        replans.push_back({origin, std::move(changes), {}, {}});
    }

    // Called once per expanded node in expansion order (routed to the current
    // phase: the initial search, or the most recent replan).
    void record_step(const Point& expanded, int g, int f,
                     std::vector<FrontierEntry> frontier) {
        current_steps().push_back({expanded, g, f, std::move(frontier)});
    }

    // The path found in the current phase. Empty if unreachable.
    void set_path(std::vector<Point> p) { current_path() = std::move(p); }

    // Serialize to a .json file.
    void save(const std::string& filepath) const;

private:
    struct Replan {
        Point origin;
        std::vector<Change> changes;
        std::vector<Step> steps;
        std::vector<Point> path;
    };

    std::vector<Step>&  current_steps() { return replans.empty() ? steps : replans.back().steps; }
    std::vector<Point>& current_path()  { return replans.empty() ? path  : replans.back().path;  }

    int w, h;
    std::string algo, heur;
    int conn;
    std::vector<std::vector<int>> costs;
    std::optional<Point> start, goal;
    std::vector<Step> steps;        // initial-phase expansions
    std::vector<Point> path;        // initial-phase path
    std::vector<Replan> replans;    // Part 2+: one per map change
};