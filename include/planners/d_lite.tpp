#pragma once

#include "d_lite.h"

// D* Lite (Koenig & Likhachev, 2002). Searches backward from the goal using
// local consistency: g is the last-known cost-to-goal, rhs is a one-step
// lookahead (min over neighbors of c(n,s)+g(s)). `Map` is any type exposing
// get_neighbors(node) (e.g. Graph<T> or Grid).

template <typename T, typename Map>
DLite<T, Map>::DLite(const Map& g, const T& goal, std::function<int(const T&)> h): graph(g), goal(goal), h(h) {
    map_state[goal] = {
        .rhs = 0
    };

    openset.insert(goal, compute_key(goal));
};

template <typename T, typename Map>
void DLite<T, Map>::compute_shortest_path(const T& current, ExpandCallback on_expand) {
    while (!openset.empty() && (map_state[current].g != map_state[current].rhs || compute_key(current) > openset.top_key().value())) {
        T u = openset.top().value();
        DLiteKey k_old = openset.top_key().value();
        DLiteKey k_new = compute_key(u);

        if (k_old < k_new) {
            // Stale key (km grew since u was queued): reorder, don't expand yet.
            openset.insert(u, k_new);
            continue;
        }

        openset.pop();
        if (map_state[u].g > map_state[u].rhs) {
            // Overconsistent: lower g to rhs and let predecessors react.
            map_state[u].g = map_state[u].rhs;
            for (const auto& [neighbor, cost] : graph.get_neighbors(u)) {
                update_vertex(neighbor);
            }
        } else {
            // Underconsistent: raise g to infinity and re-examine u and its predecessors.
            map_state[u].g = std::numeric_limits<int>::max();
            for (const auto& [neighbor, cost] : graph.get_neighbors(u)) {
                update_vertex(neighbor);
            }
            update_vertex(u);
        }

        // Trace one "expand" per processed vertex. D* Lite's priority is a
        // two-part key; we surface g(u) and the key's first component (k1) so the
        // frontier snapshot matches the {node, int} shape A* emits.
        if (on_expand) {
            std::vector<std::pair<T, int>> frontier;
            for (const auto& [node, key] : openset.entries())
                frontier.push_back({node, key.get_k1()});
            on_expand(u, map_state[u].g, k_new.get_k1(), frontier);
        }
    }
}

template <typename T, typename Map>
DLiteKey DLite<T, Map>::compute_key(const T& node) const {
    constexpr int INF = std::numeric_limits<int>::max();
    const int g = map_state.at(node).g;
    const int rhs = map_state.at(node).rhs;
    const int min_g = std::min(g, rhs);
    // An unexplored node (min_g == INF) gets an infinite key: adding h + km would
    // overflow, and it must sort last anyway.
    if (min_g == INF) return DLiteKey(INF, INF);
    return DLiteKey(min_g + h(node) + km, min_g);
}

template <typename T, typename Map>
void DLite<T, Map>::update_vertex(const T& node) {
    if (node != goal) {
        int min_rhs = std::numeric_limits<int>::max();
        for (const auto& [neighbor_key, edge_cost] : graph.get_neighbors(node)) {
            int g_n = map_state[neighbor_key].g;
            if (g_n != std::numeric_limits<int>::max()) {
                min_rhs = std::min(min_rhs, edge_cost + g_n);
            }
        }
        map_state[node].rhs = min_rhs;
    }

    openset.remove(node);
    if (map_state[node].g != map_state[node].rhs) openset.insert(node, compute_key(node));
}

template <typename T, typename Map>
std::vector<T> DLite<T, Map>::reconstruct_path(const T& start) const {
    std::vector<T> path = {start};
    T current = start;
    std::unordered_set<T> visited = {start};

    // A neighbor the search never touched isn't in map_state; treat it as
    // unreachable (g = inf) rather than letting at() throw.
    auto g_of = [this](const T& n) {
        auto it = map_state.find(n);
        return it == map_state.end() ? std::numeric_limits<int>::max() : it->second.g;
    };

    while (current != goal) {
        T next;
        int best = std::numeric_limits<int>::max();
        for (const auto& [neighbor_key, edge_cost] : graph.get_neighbors(current)) {
            int g_n = g_of(neighbor_key);
            if (g_n == std::numeric_limits<int>::max()) continue;   // unreachable neighbor
            int through = edge_cost + g_n;                          // total cost to goal via neighbor
            if (through < best) {
                best = through;
                next = neighbor_key;
            }
        }

        if (best == std::numeric_limits<int>::max()) return {};
        if (!visited.insert(next).second) return {};   // revisited a node -> no descending path
        path.push_back(next);
        current = next;
    }

    return path;
}

template <typename T, typename Map>
std::vector<T> DLite<T, Map>::plan(const T& current, ExpandCallback on_expand) {
    old_position = current;
    compute_shortest_path(current, on_expand);
    return reconstruct_path(current);
}

template <typename T, typename Map>
std::vector<T> DLite<T, Map>::replan(const T& current, ExpandCallback on_expand) {
    km += h(old_position);
    old_position = current;
    compute_shortest_path(current, on_expand);
    return reconstruct_path(current);
}

template <typename T, typename Map>
void DLite<T, Map>::notify_vertex_change(const T& v) {
    // The caller has already mutated the map; recompute v's lookahead and let it
    // re-enter the queue if it became inconsistent. For a grid cell change, the
    // caller calls this for the cell AND each neighbor (every incident edge moved).
    update_vertex(v);
}
