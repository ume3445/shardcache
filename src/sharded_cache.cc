#include "shardcache/sharded_cache.h"

#include <stdexcept>

namespace shardcache {

// TODO(task 2): implement. See the contract in include/shardcache/sharded_cache.h
// and the tests in tests/cache_concurrency_test.cc.

ShardedCache::ShardedCache(std::size_t /*capacity*/, std::size_t /*num_shards*/) {
  throw std::logic_error("ShardedCache: not implemented yet (docs/TASKS.md, task 2)");
}

ShardedCache::~ShardedCache() = default;

std::optional<std::string> ShardedCache::get(const std::string& /*key*/) {
  throw std::logic_error("ShardedCache::get not implemented");
}

void ShardedCache::put(const std::string& /*key*/, std::string /*value*/) {
  throw std::logic_error("ShardedCache::put not implemented");
}

bool ShardedCache::erase(const std::string& /*key*/) {
  throw std::logic_error("ShardedCache::erase not implemented");
}

std::size_t ShardedCache::size() const {
  throw std::logic_error("ShardedCache::size not implemented");
}

std::size_t ShardedCache::capacity() const {
  throw std::logic_error("ShardedCache::capacity not implemented");
}

std::size_t ShardedCache::num_shards() const {
  throw std::logic_error("ShardedCache::num_shards not implemented");
}

}  // namespace shardcache
