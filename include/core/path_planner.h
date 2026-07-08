#pragma once

#include <vector>
#include <functional>
#include <utility>

template <typename T>
class PathPlanner {
    public:
        // Invoked on each node expansion so callers can trace the search
        // without duplicating the algorithm. Null (the default) = no tracing.
        using ExpandCallback = std::function<void(
            const T& expanded, int g, int f,
            const std::vector<std::pair<T, int>>& frontier)>;

        PathPlanner() = default;
        virtual ~PathPlanner() = default;

        virtual std::vector<T> plan(const T& current, ExpandCallback on_expand = nullptr) = 0;
        virtual std::vector<T> replan(const T& current, ExpandCallback on_expand = nullptr) {
            return plan(current, on_expand);
        }
        virtual void notify_vertex_change(const T& v) {};
};