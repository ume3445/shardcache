#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace shardcache {

// Thread-safe cache interface. The server and the benchmark only talk to
// this, so LockedCache and ShardedCache are interchangeable behind it.
class Cache {
 public:
  virtual ~Cache() = default;

  virtual std::optional<std::string> get(const std::string& key) = 0;
  virtual void put(const std::string& key, std::string value) = 0;
  virtual bool erase(const std::string& key) = 0;  // true if the key existed
  virtual std::size_t size() const = 0;
  virtual std::size_t capacity() const = 0;
};

}  // namespace shardcache
