// In-process throughput/latency benchmark: N threads hammer one Cache with a
// Zipf-skewed GET/SET mix. Compares LockedCache (one global mutex) against
// ShardedCache (per-shard mutexes).
//
//   shardcache-bench --threads 1,2,4,8 --impl locked,sharded --csv out.csv

#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "shardcache/locked_cache.h"
#include "shardcache/sharded_cache.h"
#include "workload.h"

using namespace shardcache;
using Clock = std::chrono::steady_clock;

namespace {

struct Config {
  std::vector<std::size_t> threads{1, 2, 4, 8};
  std::vector<std::string> impls{"locked", "sharded"};
  std::size_t ops_per_thread = 1'000'000;
  std::size_t keys = 1'000'000;
  std::size_t capacity = 500'000;
  std::size_t shards = 64;
  double read_ratio = 0.9;
  double zipf = 0.99;
  std::size_t value_bytes = 64;
  std::size_t latency_sample_every = 16;
  int repeats = 3;
  std::string csv;
};

struct Result {
  double mops = 0;
  double p50_ns = 0;
  double p99_ns = 0;
  double hit_rate = 0;
};

std::vector<std::size_t> parse_list(const std::string& s) {
  std::vector<std::size_t> out;
  std::stringstream ss(s);
  for (std::string item; std::getline(ss, item, ',');) out.push_back(std::stoul(item));
  return out;
}

std::vector<std::string> parse_names(const std::string& s) {
  std::vector<std::string> out;
  std::stringstream ss(s);
  for (std::string item; std::getline(ss, item, ',');) out.push_back(item);
  return out;
}

std::unique_ptr<Cache> make_cache(const std::string& impl, const Config& c) {
  if (impl == "locked") return std::make_unique<LockedCache>(c.capacity);
  if (impl == "sharded") return std::make_unique<ShardedCache>(c.capacity, c.shards);
  throw std::invalid_argument("unknown impl " + impl);
}

Result run_once(const std::string& impl, std::size_t threads, const Config& c,
                const std::vector<std::string>& keys, const std::vector<std::vector<bench::Op>>& ops) {
  auto cache = make_cache(impl, c);
  const std::string value(c.value_bytes, 'v');
  for (std::size_t i = 0; i < std::min(c.capacity, keys.size()); ++i) cache->put(keys[i], value);  // warm

  std::vector<std::vector<std::uint64_t>> lat(threads);
  std::vector<std::uint64_t> hits(threads, 0), reads(threads, 0);
  std::atomic<std::size_t> ready{0};
  std::atomic<bool> go{false};

  std::vector<std::thread> pool;
  for (std::size_t t = 0; t < threads; ++t) {
    pool.emplace_back([&, t] {
      auto& my_ops = ops[t];
      auto& my_lat = lat[t];
      my_lat.reserve(my_ops.size() / c.latency_sample_every + 1);
      ++ready;
      while (!go.load(std::memory_order_acquire)) {}
      for (std::size_t i = 0; i < my_ops.size(); ++i) {
        const auto& op = my_ops[i];
        const bool sample = i % c.latency_sample_every == 0;
        Clock::time_point t0;
        if (sample) t0 = Clock::now();
        if (op.is_read) {
          ++reads[t];
          if (cache->get(keys[op.key])) ++hits[t];
        } else {
          cache->put(keys[op.key], value);
        }
        if (sample) my_lat.push_back(static_cast<std::uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - t0).count()));
      }
    });
  }
  while (ready < threads) {}
  auto start = Clock::now();
  go.store(true, std::memory_order_release);
  for (auto& th : pool) th.join();
  double secs = std::chrono::duration<double>(Clock::now() - start).count();

  std::vector<std::uint64_t> all;
  std::uint64_t total_hits = 0, total_reads = 0;
  for (std::size_t t = 0; t < threads; ++t) {
    all.insert(all.end(), lat[t].begin(), lat[t].end());
    total_hits += hits[t];
    total_reads += reads[t];
  }
  Result r;
  r.mops = static_cast<double>(threads * c.ops_per_thread) / secs / 1e6;
  r.p50_ns = bench::percentile(all, 0.50);
  r.p99_ns = bench::percentile(all, 0.99);
  r.hit_rate = total_reads ? static_cast<double>(total_hits) / static_cast<double>(total_reads) : 0;
  return r;
}

}  // namespace

int main(int argc, char** argv) {
  Config c;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto next = [&] { return std::string(argv[++i]); };
    if (a == "--threads") c.threads = parse_list(next());
    else if (a == "--impl") c.impls = parse_names(next());
    else if (a == "--ops") c.ops_per_thread = std::stoul(next());
    else if (a == "--keys") c.keys = std::stoul(next());
    else if (a == "--capacity") c.capacity = std::stoul(next());
    else if (a == "--shards") c.shards = std::stoul(next());
    else if (a == "--read-ratio") c.read_ratio = std::stod(next());
    else if (a == "--zipf") c.zipf = std::stod(next());
    else if (a == "--value-bytes") c.value_bytes = std::stoul(next());
    else if (a == "--repeats") c.repeats = std::stoi(next());
    else if (a == "--csv") c.csv = next();
    else { std::cerr << "unknown flag " << a << "\n"; return 2; }
  }

  std::cout << "keys=" << c.keys << " capacity=" << c.capacity << " shards=" << c.shards
            << " reads=" << c.read_ratio * 100 << "% zipf=" << c.zipf << " value=" << c.value_bytes
            << "B ops/thread=" << c.ops_per_thread << " repeats=" << c.repeats
            << " (median of repeats reported)\n"
            << "hardware threads: " << std::thread::hardware_concurrency() << "\n\n";
  std::cout << std::left << std::setw(9) << "impl" << std::setw(9) << "threads" << std::setw(12) << "Mops/s"
            << std::setw(10) << "p50 ns" << std::setw(10) << "p99 ns" << "hit rate\n";

  std::ofstream csv;
  if (!c.csv.empty()) {
    csv.open(c.csv);
    csv << "impl,threads,mops,p50_ns,p99_ns,hit_rate,keys,capacity,shards,read_ratio,zipf,value_bytes\n";
  }

  const auto keys = bench::make_keys(c.keys);
  std::size_t max_threads = *std::max_element(c.threads.begin(), c.threads.end());
  const auto ops = bench::make_ops(max_threads, c.ops_per_thread, c.keys, c.zipf, c.read_ratio, 42);

  for (const auto& impl : c.impls) {
    for (std::size_t threads : c.threads) {
      std::vector<Result> runs;
      for (int r = 0; r < c.repeats; ++r) runs.push_back(run_once(impl, threads, c, keys, ops));
      std::sort(runs.begin(), runs.end(), [](const Result& a, const Result& b) { return a.mops < b.mops; });
      const Result& m = runs[runs.size() / 2];
      std::cout << std::left << std::setw(9) << impl << std::setw(9) << threads << std::setw(12) << std::fixed
                << std::setprecision(2) << m.mops << std::setw(10) << std::setprecision(0) << m.p50_ns
                << std::setw(10) << m.p99_ns << std::setprecision(3) << m.hit_rate << "\n";
      if (csv) {
        csv << impl << "," << threads << "," << m.mops << "," << m.p50_ns << "," << m.p99_ns << "," << m.hit_rate
            << "," << c.keys << "," << c.capacity << "," << c.shards << "," << c.read_ratio << "," << c.zipf << ","
            << c.value_bytes << "\n";
      }
    }
  }
  return 0;
}
