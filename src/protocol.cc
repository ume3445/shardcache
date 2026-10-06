#include "shardcache/protocol.h"

namespace shardcache {
namespace {

bool is_space(char c) { return c == ' ' || c == '\t'; }

// Splits off the next whitespace-delimited token; `rest` keeps what follows
// the single separating space.
std::string_view next_token(std::string_view in, std::string_view& rest) {
  std::size_t end = 0;
  while (end < in.size() && !is_space(in[end])) ++end;
  std::string_view token = in.substr(0, end);
  rest = end < in.size() ? in.substr(end + 1) : std::string_view{};
  return token;
}

ParseResult error(std::string msg) { return ParseResult{std::nullopt, std::move(msg)}; }

std::optional<std::string> validate_key(std::string_view key) {
  if (key.empty()) return "missing key";
  if (key.size() > kMaxKeyBytes) return "key too long";
  return std::nullopt;
}

}  // namespace

ParseResult parse_request(std::string_view line) {
  if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
  if (line.empty()) return error("empty request");

  std::string_view rest;
  std::string_view cmd = next_token(line, rest);

  if (cmd == "PING" || cmd == "STATS") {
    if (!rest.empty()) return error("unexpected arguments");
    return ParseResult{Request{cmd == "PING" ? Request::Op::kPing : Request::Op::kStats, {}, {}}, {}};
  }

  if (cmd == "GET" || cmd == "DEL") {
    std::string_view after_key;
    std::string_view key = next_token(rest, after_key);
    if (auto err = validate_key(key)) return error(*err);
    if (!after_key.empty() || rest.size() > key.size()) return error("unexpected arguments");
    return ParseResult{Request{cmd == "GET" ? Request::Op::kGet : Request::Op::kDel, std::string(key), {}}, {}};
  }

  if (cmd == "SET") {
    std::string_view value;
    std::string_view key = next_token(rest, value);
    if (auto err = validate_key(key)) return error(*err);
    if (rest.size() == key.size()) return error("missing value");  // no separator after key
    return ParseResult{Request{Request::Op::kSet, std::string(key), std::string(value)}, {}};
  }

  return error("unknown command");
}

std::string handle_line(Cache& cache, std::string_view line) {
  ParseResult parsed = parse_request(line);
  if (!parsed.request) return "ERROR " + parsed.error;
  Request& req = *parsed.request;

  switch (req.op) {
    case Request::Op::kPing:
      return "PONG";
    case Request::Op::kGet: {
      auto value = cache.get(req.key);
      return value ? "VALUE " + *value : "NOT_FOUND";
    }
    case Request::Op::kSet:
      cache.put(req.key, std::move(req.value));
      return "OK";
    case Request::Op::kDel:
      return cache.erase(req.key) ? "DELETED" : "NOT_FOUND";
    case Request::Op::kStats:
      return "STATS size=" + std::to_string(cache.size()) + " capacity=" + std::to_string(cache.capacity());
  }
  return "ERROR internal";
}

}  // namespace shardcache
