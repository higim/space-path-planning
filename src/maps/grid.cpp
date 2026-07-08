#include "grid.h"

bool Grid::in_bounds(const Point& key) const {
    return 0 <= key.row && key.row < h && 0 <= key.col && key.col < w;
}

int Grid::get_cost(const Point& key) const {
    if (in_bounds(key))
        return cells[key.row * w + key.col];
    throw std::out_of_range("Point not valid");
}

void Grid::update_cell(const Point& key, int cost) {
    if (!in_bounds(key))
        throw std::out_of_range("Point not valid");
    cells[key.row * w + key.col] = cost;
}

std::vector<std::pair<Point, int>> Grid::get_neighbors(const Point& key) const {
    // 4-connected: orthogonal moves only. Every move is distance 1
    static const std::array<Point, 4> deltas = {{
        {-1, 0}, {1, 0}, {0, -1}, {0, 1},
    }};

    std::vector<std::pair<Point, int>> neighbors;
    if (!in_bounds(key)) return neighbors;  // unknown cell -> no neighbors (matches Graph)

    neighbors.reserve(deltas.size());
    const int here = cells[key.row * w + key.col];

    for (const auto& d : deltas) {
        Point candidate{key.row + d.row, key.col + d.col};
        if (in_bounds(candidate)) {
            const int there = cells[candidate.row * w + candidate.col];
            const int step = (here + there) / 2;
            neighbors.push_back({candidate, step});
        }
    }
    return neighbors;
}