#include "shardcache/sharded_cache.h"

#include <cstdint>
#include <functional>
#include <stdexcept>

namespace shardcache {
namespace {

// std::hash<std::string> is fine on its own, but its low bits are not
// guaranteed to be well mixed on every standard library. One multiply by a
// large odd constant (Fibonacci hashing) spreads all input bits into the high
// bits, and we take the shard from those.
std::size_t shard_index(const std::string& key, std::size_t num_shards) {
  std::uint64_t h = std::hash<std::string>{}(key);
  h *= 0x9E3779B97F4A7C15ull;
  return static_cast<std::size_t>((h >> 32) % num_shards);
}

}  // namespace

ShardedCache::ShardedCache(std::size_t capacity, std::size_t num_shards) : capacity_(capacity) {
  if (num_shards == 0) throw std::invalid_argument("num_shards must be > 0");
  if (num_shards > capacity) throw std::invalid_argument("num_shards must be <= capacity");

  // 10 slots over 4 shards -> 3, 3, 2, 2: the first (capacity % n) shards get
  // one extra slot, so the total is exactly `capacity`.
  const std::size_t base = capacity / num_shards;
  const std::size_t extra = capacity % num_shards;
  shards_.reserve(num_shards);
  for (std::size_t i = 0; i < num_shards; ++i) {
    shards_.push_back(std::make_unique<Shard>(base + (i < extra ? 1 : 0)));
  }
}

ShardedCache::~ShardedCache() = default;

ShardedCache::Shard& ShardedCache::shard_for(const std::string& key) {
  return *shards_[shard_index(key, shards_.size())];
}

const ShardedCache::Shard& ShardedCache::shard_for(const std::string& key) const {
  return *shards_[shard_index(key, shards_.size())];
}

// Every operation: hash the key (no lock needed, shards_ never changes after
// construction), then hold only that shard's lock for the O(1) LRU work.
// get() needs an exclusive lock too, because a hit reorders the LRU list.

std::optional<std::string> ShardedCache::get(const std::string& key) {
  Shard& s = shard_for(key);
  std::lock_guard<std::mutex> lock(s.mu);
  return s.lru.get(key);
}

void ShardedCache::put(const std::string& key, std::string value) {
  Shard& s = shard_for(key);
  std::lock_guard<std::mutex> lock(s.mu);
  s.lru.put(key, std::move(value));
}

bool ShardedCache::erase(const std::string& key) {
  Shard& s = shard_for(key);
  std::lock_guard<std::mutex> lock(s.mu);
  return s.lru.erase(key);
}

std::size_t ShardedCache::size() const {
  // Locks one shard at a time, never all at once, so this is not an atomic
  // snapshot: other threads can change shards we've already counted. Each
  // shard's count is exact and each is <= its own capacity, so the sum is
  // always <= capacity(), which is all STATS needs. Locking every shard at
  // once would stall the whole cache for a monitoring call.
  std::size_t total = 0;
  for (const auto& shard : shards_) {
    std::lock_guard<std::mutex> lock(shard->mu);
    total += shard->lru.size();
  }
  return total;
}

}  // namespace shardcache
