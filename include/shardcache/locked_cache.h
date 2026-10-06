#pragma once

#include <mutex>

#include "shardcache/cache.h"
#include "shardcache/lru_cache.h"

namespace shardcache {

// Baseline: one LruCache behind one global mutex. Every operation, including
// get(), takes the same exclusive lock, so threads serialize. This is the
// number ShardedCache has to beat.
class LockedCache final : public Cache {
 public:
  explicit LockedCache(std::size_t capacity);

  std::optional<std::string> get(const std::string& key) override;
  void put(const std::string& key, std::string value) override;
  bool erase(const std::string& key) override;
  std::size_t size() const override;
  std::size_t capacity() const override;

 private:
  mutable std::mutex mu_;
  LruCache lru_;
};

}  // namespace shardcache
