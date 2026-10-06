#pragma once

#include <map>
#include <mutex>

#include "shardcache/cache.h"

// Unbounded map-backed Cache so protocol/server tests don't depend on the
// LRU/sharded implementations.
class FakeCache final : public shardcache::Cache {
 public:
  std::optional<std::string> get(const std::string& key) override {
    std::lock_guard<std::mutex> l(mu_);
    auto it = m_.find(key);
    if (it == m_.end()) return std::nullopt;
    return it->second;
  }
  void put(const std::string& key, std::string value) override {
    std::lock_guard<std::mutex> l(mu_);
    m_[key] = std::move(value);
  }
  bool erase(const std::string& key) override {
    std::lock_guard<std::mutex> l(mu_);
    return m_.erase(key) > 0;
  }
  std::size_t size() const override {
    std::lock_guard<std::mutex> l(mu_);
    return m_.size();
  }
  std::size_t capacity() const override { return 1000; }

 private:
  mutable std::mutex mu_;
  std::map<std::string, std::string> m_;
};
