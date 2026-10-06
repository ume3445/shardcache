#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "shardcache/locked_cache.h"
#include "shardcache/server.h"
#include "shardcache/sharded_cache.h"

namespace {
std::atomic<bool> g_stop{false};
void on_signal(int) { g_stop = true; }

void usage() {
  std::cerr << "usage: shardcache-server [--host 127.0.0.1] [--port 7070] [--capacity 100000]\n"
               "                         [--impl sharded|locked] [--shards 64]\n";
}
}  // namespace

int main(int argc, char** argv) {
  shardcache::ServerOptions opts;
  std::size_t capacity = 100000;
  std::size_t shards = 64;
  std::string impl = "sharded";

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    auto next = [&]() -> std::string {
      if (i + 1 >= argc) { usage(); std::exit(2); }
      return argv[++i];
    };
    if (arg == "--host") opts.host = next();
    else if (arg == "--port") opts.port = static_cast<std::uint16_t>(std::stoi(next()));
    else if (arg == "--capacity") capacity = std::stoul(next());
    else if (arg == "--shards") shards = std::stoul(next());
    else if (arg == "--impl") impl = next();
    else { usage(); return 2; }
  }

  std::unique_ptr<shardcache::Cache> cache;
  if (impl == "sharded") cache = std::make_unique<shardcache::ShardedCache>(capacity, shards);
  else if (impl == "locked") cache = std::make_unique<shardcache::LockedCache>(capacity);
  else { usage(); return 2; }

  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  shardcache::Server server(*cache, opts);
  server.start();
  std::cout << "shardcache listening on " << opts.host << ":" << server.port() << " (" << impl
            << ", capacity " << capacity << (impl == "sharded" ? ", shards " + std::to_string(shards) : "")
            << ")" << std::endl;

  while (!g_stop) std::this_thread::sleep_for(std::chrono::milliseconds(100));
  server.stop();
  std::cout << "stopped" << std::endl;
  return 0;
}
