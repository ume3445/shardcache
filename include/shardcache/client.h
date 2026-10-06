#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace shardcache {

// Minimal blocking client, used by the tests and the load generator.
class Client {
 public:
  Client(const std::string& host, std::uint16_t port);  // throws std::runtime_error
  ~Client();
  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;

  std::string request(std::string_view line);  // send one line, return one response line
  void send_raw(std::string_view bytes);        // for pipelining tests
  std::string read_line();                      // throws on disconnect

 private:
  int fd_ = -1;
  std::string buffer_;
};

}  // namespace shardcache
