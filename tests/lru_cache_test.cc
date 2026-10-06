#include "shardcache/lru_cache.h"

#include <gtest/gtest.h>

#include <list>
#include <random>
#include <stdexcept>
#include <unordered_map>

using shardcache::LruCache;

TEST(LruCache, ZeroCapacityThrows) { EXPECT_THROW(LruCache(0), std::invalid_argument); }

TEST(LruCache, MissingKeyReturnsNullopt) {
  LruCache c(2);
  EXPECT_FALSE(c.get("nope").has_value());
  EXPECT_EQ(c.size(), 0u);
}

TEST(LruCache, PutThenGet) {
  LruCache c(2);
  c.put("a", "1");
  ASSERT_TRUE(c.get("a").has_value());
  EXPECT_EQ(*c.get("a"), "1");
  EXPECT_EQ(c.size(), 1u);
}

TEST(LruCache, PutOverwritesWithoutGrowing) {
  LruCache c(2);
  c.put("a", "1");
  c.put("a", "2");
  EXPECT_EQ(*c.get("a"), "2");
  EXPECT_EQ(c.size(), 1u);
}

TEST(LruCache, EvictsLeastRecentlyUsed) {
  LruCache c(2);
  c.put("a", "1");
  c.put("b", "2");
  c.put("c", "3");
  EXPECT_FALSE(c.get("a").has_value());
  EXPECT_TRUE(c.get("b").has_value());
  EXPECT_TRUE(c.get("c").has_value());
  EXPECT_EQ(c.size(), 2u);
}

TEST(LruCache, GetRefreshesRecency) {
  LruCache c(2);
  c.put("a", "1");
  c.put("b", "2");
  c.get("a");
  c.put("c", "3");
  EXPECT_TRUE(c.get("a").has_value());
  EXPECT_FALSE(c.get("b").has_value());
}

TEST(LruCache, OverwriteRefreshesRecency) {
  LruCache c(2);
  c.put("a", "1");
  c.put("b", "2");
  c.put("a", "1b");
  c.put("c", "3");
  EXPECT_EQ(*c.get("a"), "1b");
  EXPECT_FALSE(c.get("b").has_value());
}

TEST(LruCache, Erase) {
  LruCache c(2);
  c.put("a", "1");
  EXPECT_TRUE(c.erase("a"));
  EXPECT_FALSE(c.erase("a"));
  EXPECT_FALSE(c.get("a").has_value());
  EXPECT_EQ(c.size(), 0u);
  c.put("b", "2");
  c.put("c", "3");
  EXPECT_EQ(c.size(), 2u);  // erase must free the slot
  EXPECT_TRUE(c.get("b").has_value());
}

TEST(LruCache, CapacityOne) {
  LruCache c(1);
  c.put("a", "1");
  c.put("b", "2");
  EXPECT_FALSE(c.get("a").has_value());
  EXPECT_EQ(*c.get("b"), "2");
}

// Model-based test: 50k random operations against a slow but obviously
// correct reference LRU. Any divergence in returned values or size fails.
TEST(LruCache, MatchesReferenceModel) {
  constexpr std::size_t kCap = 64;
  LruCache c(kCap);
  std::list<std::pair<std::string, std::string>> model;  // front = most recent

  auto model_find = [&](const std::string& k) {
    for (auto it = model.begin(); it != model.end(); ++it)
      if (it->first == k) return it;
    return model.end();
  };

  std::mt19937 rng(123);
  std::uniform_int_distribution<int> key_dist(0, 199), op_dist(0, 9);
  for (int i = 0; i < 50'000; ++i) {
    std::string k = "k" + std::to_string(key_dist(rng));
    int op = op_dist(rng);
    if (op < 5) {  // get
      auto it = model_find(k);
      auto got = c.get(k);
      if (it == model.end()) {
        ASSERT_FALSE(got.has_value()) << "op " << i;
      } else {
        ASSERT_TRUE(got.has_value()) << "op " << i;
        ASSERT_EQ(*got, it->second) << "op " << i;
        model.splice(model.begin(), model, it);
      }
    } else if (op < 9) {  // put
      std::string v = "v" + std::to_string(i);
      c.put(k, v);
      auto it = model_find(k);
      if (it != model.end()) model.erase(it);
      model.emplace_front(k, v);
      if (model.size() > kCap) model.pop_back();
    } else {  // erase
      auto it = model_find(k);
      ASSERT_EQ(c.erase(k), it != model.end()) << "op " << i;
      if (it != model.end()) model.erase(it);
    }
    ASSERT_EQ(c.size(), model.size()) << "op " << i;
  }
}
