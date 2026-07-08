#include <string>
#include <gtest/gtest.h>

#include "priority_list.h"

using namespace std;

// --- Empty state ---------------------------------------------------------

TEST(PriorityListTests, EmptyList) {
    PriorityList<string, int> p;
    EXPECT_TRUE(p.empty());
    EXPECT_EQ(p.size(), 0u);
    EXPECT_FALSE(p.top_key().has_value());  // no min on an empty queue
    EXPECT_FALSE(p.pop().has_value());      // pop() is safe when empty
}

// --- Basic insert / membership ------------------------------------------

TEST(PriorityListTests, InsertIncreasesSize) {
    PriorityList<string, int> p;
    p.insert("dos", 2);
    EXPECT_EQ(p.size(), 1u);
    EXPECT_FALSE(p.empty());
    EXPECT_TRUE(p.contains("dos"));
    EXPECT_FALSE(p.contains("uno"));
}

// --- Ordering: pop always yields the minimum key -------------------------

TEST(PriorityListTests, PopReturnsMinimumFirst) {
    PriorityList<string, int> p;
    p.insert("b", 2);
    p.insert("a", 1);
    p.insert("c", 3);

    EXPECT_EQ(p.top_key().value(), 1);
    EXPECT_EQ(p.pop().value(), "a");
    EXPECT_EQ(p.pop().value(), "b");
    EXPECT_EQ(p.pop().value(), "c");
    EXPECT_TRUE(p.empty());
}

TEST(PriorityListTests, TopKeyDoesNotRemove) {
    PriorityList<string, int> p;
    p.insert("a", 5);
    EXPECT_EQ(p.top_key().value(), 5);
    EXPECT_EQ(p.size(), 1u);              // peeking must not consume
    EXPECT_EQ(p.top_key().value(), 5);
}

// --- The invariant: pop() must erase from BOTH containers ----------------

TEST(PriorityListTests, PopClearsMembership) {
    PriorityList<string, int> p;
    p.insert("a", 1);
    EXPECT_EQ(p.pop().value(), "a");
    EXPECT_FALSE(p.contains("a"));        // key_of must be cleaned up too
    EXPECT_TRUE(p.empty());
}

// --- Upsert: same payload never duplicates, key is updated ---------------

TEST(PriorityListTests, UpsertDecreaseKeyReorders) {
    PriorityList<string, int> p;
    p.insert("a", 10);
    p.insert("b", 5);
    p.insert("a", 1);                     // decrease-key: a jumps ahead of b

    EXPECT_EQ(p.size(), 2u);              // still 2 entries, not 3
    EXPECT_EQ(p.pop().value(), "a");
    EXPECT_EQ(p.pop().value(), "b");
}

TEST(PriorityListTests, UpsertIncreaseKeyReorders) {
    PriorityList<string, int> p;
    p.insert("a", 1);
    p.insert("b", 5);
    p.insert("a", 10);                    // raise a's key behind b

    EXPECT_EQ(p.size(), 2u);
    EXPECT_EQ(p.pop().value(), "b");
    EXPECT_EQ(p.pop().value(), "a");
}

// --- remove() ------------------------------------------------------------

TEST(PriorityListTests, RemoveExisting) {
    PriorityList<string, int> p;
    p.insert("a", 1);
    p.insert("b", 2);
    p.remove("a");

    EXPECT_FALSE(p.contains("a"));
    EXPECT_EQ(p.size(), 1u);
    EXPECT_EQ(p.pop().value(), "b");
}

TEST(PriorityListTests, RemoveMissingIsNoop) {
    PriorityList<string, int> p;
    p.insert("a", 1);
    p.remove("zzz");                      // not present — must not throw or corrupt
    EXPECT_EQ(p.size(), 1u);
    EXPECT_TRUE(p.contains("a"));
}

TEST(PriorityListTests, RemoveThenReinsert) {
    PriorityList<string, int> p;
    p.insert("a", 1);
    p.remove("a");
    EXPECT_TRUE(p.empty());
    p.insert("a", 7);                     // stale key must not linger
    EXPECT_EQ(p.top_key().value(), 7);
    EXPECT_EQ(p.size(), 1u);
}