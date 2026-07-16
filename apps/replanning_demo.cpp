// replanning_demo.cpp
// ---------------------------------------------------------------------------
// Article 2 demo. A rover crosses terrain it only partly knows, discovering
// obstacles within a short sensor horizon as it drives, and replanning each time
// the road ahead is blocked. We run the SAME drive two ways:
//
//   * A*      -- no memory: every replan is a fresh search from the rover's cell.
//   * D* Lite -- reuses its previous search, repairing only what the news changed.
//
// Each run is written to a trace (docs/trace-format.md): an initial search plus
// one replan phase per discovery (origin = rover position, changes = the newly
// revealed obstacle cells, then that phase's search steps and new path). The
// visualizer replays the whole drive.
//
// We also sweep several map sizes and write scaling.json: D* Lite's reuse
// advantage over A* compounds as the map (and so each from-scratch search) grows.
//
//   ./build/replanning_demo
//     -> traces/astar_drive.json, traces/dlite_drive.json, traces/scaling.json
// ---------------------------------------------------------------------------
#include <cstdint>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "core/point.h"
#include "core/heuristics.h"
#include "maps/grid.h"
#include "planners/a_star.h"
#include "planners/d_lite.h"
#include "core/search_trace.h"

namespace {

constexpr int OBSTACLE = 1'000'000;
constexpr int SENSE = 4;   // rover reveals obstacles within this many cells

using ObSet = std::set<std::pair<int, int>>;

auto h_to = [](Point t) {
    return [t](const Point& p) { return heuristics::manhattan(p, t); };
};

// A small deterministic PRNG (no <random> seeding differences across builds).
struct Rng {
    std::uint32_t s;
    std::uint32_t next() { s = s * 1103515245u + 12345u; return (s >> 16) & 0x7fff; }
};

// The TRUE obstacle field the rover doesn't know until its sensors reach it:
// clusters of impassable rock, kept clear of the start/goal corridor endpoints.
ObSet make_obstacles(int W, int H, int count, std::uint32_t seed) {
    Rng rng{seed};
    ObSet obs;
    auto blob = [&](int cr, int cc) {
        for (int dr = -1; dr <= 1; ++dr)
            for (int dc = -1; dc <= 1; ++dc) {
                if ((dr && dc) && (rng.next() & 1)) continue;  // ragged edges
                int r = cr + dr, c = cc + dc;
                if (r < 0 || r >= H || c < 0 || c >= W) continue;
                if (r == H / 2 && c < 6) continue;             // keep the launch clear
                obs.insert({r, c});
            }
    };
    for (int i = 0; i < count; ++i)
        blob(rng.next() % H, 6 + rng.next() % (W - 12));
    return obs;
}

// Reveal true obstacles within SENSE of `c` into the rover's known map; return
// the newly discovered cells so the trace and D* Lite can react to just those.
std::vector<Point> reveal(const ObSet& truth, ObSet& known_obs, Grid& known, Point c,
                          int W, int H) {
    std::vector<Point> fresh;
    for (int dr = -SENSE; dr <= SENSE; ++dr)
        for (int dc = -SENSE; dc <= SENSE; ++dc) {
            Point p{c.row + dr, c.col + dc};
            if (p.row < 0 || p.row >= H || p.col < 0 || p.col >= W) continue;
            if (truth.count({p.row, p.col}) && !known_obs.count({p.row, p.col})) {
                known_obs.insert({p.row, p.col});
                known.update_cell(p, OBSTACLE);
                fresh.push_back(p);
            }
        }
    return fresh;
}

bool blocks_path(const std::vector<Point>& path, const ObSet& known_obs) {
    for (const Point& p : path)
        if (known_obs.count({p.row, p.col})) return true;
    return false;
}

// One full drive. `dlite` picks the planner; `trace` (optional) records it.
// Returns total node expansions across the initial search and every replan.
int drive(bool dlite, int W, int H, const ObSet& truth, Point start, Point goal,
          SearchTrace* trace) {
    Grid known(H, W);   // uniform cost 1; obstacles get carved in as they're seen
    ObSet known_obs;
    int total = 0;

    auto on_expand = [&](const Point& e, int g, int f,
                         const std::vector<std::pair<Point, int>>& fr) {
        ++total;
        if (!trace) return;
        std::vector<SearchTrace::FrontierEntry> fe;
        fe.reserve(fr.size());
        for (const auto& [p, pf] : fr) fe.push_back({p, pf});
        trace->record_step(e, g, f, std::move(fe));
    };

    // The rover sees its immediate surroundings before it rolls; bake those into
    // the trace's base map so the animation starts from what it already knows.
    auto fresh0 = reveal(truth, known_obs, known, start, W, H);
    if (trace) {
        for (int r = 0; r < H; ++r)
            for (int c = 0; c < W; ++c)
                trace->set_cost({r, c}, known.get_cost({r, c}));
        trace->set_start(start);
        trace->set_goal(goal);
    }
    (void)fresh0;

    DLite<Point, Grid> d(known, goal, h_to(start));
    std::vector<Point> path;
    if (dlite) path = d.plan(start, on_expand);
    else { AStar<Point, Grid> a(known, goal, h_to(goal)); path = a.plan(start, on_expand); }
    if (trace) trace->set_path(path);

    Point robot = start;
    int guard = 0;
    while (robot != goal && ++guard < 100000) {
        if (path.size() < 2) break;
        Point next = path[1];
        std::vector<Point> fresh = reveal(truth, known_obs, known, next, W, H);

        if (blocks_path(path, known_obs)) {
            if (trace) {
                std::vector<SearchTrace::Change> ch;
                for (const Point& p : fresh) ch.push_back({p, OBSTACLE});
                trace->begin_replan(robot, std::move(ch));
            }
            if (dlite) {
                for (const Point& p : fresh) {
                    d.notify_vertex_change(p);
                    for (const auto& [nb, c] : known.get_neighbors(p))
                        d.notify_vertex_change(nb);
                }
                d.set_heuristic(h_to(robot));
                path = d.replan(robot, on_expand);
            } else {
                AStar<Point, Grid> a(known, goal, h_to(goal));
                path = a.plan(robot, on_expand);
            }
            if (trace) trace->set_path(path);
            if (path.size() < 2) break;
            continue;
        }
        robot = next;
        path.erase(path.begin());
    }
    return total;
}

void write_scaling(const std::string& path) {
    // count = number of ~6-cell obstacle blobs; ~area/60 keeps density near 10%.
    struct Row { int W, H, count; };
    const std::vector<Row> rows = {
        {40, 24, 16}, {80, 48, 64}, {120, 72, 144}, {160, 96, 256}};
    std::ofstream out(path);
    out << "{\n  \"points\": [";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto& rw = rows[i];
        ObSet truth = make_obstacles(rw.W, rw.H, rw.count, /*seed=*/7);
        Point s{rw.H / 2, 2}, g{rw.H / 2, rw.W - 3};
        int a = drive(false, rw.W, rw.H, truth, s, g, nullptr);
        int d = drive(true,  rw.W, rw.H, truth, s, g, nullptr);
        out << (i == 0 ? "" : ",") << "\n    {\"cells\": " << (rw.W * rw.H)
            << ", \"w\": " << rw.W << ", \"h\": " << rw.H
            << ", \"astar\": " << a << ", \"dlite\": " << d << "}";
        std::cout << "  scaling " << rw.W << "x" << rw.H
                  << ": A*=" << a << " D*L=" << d << std::endl;
    }
    out << "\n  ]\n}\n";
}

}  // namespace

int main() {
    // The star drive: a mid-size map, legible as an animation, with enough
    // discoveries to show A* re-searching while D* Lite mostly reuses.
    const int W = 64, H = 40;
    const Point start{H / 2, 2}, goal{H / 2, W - 3};
    ObSet truth = make_obstacles(W, H, /*count=*/45, /*seed=*/7);

    {
        SearchTrace trace(W, H, "astar", "manhattan", 4);
        int total = drive(false, W, H, truth, start, goal, &trace);
        trace.save("traces/astar_drive.json");
        std::cout << "A*   drive: total expansions " << total
                  << " -> traces/astar_drive.json" << std::endl;
    }
    {
        SearchTrace trace(W, H, "dlite", "manhattan", 4);
        int total = drive(true, W, H, truth, start, goal, &trace);
        trace.save("traces/dlite_drive.json");
        std::cout << "D*L  drive: total expansions " << total
                  << " -> traces/dlite_drive.json" << std::endl;
    }

    write_scaling("traces/scaling.json");
    std::cout << "scaling -> traces/scaling.json\n";
    return 0;
}
