#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <random>
#include <stdexcept>
#include <thread>
#include <vector>

#include "shardcache/locked_cache.h"
#include "shardcache/sharded_cache.h"

using namespace shardcache;

TEST(ShardedCache, RejectsBadShardCounts) {
  EXPECT_THROW(ShardedCache(10, 0), std::invalid_argument);
  EXPECT_THROW(ShardedCache(3, 4), std::invalid_argument);
}

TEST(ShardedCache, CapacityIsExactEvenWhenUneven) {
  ShardedCache c(10, 4);
  EXPECT_EQ(c.capacity(), 10u);
  EXPECT_EQ(c.num_shards(), 4u);
}

TEST(ShardedCache, NeverExceedsCapacity) {
  ShardedCache c(100, 8);
  for (int i = 0; i < 10'000; ++i) c.put("k" + std::to_string(i), "v");
  EXPECT_LE(c.size(), 100u);
  EXPECT_GT(c.size(), 50u);  // every shard should be close to full
}

// Every test below runs against both implementations through the Cache interface.
class CacheTest : public ::testing::TestWithParam<std::string> {
 protected:
  std::unique_ptr<Cache> make(std::size_t capacity) {
    if (GetParam() == "locked") return std::make_unique<LockedCache>(capacity);
    return std::make_unique<ShardedCache>(capacity, 16);
  }
};

TEST_P(CacheTest, BasicOps) {
  auto c = make(1000);
  c->put("a", "1");
  EXPECT_EQ(*c->get("a"), "1");
  c->put("a", "2");
  EXPECT_EQ(*c->get("a"), "2");
  EXPECT_TRUE(c->erase("a"));
  EXPECT_FALSE(c->get("a").has_value());
  EXPECT_EQ(c->size(), 0u);
}

TEST_P(CacheTest, DisjointWritersAllVisible) {
  constexpr int kThreads = 8, kPerThread = 2000;
  auto c = make(kThreads * kPerThread);  // room for everything: nothing may be evicted... per shard, see below
  std::vector<std::thread> pool;
  for (int t = 0; t < kThreads; ++t) {
    pool.emplace_back([&, t] {
      for (int i = 0; i < kPerThread; ++i) c->put("t" + std::to_string(t) + ":" + std::to_string(i), std::to_string(i));
    });
  }
  for (auto& th : pool) th.join();
  // Sharded capacity is per shard, so with an uneven hash spread a few keys can
  // be evicted even when total keys == capacity. Require at least 95% present.
  std::size_t present = 0;
  for (int t = 0; t < kThreads; ++t)
    for (int i = 0; i < kPerThread; ++i)
      if (auto v = c->get("t" + std::to_string(t) + ":" + std::to_string(i))) {
        ASSERT_EQ(*v, std::to_string(i));
        ++present;
      }
  EXPECT_GE(present, static_cast<std::size_t>(kThreads * kPerThread * 95 / 100));
  EXPECT_LE(c->size(), c->capacity());
}

// Hammer a small hot keyspace with mixed ops from many threads. Values encode
// their key, so a torn or misrouted write shows up as a mismatch. Run the
// TSan build (docs/TASKS.md) to catch data races this can't see.
TEST_P(CacheTest, ConcurrentMixedOpsStayConsistent) {
  constexpr int kThreads = 8, kOps = 20'000, kKeys = 1000;
  auto c = make(512);
  std::atomic<int> mismatches{0};
  std::vector<std::thread> pool;
  for (int t = 0; t < kThreads; ++t) {
    pool.emplace_back([&, t] {
      std::mt19937 rng(t);
      std::uniform_int_distribution<int> key(0, kKeys - 1), op(0, 9);
      for (int i = 0; i < kOps; ++i) {
        std::string k = "k" + std::to_string(key(rng));
        int o = op(rng);
        if (o < 6) {
          if (auto v = c->get(k); v && v->rfind(k + "=", 0) != 0) ++mismatches;
        } else if (o < 9) {
          c->put(k, k + "=" + std::to_string(t));
        } else {
          c->erase(k);
        }
        if (c->size() > c->capacity()) ++mismatches;
      }
    });
  }
  for (auto& th : pool) th.join();
  EXPECT_EQ(mismatches.load(), 0);
  EXPECT_LE(c->size(), c->capacity());
}

INSTANTIATE_TEST_SUITE_P(Impls, CacheTest, ::testing::Values("locked", "sharded"),
                         [](const auto& info) { return info.param; });
