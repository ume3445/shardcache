#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>

#include "shardcache/cache.h"

namespace shardcache {

struct ServerOptions {
  std::string host = "127.0.0.1";
  std::uint16_t port = 7070;          // 0 = pick a free port (see Server::port())
  std::size_t max_line_bytes = 1 << 20;
};

// TCP server speaking the protocol in protocol.h. One acceptor thread plus
// one thread per connection: simple and portable (macOS + Linux), and fine
// for hundreds of connections. Responses to pipelined requests are batched
// into a single send().
class Server {
 public:
  Server(Cache& cache, ServerOptions options);
  ~Server();
  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  void start();  // bind + listen + spawn acceptor; throws std::runtime_error on failure
  void stop();   // stop accepting, close live connections, wait for workers
  std::uint16_t port() const { return port_; }

 private:
  void accept_loop();
  void serve(int fd);

  Cache& cache_;
  ServerOptions options_;
  std::uint16_t port_ = 0;
  int listen_fd_ = -1;
  std::atomic<bool> running_{false};
  std::thread acceptor_;

  std::mutex conns_mu_;
  std::condition_variable conns_cv_;
  std::unordered_set<int> conns_;
  std::size_t active_workers_ = 0;
};

}  // namespace shardcache
