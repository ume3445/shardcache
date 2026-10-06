#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace shardcache {

// Fixed-capacity least-recently-used cache. NOT thread-safe: callers
// (LockedCache, ShardedCache) are responsible for synchronization.
//
// Contract (enforced by tests/lru_cache_test.cc):
//   - capacity must be > 0, otherwise the constructor throws std::invalid_argument
//   - get(), put(), erase() are O(1) average time
//   - get() of a present key marks it most recently used
//   - put() of a new key inserts it as most recently used; if that makes
//     size() > capacity(), the least recently used entry is evicted
//   - put() of an existing key replaces the value and marks it most recently used
//   - size() <= capacity() at all times
//
// YOUR TASK (docs/TASKS.md, task 1): choose the data structures, add them as
// private members below, and implement src/lru_cache.cc.
class LruCache {
 public:
  explicit LruCache(std::size_t capacity);

  std::optional<std::string> get(const std::string& key);
  void put(const std::string& key, std::string value);
  bool erase(const std::string& key);
  std::size_t size() const;
  std::size_t capacity() const { return capacity_; }

 private:
  std::size_t capacity_;
  // TODO(task 1): your members here.
};

}  // namespace shardcache
