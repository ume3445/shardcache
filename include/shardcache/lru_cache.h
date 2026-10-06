#pragma once

#include <cstddef>
#include <list>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

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
// Design: two structures, each covering what the other can't do in O(1).
//   order_  doubly linked list of (key, value), most recent at the front.
//           Moving or removing a node is O(1) *if you already hold an
//           iterator to it*, but finding a key in a list is O(n).
//   index_  hash map key -> iterator into order_. Finding a key is O(1),
//           but a hash map has no notion of order.
// get() = look up the iterator in index_, splice that node to the front.
// Eviction = the back of order_ is the LRU entry; erase it from both.
class LruCache {
 public:
  explicit LruCache(std::size_t capacity);

  std::optional<std::string> get(const std::string& key);
  void put(const std::string& key, std::string value);
  bool erase(const std::string& key);
  std::size_t size() const { return index_.size(); }
  std::size_t capacity() const { return capacity_; }

 private:
  using Entry = std::pair<std::string, std::string>;  // (key, value)
  using List = std::list<Entry>;

  void touch(List::iterator it);  // move a node to the front (most recent)

  std::size_t capacity_;
  List order_;
  // std::list iterators stay valid when *other* nodes are inserted, erased or
  // spliced, which is what makes storing them in the map safe. The key is
  // stored twice (map + node) on purpose: a string_view into the node would
  // save memory but dangles the moment that node is erased.
  std::unordered_map<std::string, List::iterator> index_;
};

}  // namespace shardcache
