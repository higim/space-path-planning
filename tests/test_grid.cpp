#include <stdexcept>
#include <gtest/gtest.h>

#include "grid.h"

static int cost_to(const Grid& g, const Point& from, const Point& to) {
    for (const auto& [p,c] : g.get_neighbors(from))
        if (p == to) return c;
    return -1;
}

static bool has_neighbor(const Grid& g, const Point& from, const Point& to) {
    return cost_to(g, from, to) != -1;
}

TEST(GridTests, RectangularBounds) {
    Grid g(2, 3);
    g.update_cell({1, 2}, 5);
    EXPECT_EQ(g.get_cost({1, 2}), 5);

    EXPECT_THROW(g.get_cost({2, 1}), std::out_of_range);  // row 2 invalid (h == 2)
    EXPECT_THROW(g.get_cost({0, 3}), std::out_of_range);  // col 3 invalid (w == 3)
    EXPECT_NO_THROW(g.get_cost({1, 2}));
}

// --- Neighbor connectivity & count (4-connected) ---

TEST(GridTests, CornerHasTwoNeighbors) {
    Grid g(3, 3);
    EXPECT_EQ(g.get_neighbors({0, 0}).size(), 2u);  // right + down only
}

TEST(GridTests, EdgeHasThreeNeighbors) {
    Grid g(3, 3);
    EXPECT_EQ(g.get_neighbors({0, 1}).size(), 3u);
}

TEST(GridTests, InteriorHasFourNeighbors) {
    Grid g(3, 3);
    EXPECT_EQ(g.get_neighbors({1, 1}).size(), 4u);
}

TEST(GridTests, OutOfBoundsHasNoNeighbors) {
    Grid g(3, 3);
    EXPECT_TRUE(g.get_neighbors({5, 5}).empty());  // matches Graph contract
}

TEST(GridTests, CornerNeighborsAreOrthogonalOnly) {
    Grid g(3, 3);
    // {0,0}'s neighbors must be exactly {0,1} and {1,0} -- no diagonal {1,1}.
    EXPECT_TRUE(has_neighbor(g, {0, 0}, {0, 1}));
    EXPECT_TRUE(has_neighbor(g, {0, 0}, {1, 0}));
    EXPECT_FALSE(has_neighbor(g, {0, 0}, {1, 1}));
}

// --- Step-cost formula: (here + there) / 2 ---

TEST(GridTests, DefaultStepCostIsOne) {
    Grid g(3, 3);                       // all cells = 1 -> (1+1)/2 = 1
    EXPECT_EQ(cost_to(g, {1, 1}, {1, 2}), 1);
}

TEST(GridTests, StepCostAveragesEndpoints) {
    Grid g(3, 3);
    g.update_cell({1, 2}, 9);           // (1 + 9)/2 = 5
    EXPECT_EQ(cost_to(g, {1, 1}, {1, 2}), 5);
}

TEST(GridTests, StepCostTruncatesTowardZero) {
    Grid g(3, 3);
    g.update_cell({1, 2}, 4);           // (1 + 4)/2 = 2 (integer truncation)
    EXPECT_EQ(cost_to(g, {1, 1}, {1, 2}), 2);
}

TEST(GridTests, StepCostSymmetric) {
    Grid g(3, 3);
    g.update_cell({1, 2}, 7);
    EXPECT_EQ(cost_to(g, {1, 1}, {1, 2}), cost_to(g, {1, 2}, {1, 1}));
}

// --- update_cell / get_cost round-trips & errors ---

TEST(GridTests, UpdateOutOfBoundsThrows) {
    Grid g(2, 2);
    EXPECT_THROW(g.update_cell({2, 0}, 5), std::out_of_range);
}

TEST(GridTests, UpdateDoesNotDisturbNeighbors) {
    Grid g(3, 3);
    g.update_cell({1, 1}, 9);
    EXPECT_EQ(g.get_cost({1, 1}), 9);
    EXPECT_EQ(g.get_cost({0, 1}), 1);   // neighbor untouched
    EXPECT_EQ(g.get_cost({1, 0}), 1);
}

// --- contains / bounds ---

TEST(GridTests, ContainsMatchesBounds) {
    Grid g(2, 3);
    EXPECT_TRUE(g.contains({1, 2}));
    EXPECT_FALSE(g.contains({-1, 0}));  // negative row
    EXPECT_FALSE(g.contains({2, 0}));   // row == h
    EXPECT_FALSE(g.contains({0, 3}));   // col == w
}

// --- Row/col indexing (non-square shape) ---

TEST(GridTests, IndexingNotTransposed) {
    Grid g(2, 5);                       // wide, not square
    g.update_cell({0, 4}, 8);
    EXPECT_EQ(g.get_cost({0, 4}), 8);
    EXPECT_EQ(g.get_cost({1, 0}), 1);   // untouched
}