#include "shardcache/client.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>

#include "net_util.h"

namespace shardcache {

Client::Client(const std::string& host, std::uint16_t port) {
  fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd_ < 0) throw std::runtime_error(std::string("socket: ") + std::strerror(errno));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1 ||
      ::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    std::string err = std::strerror(errno);
    ::close(fd_);
    throw std::runtime_error("connect " + host + ":" + std::to_string(port) + ": " + err);
  }
  int one = 1;
  setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
  net::configure_socket(fd_);
}

Client::~Client() {
  if (fd_ >= 0) ::close(fd_);
}

std::string Client::request(std::string_view line) {
  std::string msg(line);
  msg += '\n';
  send_raw(msg);
  return read_line();
}

void Client::send_raw(std::string_view bytes) {
  if (!net::send_all(fd_, bytes)) throw std::runtime_error("send failed");
}

std::string Client::read_line() {
  char chunk[64 * 1024];
  while (true) {
    std::size_t nl = buffer_.find('\n');
    if (nl != std::string::npos) {
      std::string line = buffer_.substr(0, nl);
      buffer_.erase(0, nl + 1);
      return line;
    }
    ssize_t n = ::recv(fd_, chunk, sizeof(chunk), 0);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) throw std::runtime_error("connection closed");
    buffer_.append(chunk, static_cast<std::size_t>(n));
  }
}

}  // namespace shardcache
