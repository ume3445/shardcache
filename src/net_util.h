#pragma once

#include <sys/socket.h>
#include <sys/types.h>

#include <cerrno>
#include <string_view>

namespace shardcache::net {

// Avoid SIGPIPE when the peer has gone away: MSG_NOSIGNAL on Linux,
// SO_NOSIGPIPE (set per socket) on macOS.
#ifdef MSG_NOSIGNAL
inline constexpr int kSendFlags = MSG_NOSIGNAL;
#else
inline constexpr int kSendFlags = 0;
#endif

inline void configure_socket(int fd) {
#ifdef SO_NOSIGPIPE
  int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#else
  (void)fd;
#endif
}

inline bool send_all(int fd, std::string_view data) {
  while (!data.empty()) {
    ssize_t n = ::send(fd, data.data(), data.size(), kSendFlags);
    if (n < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    data.remove_prefix(static_cast<std::size_t>(n));
  }
  return true;
}

}  // namespace shardcache::net
