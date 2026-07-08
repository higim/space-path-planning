#pragma once

#include <unordered_map>
#include <vector>

template <typename T> class Graph {
    public:
        Graph() = default;
        
        const std::vector<std::pair<T, int>>& get_neighbors(const T& key) const {
            static const std::vector<std::pair<T, int>> empty;
            if (auto search = g.find(key); search != g.end()) {
                return search->second;
            }
            return empty;
        }

        void add_edge(const T& origin, const T& end, int value) {
            g[origin].push_back({end, value});
        }

        void update_edge(const T& origin, const T& end, int new_value) {
            for (auto& [key, value] : g[origin]) {
                if (key == end) {
                    value = new_value;
                    return;
                }
            }

            add_edge(origin, end, new_value);
        }

    private:
        std::unordered_map<T, std::vector<std::pair<T, int>>> g;
};
