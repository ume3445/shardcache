#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace shardcache::bench {

// Zipf-distributed key indices in [0, n). s ~ 0.99 is the classic skew for
// cache traffic: a few keys are very hot, most are cold.
class Zipf {
 public:
  Zipf(std::size_t n, double s) : cdf_(n) {
    double sum = 0;
    for (std::size_t i = 0; i < n; ++i) sum += 1.0 / std::pow(static_cast<double>(i + 1), s);
    double acc = 0;
    for (std::size_t i = 0; i < n; ++i) {
      acc += 1.0 / std::pow(static_cast<double>(i + 1), s) / sum;
      cdf_[i] = acc;
    }
    cdf_.back() = 1.0;
  }

  template <typename Rng>
  std::size_t operator()(Rng& rng) const {
    double u = std::uniform_real_distribution<double>(0.0, 1.0)(rng);
    return static_cast<std::size_t>(std::lower_bound(cdf_.begin(), cdf_.end(), u) - cdf_.begin());
  }

 private:
  std::vector<double> cdf_;
};

struct Op {
  std::uint32_t key;
  bool is_read;
};

// Pre-generates each thread's operations so RNG cost stays out of the timing.
// Hot keys are scattered across the keyspace (via a fixed permutation) so the
// hottest keys don't all land next to each other.
inline std::vector<std::vector<Op>> make_ops(std::size_t threads, std::size_t ops_per_thread,
                                              std::size_t keys, double zipf_s, double read_ratio,
                                              std::uint64_t seed) {
  Zipf zipf(keys, zipf_s);
  std::vector<std::uint32_t> perm(keys);
  for (std::size_t i = 0; i < keys; ++i) perm[i] = static_cast<std::uint32_t>(i);
  std::mt19937_64 perm_rng(seed);
  std::shuffle(perm.begin(), perm.end(), perm_rng);

  std::vector<std::vector<Op>> out(threads);
  for (std::size_t t = 0; t < threads; ++t) {
    std::mt19937_64 rng(seed + 1 + t);
    std::bernoulli_distribution read(read_ratio);
    out[t].reserve(ops_per_thread);
    for (std::size_t i = 0; i < ops_per_thread; ++i) out[t].push_back({perm[zipf(rng)], read(rng)});
  }
  return out;
}

inline std::vector<std::string> make_keys(std::size_t n) {
  std::vector<std::string> keys;
  keys.reserve(n);
  for (std::size_t i = 0; i < n; ++i) keys.push_back("key:" + std::to_string(i));
  return keys;
}

inline double percentile(std::vector<std::uint64_t>& v, double p) {
  if (v.empty()) return 0;
  std::size_t idx = std::min(v.size() - 1, static_cast<std::size_t>(p * static_cast<double>(v.size())));
  std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(idx), v.end());
  return static_cast<double>(v[idx]);
}

}  // namespace shardcache::bench
