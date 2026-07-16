#include <string>
#include <vector>
#include <gtest/gtest.h>

#include "grid.h"
#include "graph.h"
#include "d_star.h"
#include "a_star.h"

using namespace std;

// A diamond with two ways from S to G: via A (cheap) or via B (dear).
// Edges are symmetric because D* reads c(x,y) from get_neighbors(x) and assumes
// c(x,y) == c(y,x).
static Graph<string> make_diamond(int ag_cost = 1, int bg_cost = 5) {
    Graph<string> g;
    auto edge = [&](const string& a, const string& b, int c) {
        g.add_edge(a, b, c);
        g.add_edge(b, a, c);
    };
    edge("S", "A", 1);
    edge("A", "G", ag_cost);
    edge("S", "B", 1);
    edge("B", "G", bg_cost);
    return g;
}

// Retell an edge cost in both directions and tell D* which endpoints moved.
static void change_edge(Graph<string>& g, DStar<string>& d,
                        const string& a, const string& b, int c) {
    g.update_edge(a, b, c);
    g.update_edge(b, a, c);
    d.notify_vertex_change(a);
    d.notify_vertex_change(b);
}

// --- D* on an abstract graph -------------------------------------------------

TEST(DStarTests, FindsOptimalPathOnDiamond) {
    Graph<string> g = make_diamond();      // via A costs 2, via B costs 6
    DStar<string> d(g, "G");
    EXPECT_EQ(d.plan("S"), (vector<string>{"S", "A", "G"}));
}

// The reason D* exists: after the world changes, reuse the old search instead of
// starting over. Raise the A-G edge so the cheap route disappears.
TEST(DStarTests, ReplanRerootsAfterEdgeIncrease) {
    Graph<string> g = make_diamond();
    DStar<string> d(g, "G");
    ASSERT_EQ(d.plan("S"), (vector<string>{"S", "A", "G"}));

    change_edge(g, d, "A", "G", 100);      // A-route now costs 101, B-route 6
    EXPECT_EQ(d.replan("S"), (vector<string>{"S", "B", "G"}));
}

// The symmetric case: a route that was expensive becomes the best one.
TEST(DStarTests, ReplanRerootsAfterEdgeDecrease) {
    Graph<string> g = make_diamond(/*ag_cost=*/9);  // via A costs 10, via B costs 6
    DStar<string> d(g, "G");
    ASSERT_EQ(d.plan("S"), (vector<string>{"S", "B", "G"}));

    change_edge(g, d, "A", "G", 1);        // A-route now costs 2, B-route 6
    EXPECT_EQ(d.replan("S"), (vector<string>{"S", "A", "G"}));
}

TEST(DStarTests, UnreachableGoalReturnsEmpty) {
    Graph<string> g;                       // G lives in its own component
    g.add_edge("S", "A", 1);
    g.add_edge("A", "S", 1);

    DStar<string> d(g, "G");
    EXPECT_TRUE(d.plan("S").empty());
}

// --- the SAME D* running on a Grid ------------------------------------------

static int grid_path_cost(const Grid& g, const vector<Point>& path) {
    int total = 0;
    for (size_t i = 0; i + 1 < path.size(); ++i)
        for (const auto& [nb, c] : g.get_neighbors(path[i]))
            if (nb == path[i + 1]) { total += c; break; }
    return total;
}

static auto zero_h = [](const Point&) { return 0; };  // A* with h=0 == Dijkstra == optimal

TEST(DStarGridTests, WalksACorridor) {
    Grid grid(1, 4);
    DStar<Point, Grid> d(grid, Point{0, 3});
    EXPECT_EQ(d.plan(Point{0, 0}),
              (vector<Point>{{0, 0}, {0, 1}, {0, 2}, {0, 3}}));
}

TEST(DStarGridTests, RoutesAroundExpensiveCell) {
    Grid grid(3, 3);
    grid.update_cell({0, 1}, 100);

    DStar<Point, Grid> d(grid, Point{0, 2});
    vector<Point> path = d.plan(Point{0, 0});

    ASSERT_FALSE(path.empty());
    EXPECT_EQ(path.front(), (Point{0, 0}));
    EXPECT_EQ(path.back(),  (Point{0, 2}));
    for (const Point& p : path)
        EXPECT_NE(p, (Point{0, 1}));
}

// After a wall drops onto the old route, D*'s replanned path must still be
// optimal -- pin it to a fresh A* on the same, now-modified grid.
TEST(DStarGridTests, ReplanMatchesAStarOptimumAfterBlocking) {
    const int OBSTACLE = 1'000'000;
    Grid grid(6, 6);
    const Point start{0, 0}, goal{5, 5};

    DStar<Point, Grid> d(grid, goal);
    ASSERT_FALSE(d.plan(start).empty());

    // Wall across row 2, columns 0..4; only the far column stays open.
    for (int c = 0; c < 5; ++c) {
        grid.update_cell({2, c}, OBSTACLE);
        d.notify_vertex_change({2, c});
    }
    vector<Point> dstar_path = d.replan(start);

    AStar<Point, Grid> a(grid, goal, zero_h);
    vector<Point> astar_path = a.plan(start);

    ASSERT_FALSE(dstar_path.empty());
    EXPECT_EQ(dstar_path.front(), start);
    EXPECT_EQ(dstar_path.back(),  goal);
    EXPECT_EQ(grid_path_cost(grid, dstar_path), grid_path_cost(grid, astar_path));
    for (const Point& p : dstar_path)          // never walks the wall
        EXPECT_FALSE(p.row == 2 && p.col < 5);
}

// The payoff, made measurable: replanning a small local change touches far fewer
// states than the from-scratch search did.
TEST(DStarGridTests, ReplanExpandsFewerNodesThanInitialSearch) {
    const int OBSTACLE = 1'000'000;
    Grid grid(12, 12);
    const Point start{0, 0}, goal{11, 11};

    int expansions = 0;
    auto count = [&expansions](const Point&, int, int,
                               const vector<pair<Point, int>>&) { ++expansions; };

    DStar<Point, Grid> d(grid, goal);
    d.plan(start, count);
    const int initial_expansions = expansions;

    // A short wall near the goal invalidates only the tail of the path.
    expansions = 0;
    for (int c = 8; c < 11; ++c) {
        grid.update_cell({10, c}, OBSTACLE);
        d.notify_vertex_change({10, c});
    }
    d.replan(start, count);
    const int replan_expansions = expansions;

    EXPECT_GT(initial_expansions, 0);
    EXPECT_LT(replan_expansions, initial_expansions);
}
