#pragma once

#include <vector>
#include <functional>
#include <unordered_map>
#include <optional>
#include <limits>

#include "graph.h"
#include "path_planner.h"
#include "priority_list.h"


template <typename T, typename Map = Graph<T>>
class AStar : public PathPlanner<T> {
    public:
        using ExpandCallback = typename PathPlanner<T>::ExpandCallback;

        AStar(const Map& g, const T& goal, std::function<int(const T&)> h);

        std::vector<T> plan(const T& current, ExpandCallback on_expand = nullptr) override;

    private:
        struct NodeInfo {
            int g_score = std::numeric_limits<int>::max();
            int f_score = std::numeric_limits<int>::max();
            std::optional<T> parent = std::nullopt;
        };

        std::unordered_map<T, NodeInfo> map_state;
        PriorityList<T, int> openset;
        const Map& graph;
        const T goal;
        std::function<int (const T&)> h;

        std::vector<T> reconstruct_path(const T& start) const;
};

#include "a_star.tpp"