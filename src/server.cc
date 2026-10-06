#include "shardcache/server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>

#include "net_util.h"
#include "shardcache/protocol.h"

namespace shardcache {

Server::Server(Cache& cache, ServerOptions options) : cache_(cache), options_(std::move(options)) {}

Server::~Server() { stop(); }

void Server::start() {
  listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) throw std::runtime_error(std::string("socket: ") + std::strerror(errno));

  int one = 1;
  setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(options_.port);
  if (::inet_pton(AF_INET, options_.host.c_str(), &addr.sin_addr) != 1) {
    ::close(listen_fd_);
    throw std::runtime_error("invalid host: " + options_.host);
  }
  if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 ||
      ::listen(listen_fd_, 512) < 0) {
    std::string err = std::strerror(errno);
    ::close(listen_fd_);
    throw std::runtime_error("bind/listen on " + options_.host + ":" + std::to_string(options_.port) + ": " + err);
  }

  socklen_t len = sizeof(addr);
  ::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &len);
  port_ = ntohs(addr.sin_port);

  running_ = true;
  acceptor_ = std::thread([this] { accept_loop(); });
}

void Server::stop() {
  if (!running_.exchange(false)) return;
  if (acceptor_.joinable()) acceptor_.join();
  ::close(listen_fd_);
  listen_fd_ = -1;

  // Wake every worker blocked in recv(), then wait for all of them to exit.
  std::unique_lock<std::mutex> lock(conns_mu_);
  for (int fd : conns_) ::shutdown(fd, SHUT_RDWR);
  conns_cv_.wait(lock, [this] { return active_workers_ == 0; });
}

void Server::accept_loop() {
  // poll() with a timeout instead of a bare accept(): closing a listening
  // socket from another thread does not reliably wake accept() on macOS.
  pollfd pfd{listen_fd_, POLLIN, 0};
  while (running_) {
    int ready = ::poll(&pfd, 1, 100);
    if (ready <= 0) continue;
    int fd = ::accept(listen_fd_, nullptr, nullptr);
    if (fd < 0) continue;

    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));  // small request/response messages
    net::configure_socket(fd);

    {
      std::lock_guard<std::mutex> lock(conns_mu_);
      conns_.insert(fd);
      ++active_workers_;
    }
    std::thread([this, fd] { serve(fd); }).detach();
  }
}

void Server::serve(int fd) {
  std::string inbuf;
  std::string outbuf;
  char chunk[64 * 1024];

  while (true) {
    ssize_t n = ::recv(fd, chunk, sizeof(chunk), 0);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) break;
    inbuf.append(chunk, static_cast<std::size_t>(n));

    // Answer every complete line we have, then flush once.
    std::size_t start = 0;
    for (std::size_t nl; (nl = inbuf.find('\n', start)) != std::string::npos; start = nl + 1) {
      outbuf += handle_line(cache_, std::string_view(inbuf).substr(start, nl - start));
      outbuf += '\n';
    }
    inbuf.erase(0, start);

    bool too_long = inbuf.size() > options_.max_line_bytes;
    if (too_long) outbuf += "ERROR line too long\n";
    if (!outbuf.empty() && !net::send_all(fd, outbuf)) break;
    outbuf.clear();
    if (too_long) break;
  }

  ::close(fd);
  std::lock_guard<std::mutex> lock(conns_mu_);
  conns_.erase(fd);
  --active_workers_;
  conns_cv_.notify_all();
}

}  // namespace shardcache
