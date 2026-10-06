#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <vector>

#include "shardcache/cache.h"
#include "shardcache/lru_cache.h"

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
//   - eviction is LRU *within a shard* (approximate global LRU)
//
// Why it scales: with one global lock (LockedCache), T threads take turns,
// so throughput is capped at what one core does. With N shards, two
// operations only wait on each other if their keys hash to the same shard,
// roughly a 1/N chance for uniform keys. Under heavy skew this weakens: the
// hottest key lives in exactly one shard, so that shard's lock becomes the
// bottleneck no matter how many shards exist (see docs/DESIGN.md).
class ShardedCache final : public Cache {
 public:
  ShardedCache(std::size_t capacity, std::size_t num_shards);
  ~ShardedCache() override;

  std::optional<std::string> get(const std::string& key) override;
  void put(const std::string& key, std::string value) override;
  bool erase(const std::string& key) override;
  std::size_t size() const override;
  std::size_t capacity() const override { return capacity_; }
  std::size_t num_shards() const { return shards_.size(); }

 private:
  // alignas(64): each shard starts on its own 64-byte cache line. Without it,
  // two shards' mutexes can share a line, and every lock/unlock on one shard
  // invalidates that line in the other cores' caches ("false sharing"): the
  // shards would contend in hardware even though they share no data.
  struct alignas(64) Shard {
    explicit Shard(std::size_t cap) : lru(cap) {}
    mutable std::mutex mu;
    LruCache lru;
  };

  Shard& shard_for(const std::string& key);
  const Shard& shard_for(const std::string& key) const;

  std::size_t capacity_;
  // unique_ptr because std::mutex can't be copied or moved, and
  // std::vector<Shard> needs to move elements when it grows. Heap-allocating
  // each shard also keeps them on separate cache lines.
  std::vector<std::unique_ptr<Shard>> shards_;
};

}  // namespace shardcache
