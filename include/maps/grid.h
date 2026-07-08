#pragma once

#include <stdexcept>
#include <vector>
#include <array>

#include "point.h"

class Grid {
    public:
        Grid(int size_i, int size_j) : h(size_i), w(size_j), cells(size_i*size_j, 1) {};

        std::vector<std::pair<Point, int>> get_neighbors(const Point& key) const;
        void update_cell(const Point& key, int cost);
        int get_cost(const Point& key) const;
        bool contains(const Point& key) const { return in_bounds(key); }

    private:
        bool in_bounds(const Point& key) const;   // key.first = row (< h), key.second = col (< w)

        int h, w;
        std::vector<int> cells;
};
