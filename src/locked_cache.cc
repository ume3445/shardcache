#include "shardcache/locked_cache.h"

namespace shardcache {

LockedCache::LockedCache(std::size_t capacity) : lru_(capacity) {}

std::optional<std::string> LockedCache::get(const std::string& key) {
  std::lock_guard<std::mutex> lock(mu_);
  return lru_.get(key);
}

void LockedCache::put(const std::string& key, std::string value) {
  std::lock_guard<std::mutex> lock(mu_);
  lru_.put(key, std::move(value));
}

bool LockedCache::erase(const std::string& key) {
  std::lock_guard<std::mutex> lock(mu_);
  return lru_.erase(key);
}

std::size_t LockedCache::size() const {
  std::lock_guard<std::mutex> lock(mu_);
  return lru_.size();
}

std::size_t LockedCache::capacity() const { return lru_.capacity(); }

}  // namespace shardcache
