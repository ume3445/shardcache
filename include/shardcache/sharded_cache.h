#pragma once

#include <cstddef>

#include "shardcache/cache.h"

namespace shardcache {

// N independent LRU shards, each with its own lock. A key always maps to the
// same shard, so operations on different shards never contend.
//
// Contract (enforced by tests/cache_concurrency_test.cc):
//   - num_shards must be > 0 and <= capacity, else std::invalid_argument
//   - capacity() == the capacity passed in, exactly: split it as evenly as
//     possible across shards (some shards may get one more slot)
//   - all operations are safe to call from any number of threads
//   - size() <= capacity() at all times
//   - eviction is LRU *within a shard* (approximate global LRU; be ready to
//     explain why that trade-off is acceptable)
//
// YOUR TASK (docs/TASKS.md, task 2): add private members and implement
// src/sharded_cache.cc.
class ShardedCache final : public Cache {
 public:
  ShardedCache(std::size_t capacity, std::size_t num_shards);
  ~ShardedCache() override;

  std::optional<std::string> get(const std::string& key) override;
  void put(const std::string& key, std::string value) override;
  bool erase(const std::string& key) override;
  std::size_t size() const override;
  std::size_t capacity() const override;
  std::size_t num_shards() const;

 private:
  // TODO(task 2): your members here.
};

}  // namespace shardcache
