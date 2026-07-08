#pragma once
#include <cstdlib>
#include "core/point.h"

namespace heuristics {
    inline int manhattan(const Point& a, const Point& b) {
        return std::abs(a.row - b.row) + std::abs(a.col - b.col);
    }

    inline int zero(const Point&, const Point&) {
        return 0;
    }
};