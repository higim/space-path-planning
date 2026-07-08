#pragma once

#include <set>
#include <unordered_map>
#include <optional>
#include <vector>
#include <utility>
#include <cstddef>
#include <concepts>

template <typename K>
concept Comparable = std::totally_ordered<K>;

template <typename T>
concept Payload = std::totally_ordered<T> && requires(T t) {
    { std::hash<T>{}(t) } -> std::convertible_to<std::size_t>;
};


template <Payload T, Comparable K>
class PriorityList {
    public:
        PriorityList() = default;

        // Insert-or-update: if the payload is already queued, move it to its
        // new key; otherwise add it. At most one entry per payload.
        void insert(const T& payload, const K& key) {
            if (auto it = key_of.find(payload); it != key_of.end()) {
                pq.erase({it->second, payload});   
                it->second = key;                  
            } else {
                key_of.emplace(payload, key);
            }
            pq.insert({key, payload});
        }

        void remove(const T& payload) {
            if (auto it = key_of.find(payload); it != key_of.end()) {
                pq.erase({it->second, payload});
                key_of.erase(it);
            }
        }

        bool contains(const T& payload) const {
            return key_of.contains(payload);
        }

        bool empty() const {
            return pq.empty();
        }

        std::size_t size() const {
            return pq.size();
        }

        // The minimum key, without removing it.
        std::optional<K> top_key() const {
            if (pq.empty()) return std::nullopt;
            return pq.begin()->first;
        }

        // The minimum's payload, without removing it.
        std::optional<T> top() const {
            if (pq.empty()) return std::nullopt;
            return pq.begin()->second;
        }

        // A read-only snapshot of every queued entry as {payload, key} pairs,
        // ordered by key (the underlying set is already sorted). Used to record
        // the frontier during a traced search.
        std::vector<std::pair<T, K>> entries() const {
            std::vector<std::pair<T, K>> out;
            out.reserve(pq.size());
            for (const auto& [key, payload] : pq)
                out.push_back({payload, key});
            return out;
        }

        // Remove and return the minimum's payload.
        std::optional<T> pop() {
            if (pq.empty()) return std::nullopt;
            auto it = pq.begin();
            T payload = it->second;
            pq.erase(it);
            key_of.erase(payload);
            return payload;
        }

    private:
        std::set<std::pair<K, T>> pq;   // ordered by (key, then payload); min = *begin()
        std::unordered_map<T, K> key_of;  // payload -> its current key
};
