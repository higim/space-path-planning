// astar_demo.cpp
// ---------------------------------------------------------------------------
// Article 1 demo: build a Mars-like grid, run A*, record the search, and write
// a JSON trace (see TRACE_FORMAT.md) that the Python/HTML visualizers render.
//
// Run from the repo root so the output path resolves:
//     ./build/apps/astar_demo traces/run.json
// then:
//     python tools/visualizer/render_trace.py traces/run.json --hero hero.png --anim search.gif
//
// This file is the glue between four pieces: the Grid map, the Manhattan
// heuristic, the AStar planner, and the SearchTrace recorder. A* stays pure --
// the recording happens through an optional callback the planner invokes on
// each expansion (see the note at the bottom for the one-line hook in AStar).
// ---------------------------------------------------------------------------
#include <iostream>
#include <string>
#include <vector>
#include <random>
#include <cmath>

#include "core/point.h"
#include "core/heuristics.h"
#include "maps/grid.h"
#include "planners/a_star.h"
#include "core/search_trace.h"

namespace {

constexpr int OBSTACLE = 1'000'000;  // impassable cell cost (matches trace format)

// Build a Mars-ish cost grid: cheap regolith, a few rocky blobs, scattered
// boulders. Deterministic given a seed so the article's figures are repeatable.
void build_mars_terrain(Grid& grid, int W, int H, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> rowd(0, H - 1), cold(0, W - 1);
    std::uniform_int_distribution<int> radd(2, 4);

    auto blob = [&](int cr, int cc, int radius) {
        for (int r = std::max(0, cr - radius); r <= std::min(H - 1, cr + radius); ++r)
            for (int c = std::max(0, cc - radius); c <= std::min(W - 1, cc + radius); ++c) {
                double d = std::hypot(r - cr, c - cc);
                if (d <= radius) {
                    Point p{r, c};
                    if (d <= radius * 0.45) grid.update_cell(p, OBSTACLE);
                    else grid.update_cell(p, std::max(grid.get_cost(p),
                                                      static_cast<int>(2 + (radius - d) / radius * 6)));
                }
            }
    };

    for (int i = 0; i < 6; ++i) blob(rowd(rng), cold(rng), radd(rng));   // rocky outcrops
    for (int i = 0; i < 18; ++i) grid.update_cell({rowd(rng), cold(rng)}, OBSTACLE); // boulders
}

// Copy the grid's costs into the trace's grid block so the visualizer can draw
// the terrain. (Kept separate from the planner: the trace is a pure record.)
void seed_trace_grid(SearchTrace& trace, const Grid& grid, int W, int H) {
    for (int r = 0; r < H; ++r)
        for (int c = 0; c < W; ++c)
            trace.set_cost({r, c}, grid.get_cost({r, c}));
}

}  // namespace

int main(int argc, char** argv) {
    const std::string out_path = (argc > 1) ? argv[1] : "traces/run.json";

    // --- grid setup -------------------------------------------------------
    const int W = 34, H = 24;
    Grid grid(H, W);  // Grid(rows, cols); H = rows/height, W = cols/width
    build_mars_terrain(grid, W, H, /*seed=*/7);

    const Point start{H - 3, 2};
    const Point goal{2, W - 3};
    // Guarantee start/goal are traversable even if a blob landed on them.
    grid.update_cell(start, 1);
    grid.update_cell(goal, 1);

    // --- trace setup ------------------------------------------------------
    SearchTrace trace(W, H, "astar", "manhattan", /*connectivity=*/4);
    seed_trace_grid(trace, grid, W, H);
    trace.set_start(start);
    trace.set_goal(goal);

    // --- heuristic: Manhattan to goal, bound via a lambda -----------------
    auto h = [goal](const Point& p) { return heuristics::manhattan(p, goal); };

    // --- run A*, recording each expansion ---------------------------------
    // The planner calls this on every node it expands, handing us the node,
    // its g and f, and a snapshot of the open set after the expansion.
    auto on_expand = [&trace](const Point& expanded, int g, int f,
                              const std::vector<std::pair<Point,int>>& frontier) {
        std::vector<SearchTrace::FrontierEntry> fe;
        fe.reserve(frontier.size());
        for (const auto& [p, pf] : frontier) fe.push_back({p, pf});
        trace.record_step(expanded, g, f, std::move(fe));
    };

    AStar<Point, Grid> planner(grid, goal, h);
    std::vector<Point> path = planner.plan(start, on_expand);

    trace.set_path(path);

    // --- save -------------------------------------------------------------
    trace.save(out_path);

    std::cout << "A* done: expanded nodes recorded, path length " << path.size()
              << "\nTrace written to " << out_path << "\n"
              << "Render it with:\n"
              << "  python tools/visualizer/render_trace.py " << out_path
              << " --hero hero.png --anim search.gif\n";
    return 0;
}