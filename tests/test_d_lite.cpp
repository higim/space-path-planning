#include <string>
#include <vector>
#include <gtest/gtest.h>

#include "grid.h"
#include "graph.h"
#include "d_lite.h"
#include "a_star.h"

using namespace std;

// D* Lite searches backward from the goal, so its heuristic estimates distance
// to the *start*, and its optimality proof needs h(start) == 0. A zero heuristic
// satisfies that trivially and keeps these tests independent of that subtlety
// (D* Lite then degrades to a backward uniform-cost search).
static auto zero_str   = [](const string&) { return 0; };
static auto zero_point = [](const Point&)  { return 0; };

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

static void change_edge(Graph<string>& g, DLite<string>& d,
                        const string& a, const string& b, int c) {
    g.update_edge(a, b, c);
    g.update_edge(b, a, c);
    d.notify_vertex_change(a);
    d.notify_vertex_change(b);
}

// Raising a grid cell moves every edge incident to it, so D* Lite must be told
// about the cell AND each of its neighbors (per notify_vertex_change's contract).
static void block_cell(Grid& g, DLite<Point, Grid>& d, const Point& p, int cost) {
    g.update_cell(p, cost);
    d.notify_vertex_change(p);
    for (const auto& [nb, c] : g.get_neighbors(p))
        d.notify_vertex_change(nb);
}

// --- D* Lite on an abstract graph -------------------------------------------

TEST(DLiteTests, FindsOptimalPathOnDiamond) {
    Graph<string> g = make_diamond();
    DLite<string> d(g, "G", zero_str);
    EXPECT_EQ(d.plan("S"), (vector<string>{"S", "A", "G"}));
}

TEST(DLiteTests, ReplanRerootsAfterEdgeIncrease) {
    Graph<string> g = make_diamond();
    DLite<string> d(g, "G", zero_str);
    ASSERT_EQ(d.plan("S"), (vector<string>{"S", "A", "G"}));

    change_edge(g, d, "A", "G", 100);
    EXPECT_EQ(d.replan("S"), (vector<string>{"S", "B", "G"}));
}

TEST(DLiteTests, ReplanRerootsAfterEdgeDecrease) {
    Graph<string> g = make_diamond(/*ag_cost=*/9);
    DLite<string> d(g, "G", zero_str);
    ASSERT_EQ(d.plan("S"), (vector<string>{"S", "B", "G"}));

    change_edge(g, d, "A", "G", 1);
    EXPECT_EQ(d.replan("S"), (vector<string>{"S", "A", "G"}));
}

TEST(DLiteTests, UnreachableGoalReturnsEmpty) {
    Graph<string> g;
    g.add_edge("S", "A", 1);
    g.add_edge("A", "S", 1);

    DLite<string> d(g, "G", zero_str);
    EXPECT_TRUE(d.plan("S").empty());
}

// --- the SAME D* Lite running on a Grid -------------------------------------

static int grid_path_cost(const Grid& g, const vector<Point>& path) {
    int total = 0;
    for (size_t i = 0; i + 1 < path.size(); ++i)
        for (const auto& [nb, c] : g.get_neighbors(path[i]))
            if (nb == path[i + 1]) { total += c; break; }
    return total;
}

TEST(DLiteGridTests, WalksACorridor) {
    Grid grid(1, 4);
    DLite<Point, Grid> d(grid, Point{0, 3}, zero_point);
    EXPECT_EQ(d.plan(Point{0, 0}),
              (vector<Point>{{0, 0}, {0, 1}, {0, 2}, {0, 3}}));
}

TEST(DLiteGridTests, RoutesAroundExpensiveCell) {
    Grid grid(3, 3);
    grid.update_cell({0, 1}, 100);

    DLite<Point, Grid> d(grid, Point{0, 2}, zero_point);
    vector<Point> path = d.plan(Point{0, 0});

    ASSERT_FALSE(path.empty());
    EXPECT_EQ(path.front(), (Point{0, 0}));
    EXPECT_EQ(path.back(),  (Point{0, 2}));
    for (const Point& p : path)
        EXPECT_NE(p, (Point{0, 1}));
}

TEST(DLiteGridTests, ReplanMatchesAStarOptimumAfterBlocking) {
    const int OBSTACLE = 1'000'000;
    Grid grid(6, 6);
    const Point start{0, 0}, goal{5, 5};

    DLite<Point, Grid> d(grid, goal, zero_point);
    ASSERT_FALSE(d.plan(start).empty());

    for (int c = 0; c < 5; ++c)
        block_cell(grid, d, {2, c}, OBSTACLE);
    vector<Point> dlite_path = d.replan(start);

    AStar<Point, Grid> a(grid, goal, zero_point);
    vector<Point> astar_path = a.plan(start);

    ASSERT_FALSE(dlite_path.empty());
    EXPECT_EQ(dlite_path.front(), start);
    EXPECT_EQ(dlite_path.back(),  goal);
    EXPECT_EQ(grid_path_cost(grid, dlite_path), grid_path_cost(grid, astar_path));
    for (const Point& p : dlite_path)
        EXPECT_FALSE(p.row == 2 && p.col < 5);
}

TEST(DLiteGridTests, ReplanExpandsFewerNodesThanInitialSearch) {
    const int OBSTACLE = 1'000'000;
    Grid grid(12, 12);
    const Point start{0, 0}, goal{11, 11};

    int expansions = 0;
    auto count = [&expansions](const Point&, int, int,
                               const vector<pair<Point, int>>&) { ++expansions; };

    DLite<Point, Grid> d(grid, goal, zero_point);
    d.plan(start, count);
    const int initial_expansions = expansions;

    expansions = 0;
    for (int c = 8; c < 11; ++c)
        block_cell(grid, d, {10, c}, OBSTACLE);
    d.replan(start, count);
    const int replan_expansions = expansions;

    EXPECT_GT(initial_expansions, 0);
    EXPECT_LT(replan_expansions, initial_expansions);
}
