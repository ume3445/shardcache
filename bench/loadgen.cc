// Network load generator: C client connections, each a thread doing blocking
// request/response over TCP against a running shardcache-server.
//
//   shardcache-server --impl sharded &
//   shardcache-loadgen --connections 32 --requests 50000

#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "shardcache/client.h"
#include "workload.h"

using Clock = std::chrono::steady_clock;

int main(int argc, char** argv) {
  std::string host = "127.0.0.1";
  std::uint16_t port = 7070;
  std::size_t connections = 32, requests = 50'000, keys = 100'000, value_bytes = 64;
  double read_ratio = 0.9, zipf = 0.99;

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto next = [&] { return std::string(argv[++i]); };
    if (a == "--host") host = next();
    else if (a == "--port") port = static_cast<std::uint16_t>(std::stoi(next()));
    else if (a == "--connections") connections = std::stoul(next());
    else if (a == "--requests") requests = std::stoul(next());
    else if (a == "--keys") keys = std::stoul(next());
    else if (a == "--read-ratio") read_ratio = std::stod(next());
    else if (a == "--zipf") zipf = std::stod(next());
    else if (a == "--value-bytes") value_bytes = std::stoul(next());
    else { std::cerr << "unknown flag " << a << "\n"; return 2; }
  }

  const auto key_names = shardcache::bench::make_keys(keys);
  const auto ops = shardcache::bench::make_ops(connections, requests, keys, zipf, read_ratio, 7);
  const std::string value(value_bytes, 'v');

  {  // preload so reads mostly hit
    shardcache::Client c(host, port);
    for (const auto& k : key_names) c.request("SET " + k + " " + value);
  }

  std::vector<std::vector<std::uint64_t>> lat(connections);
  std::atomic<std::size_t> errors{0};
  std::atomic<bool> go{false};
  std::vector<std::thread> pool;
  for (std::size_t t = 0; t < connections; ++t) {
    pool.emplace_back([&, t] {
      shardcache::Client c(host, port);
      lat[t].reserve(requests);
      while (!go.load()) {}
      for (const auto& op : ops[t]) {
        auto t0 = Clock::now();
        std::string resp = op.is_read ? c.request("GET " + key_names[op.key])
                                      : c.request("SET " + key_names[op.key] + " " + value);
        lat[t].push_back(static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - t0).count()));
        if (resp.rfind("ERROR", 0) == 0) ++errors;
      }
    });
  }
  auto start = Clock::now();
  go = true;
  for (auto& th : pool) th.join();
  double secs = std::chrono::duration<double>(Clock::now() - start).count();

  std::vector<std::uint64_t> all;
  for (auto& v : lat) all.insert(all.end(), v.begin(), v.end());
  double total = static_cast<double>(connections * requests);
  std::cout << std::fixed << std::setprecision(0) << "connections=" << connections << " requests=" << total
            << " throughput=" << total / secs << " req/s"
            << std::setprecision(1) << " p50=" << shardcache::bench::percentile(all, 0.50) / 1000.0 << "us"
            << " p99=" << shardcache::bench::percentile(all, 0.99) / 1000.0 << "us"
            << " errors=" << errors.load() << "\n";
  return errors ? 1 : 0;
}
