#include "shardcache/lru_cache.h"

#include <stdexcept>

namespace shardcache {

LruCache::LruCache(std::size_t capacity) : capacity_(capacity) {
  if (capacity == 0) throw std::invalid_argument("LruCache capacity must be > 0");
  // Size the hash table up front so it never rehashes mid-run; a rehash is an
  // O(n) pause that would show up as a latency spike while holding the lock.
  index_.reserve(capacity + 1);
}

void LruCache::touch(List::iterator it) {
  // splice relinks the node at the front: no allocation, no copy, and every
  // iterator (including the one stored in index_) stays valid.
  order_.splice(order_.begin(), order_, it);
}

std::optional<std::string> LruCache::get(const std::string& key) {
  auto found = index_.find(key);
  if (found == index_.end()) return std::nullopt;
  touch(found->second);
  // Return a copy: handing out a reference would let the caller read the
  // value after the lock protecting this cache has been released.
  return found->second->second;
}

void LruCache::put(const std::string& key, std::string value) {
  auto found = index_.find(key);
  if (found != index_.end()) {  // update in place, refresh recency
    found->second->second = std::move(value);
    touch(found->second);
    return;
  }

  order_.emplace_front(key, std::move(value));
  index_.emplace(key, order_.begin());

  if (index_.size() > capacity_) {  // evict the least recently used entry
    const std::string& victim = order_.back().first;
    index_.erase(victim);  // erase from the map first: `victim` refers into the node
    order_.pop_back();
  }
}

bool LruCache::erase(const std::string& key) {
  auto found = index_.find(key);
  if (found == index_.end()) return false;
  order_.erase(found->second);
  index_.erase(found);
  return true;
}

}  // namespace shardcache
