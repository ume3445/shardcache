#include "shardcache/lru_cache.h"

#include <stdexcept>

namespace shardcache {

// TODO(task 1): implement. See the contract in include/shardcache/lru_cache.h
// and the tests in tests/lru_cache_test.cc.

LruCache::LruCache(std::size_t capacity) : capacity_(capacity) {
  throw std::logic_error("LruCache: not implemented yet (docs/TASKS.md, task 1)");
}

std::optional<std::string> LruCache::get(const std::string& /*key*/) {
  throw std::logic_error("LruCache::get not implemented");
}

void LruCache::put(const std::string& /*key*/, std::string /*value*/) {
  throw std::logic_error("LruCache::put not implemented");
}

bool LruCache::erase(const std::string& /*key*/) {
  throw std::logic_error("LruCache::erase not implemented");
}

std::size_t LruCache::size() const {
  throw std::logic_error("LruCache::size not implemented");
}

}  // namespace shardcache
