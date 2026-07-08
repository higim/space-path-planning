#include <string>
#include <vector>
#include <unordered_map>
#include <gtest/gtest.h>

#include "grid.h"
#include "graph.h"
#include "a_star.h"

using namespace std;

static Graph<string> make_diamond() {
    Graph<string> g;
    auto edge = [&](const string& a, const string& b, int c) {
        g.add_edge(a, b, c);
        g.add_edge(b, a, c);
    };
    edge("S", "A", 1);
    edge("A", "G", 1);
    edge("S", "B", 1);
    edge("B", "G", 5);
    return g;
}

TEST(AStarTests, FindsOptimalPathWithZeroHeuristic) {
    Graph<string> g = make_diamond();
    auto h = [](const string&) { return 0; };   // degrades to Dijkstra

    AStar<string> a(g, "G", h);
    EXPECT_EQ(a.plan("S"), (vector<string>{"S", "A", "G"}));
}

TEST(AStarTests, FindsOptimalPathWithAdmissibleHeuristic) {
    Graph<string> g = make_diamond();
    unordered_map<string, int> est{{"S", 2}, {"A", 1}, {"B", 1}, {"G", 0}};
    auto h = [est](const string& n) { return est.at(n); };

    AStar<string> a(g, "G", h);
    EXPECT_EQ(a.plan("S"), (vector<string>{"S", "A", "G"}));
}

// SAME A* running on a Grid

static auto zero_point_h = [](const Point&) { return 0; };  // h=0 keeps A* optimal on a Grid

TEST(AStarGridTests, WalksACorridor) {
    Grid grid(1, 4); 
    AStar<Point, Grid> a(grid, Point{0, 3}, zero_point_h);
    EXPECT_EQ(a.plan(Point{0, 0}),
              (vector<Point>{{0, 0}, {0, 1}, {0, 2}, {0, 3}}));
}

TEST(AStarGridTests, RoutesAroundExpensiveCell) {
    Grid grid(3, 3);             
    grid.update_cell({0, 1}, 100); 

    AStar<Point, Grid> a(grid, Point{0, 2}, zero_point_h);
    vector<Point> path = a.plan(Point{0, 0});

    ASSERT_FALSE(path.empty());
    EXPECT_EQ(path.front(), (Point{0, 0}));
    EXPECT_EQ(path.back(),  (Point{0, 2}));
    for (const Point& p : path)
        EXPECT_NE(p, (Point{0, 1}));
}

TEST(AStarTests, HeuristicDoesNotOverrideTrueCost) {
    Graph<string> g = make_diamond();
    // Make B's node look attractive (low h) yet the optimal path is via A.
    unordered_map<string, int> est{{"S", 2}, {"A", 1}, {"B", 0}, {"G", 0}};
    auto h = [est](const string& n) { return est.at(n); };

    AStar<string> a(g, "G", h);
    EXPECT_EQ(a.plan("S"), (vector<string>{"S", "A", "G"}));
}
