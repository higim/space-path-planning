#pragma once

#include <optional>
#include <unordered_map>
#include <functional>
#include <vector>
#include <limits>
#include <algorithm>

#include "graph.h"
#include "path_planner.h"
#include "priority_list.h"


template <typename T, typename Map = Graph<T>>
class DStar : public PathPlanner<T> {
    public:
        using ExpandCallback = typename PathPlanner<T>::ExpandCallback;

        DStar(const Map& g, const T& goal);

        std::vector<T> plan(const T& current, ExpandCallback on_expand = nullptr) override;
        std::vector<T> replan(const T& current, ExpandCallback on_expand = nullptr) override;
        void notify_vertex_change(const T& v) override;
    private:

        enum State {
            NEW,
            OPEN,
            CLOSED
        };

        struct NodeInfo {
            int cost = std::numeric_limits<int>::max();
            int k = std::numeric_limits<int>::max();
            State state = State::NEW;
            std::optional<T> parent = std::nullopt;
        };

        std::unordered_map<T, NodeInfo> map_state;
        PriorityList<T, int> openset;

        const Map& graph;
        const T goal;

        int process_state(ExpandCallback on_expand);
        void insert_state(const T& x, int h_new);
};

#include "d_star.tpp"
