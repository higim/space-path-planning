#pragma once

#include <vector>

template <typename T>
class PathPlanner {
    public:
        PathPlanner() = default;
        virtual ~PathPlanner() = default;

        virtual std::vector<T> plan(const T& current) = 0;
        virtual std::vector<T> replan(const T& current) {
            return plan(current);
        }
        virtual void notify_vertex_change(const T& v) {};
};