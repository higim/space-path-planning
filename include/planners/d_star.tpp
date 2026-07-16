#include "d_star.h"

// This is Stentz's D* (1994). Every node's `cost` is its estimated cost-TO-GOAL
// (h in the paper), `k` is the smallest cost it has held while on the open list
// (the key the open list is sorted by), and `parent` is the back-pointer toward
// the goal. Edges are treated as symmetric: c(x,y) is read from get_neighbors(x).
// `Map` is any type exposing get_neighbors(node) (e.g. Graph<T> or Grid).
template <typename T, typename Map>
void DStar<T, Map>::insert_state(const T& x, int h_new) {
    NodeInfo& info = map_state[x];
    switch (info.state) {
        case State::NEW:    info.k = h_new;                       break;
        case State::OPEN:   info.k = std::min(info.k, h_new);     break;
        case State::CLOSED: info.k = std::min(info.cost, h_new);  break;
    }
    info.cost  = h_new;
    info.state = State::OPEN;
    openset.insert(x, info.k);
}

template <typename T, typename Map>
DStar<T, Map>::DStar(const Map& g, const T& goal) : graph(g), goal(goal) {
    insert_state(goal, 0);   // goal has zero cost-to-goal, no back-pointer
}

template <typename T, typename Map>
int DStar<T, Map>::process_state(ExpandCallback on_expand) {
    auto popped = openset.pop();
    if (!popped) return -1;

    const T x = popped.value();

    // Pin the bucket set before taking a long-lived reference to X: a later
    // map_state[y] on a NEW neighbor would otherwise insert, possibly rehash the
    // table, and dangle X (and any Y) mid-loop. Touch every neighbor first so no
    // insertion happens once references are live. (A* sidesteps this by never
    // holding a NodeInfo&; D* keeps one for readability, so we pin here instead.)
    for (const auto& [y, c_xy] : graph.get_neighbors(x)) map_state[y];

    NodeInfo& X = map_state[x];
    const int k_old = X.k;
    X.state = State::CLOSED;

    // If X was raised (its cost is now above the key it was queued with), first
    // try to repair X by routing it through a cheaper, up-to-date neighbor.
    if (k_old < X.cost) {
        for (const auto& [y, c_xy] : graph.get_neighbors(x)) {
            const NodeInfo& Y = map_state[y];
            if (Y.state != State::NEW && Y.cost <= k_old && X.cost > Y.cost + c_xy) {
                X.parent = y;
                X.cost   = Y.cost + c_xy;
            }
        }
    }

    if (k_old == X.cost) {
        for (const auto& [y, c_xy] : graph.get_neighbors(x)) {
            NodeInfo& Y = map_state[y];
            const bool is_child = Y.parent.has_value() && Y.parent.value() == x;
            if (Y.state == State::NEW
                || (is_child  && Y.cost != X.cost + c_xy)
                || (!is_child && Y.cost >  X.cost + c_xy)) {
                Y.parent = x;
                insert_state(y, X.cost + c_xy);
            }
        }
    } else {
        // X is still raised: propagate more conservatively.
        for (const auto& [y, c_xy] : graph.get_neighbors(x)) {
            NodeInfo& Y = map_state[y];
            const bool is_child = Y.parent.has_value() && Y.parent.value() == x;
            if (Y.state == State::NEW || (is_child && Y.cost != X.cost + c_xy)) {
                Y.parent = x;
                insert_state(y, X.cost + c_xy);
            } else if (!is_child && Y.cost > X.cost + c_xy) {
                insert_state(x, X.cost);          // re-open X so it keeps propagating
            } else if (!is_child && X.cost > Y.cost + c_xy
                       && Y.state == State::CLOSED && Y.cost > k_old) {
                insert_state(y, Y.cost);          // Y may hand X a cheaper route
            }
        }
    }

    // Trace one "expand" per processed state. D* has no A*-style f-value, so we
    // map the node's cost-to-goal onto g and the key it was popped at onto f;
    // the frontier is the open list *after* this expansion (matches A*).
    if (on_expand) {
        std::vector<std::pair<T, int>> frontier;
        for (const auto& [node, key] : openset.entries())
            frontier.push_back({node, key});
        on_expand(x, X.cost, k_old, frontier);
    }

    return openset.empty() ? -1 : openset.top_key().value();
}

template <typename T, typename Map>
std::vector<T> DStar<T, Map>::plan(const T& start, ExpandCallback on_expand) {
    int k_min;
    while ((k_min = process_state(on_expand)) != -1 && map_state[start].cost > k_min) {}

    if (map_state[start].cost == std::numeric_limits<int>::max())
        return {};   // start unreachable

    std::vector<T> path;
    T node = start;
    while (node != goal) {
        path.push_back(node);
        const NodeInfo& info = map_state[node];
        if (!info.parent.has_value()) return {};
        node = info.parent.value();
    }
    path.push_back(goal);
    return path;
}

template <typename T, typename Map>
std::vector<T> DStar<T, Map>::replan(const T& current, ExpandCallback on_expand) {
    return plan(current, on_expand);
}

template <typename T, typename Map>
void DStar<T, Map>::notify_vertex_change(const T& v) {
    if (map_state.contains(v) && map_state[v].state == State::CLOSED)
        insert_state(v, map_state[v].cost);
}
