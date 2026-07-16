#pragma once

#include <vector>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <limits>
#include <compare>

#include "graph.h"
#include "path_planner.h"
#include "priority_list.h"

class DLiteKey {
    public:
        DLiteKey(const int value1, const int value2) : k1(value1), k2(value2) {};
        auto operator<=>(const DLiteKey& other) const = default;
        
        int get_k1() const { return k1; };
        int get_k2() const { return k2; };

    private:
        int k1, k2;
};

template <typename T, typename Map = Graph<T>>
class DLite : public PathPlanner<T> {
    public:
        using ExpandCallback = typename PathPlanner<T>::ExpandCallback;

        DLite(const Map& g, const T& goal, std::function<int(const T&)> h);

        std::vector<T> plan(const T& current, ExpandCallback on_expand = nullptr) override;
        std::vector<T> replan(const T& current, ExpandCallback on_expand = nullptr) override;
        void notify_vertex_change(const T& v) override;

        // D* Lite's key uses h(s) = estimated distance from s to the robot, so a
        // moving robot needs its heuristic rebound to the new position before the
        // next replan (km then accounts for the move via h(old_position)). Leave
        // it unset (or zero) for a stationary robot.
        void set_heuristic(std::function<int(const T&)> h_new) { h = std::move(h_new); }

    private:
        struct NodeInfo {
            int g = std::numeric_limits<int>::max();
            int rhs = std::numeric_limits<int>::max();
        };

        std::unordered_map<T, NodeInfo> map_state;
        const Map& graph;
        const T goal;
        T old_position;
        std::function<int (const T&)> h;
        PriorityList<T, DLiteKey> openset;
        int km = 0;

        std::vector<T> reconstruct_path(const T& start) const;
        void compute_shortest_path(const T& current, ExpandCallback on_expand);
        DLiteKey compute_key(const T& node) const;
        void update_vertex(const T& node);
};

#include "d_lite.tpp"