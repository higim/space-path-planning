#pragma once

#include <limits>
#include <map>
#include <iterator>
#include <algorithm>

#include "a_star.h"
#include "priority_list.h"

template <typename T, typename Map>
AStar<T, Map>::AStar(const Map& g, const T& goal, std::function<int(const T&)> h) : graph(g), goal(goal), h(h) {};

template <typename T, typename Map>
std::vector<T> AStar<T, Map>::plan(const T& start, ExpandCallback on_expand) {
    openset.insert(start, h(start));   // f(start) = g(0) + h(start)
    map_state[start] = {
        .g_score = 0,
        .f_score = h(start)
    };

    while (!openset.empty()) {
        T current = openset.pop().value();

        // Trace
        if (on_expand) {
            std::vector<std::pair<T, int>> frontier;
            for (const auto& [node, key] : openset.entries())
                frontier.push_back({node, key});
            on_expand(current, map_state[current].g_score,
                      map_state[current].f_score, frontier);
        }

        if (current == goal) {
            return reconstruct_path(start);
        }

        for (const auto& [neighbor_key, edge_cost] : graph.get_neighbors(current)) {
            int g_score = map_state[current].g_score;
            int temp_g = g_score + edge_cost;
            int g_neighbor = map_state[neighbor_key].g_score;
            if (temp_g < g_neighbor) {
                map_state[neighbor_key].parent = current;
                map_state[neighbor_key].g_score = temp_g;
                map_state[neighbor_key].f_score = temp_g + h(neighbor_key);
                openset.insert(neighbor_key, temp_g + h(neighbor_key));  // order by f; upsert = decrease-key
            }
        }
    };

    return {};
}

template <typename T, typename Map>
std::vector<T> AStar<T, Map>::reconstruct_path(const T& start) const {
    std::vector<T> path = {goal};
    T current = goal;
    while (map_state.at(current).parent.has_value() && map_state.at(current).parent.value() != start) {
        current = map_state.at(current).parent.value();
        path.push_back(current);
    }

    path.push_back(start);
    std::reverse(path.begin(), path.end());
    return path;
};
