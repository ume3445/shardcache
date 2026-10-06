#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "shardcache/cache.h"

namespace shardcache {

// Line-based text protocol. One request per line ('\n', optional '\r'),
// one response line per request, so clients can pipeline.
//
//   PING              -> PONG
//   GET <key>         -> VALUE <value>   | NOT_FOUND
//   SET <key> <value> -> OK              (value = rest of line, may contain spaces)
//   DEL <key>         -> DELETED         | NOT_FOUND
//   STATS             -> STATS size=<n> capacity=<n>
//   anything invalid  -> ERROR <reason>
//
// Keys: 1..250 bytes, no whitespace (same limits as memcached).
inline constexpr std::size_t kMaxKeyBytes = 250;

struct Request {
  enum class Op { kPing, kGet, kSet, kDel, kStats };
  Op op;
  std::string key;
  std::string value;
};

struct ParseResult {
  std::optional<Request> request;
  std::string error;  // set when request is empty
};

ParseResult parse_request(std::string_view line);

// Parse + execute one line against the cache. Returns the response line
// without the trailing newline.
std::string handle_line(Cache& cache, std::string_view line);

}  // namespace shardcache
