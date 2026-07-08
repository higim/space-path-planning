#pragma once

#include <compare>
#include <cstddef>
#include <functional>
#include <ostream>

struct Point {
    int row = 0;
    int col = 0;

    auto operator<=>(const Point&) const = default;
};

template <>
struct std::hash<Point> {
    std::size_t operator()(const Point& p) const noexcept {
        std::size_t h1 = std::hash<int>{}(p.row);
        std::size_t h2 = std::hash<int>{}(p.col);
        return h1 ^ (h2 << 1);
    }
};

inline std::ostream& operator<<(std::ostream& os, const Point& p) {
    return os << '{' << p.row << ", " << p.col << '}';
}