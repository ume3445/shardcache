#include "shardcache/protocol.h"

#include <gtest/gtest.h>

#include "fake_cache.h"

using namespace shardcache;

TEST(Protocol, ParsesCommands) {
  auto get = parse_request("GET user:1");
  ASSERT_TRUE(get.request);
  EXPECT_EQ(get.request->op, Request::Op::kGet);
  EXPECT_EQ(get.request->key, "user:1");

  auto set = parse_request("SET k hello world\r");
  ASSERT_TRUE(set.request);
  EXPECT_EQ(set.request->op, Request::Op::kSet);
  EXPECT_EQ(set.request->key, "k");
  EXPECT_EQ(set.request->value, "hello world");

  auto empty_value = parse_request("SET k ");
  ASSERT_TRUE(empty_value.request);
  EXPECT_EQ(empty_value.request->value, "");

  EXPECT_TRUE(parse_request("PING").request);
  EXPECT_TRUE(parse_request("STATS").request);
  EXPECT_TRUE(parse_request("DEL k").request);
}

TEST(Protocol, RejectsMalformed) {
  for (const char* bad : {"", "\r", "FOO k", "GET", "GET ", "GET  k", "GET a b", "GET a ", "DEL", "SET",
                          "SET k", "PING extra", "get k"}) {
    auto r = parse_request(bad);
    EXPECT_FALSE(r.request) << "accepted: '" << bad << "'";
    EXPECT_FALSE(r.error.empty()) << bad;
  }
  EXPECT_FALSE(parse_request("GET " + std::string(kMaxKeyBytes + 1, 'x')).request);
  EXPECT_TRUE(parse_request("GET " + std::string(kMaxKeyBytes, 'x')).request);
}

TEST(Protocol, HandleLineRoundTrip) {
  FakeCache c;
  EXPECT_EQ(handle_line(c, "PING"), "PONG");
  EXPECT_EQ(handle_line(c, "GET a"), "NOT_FOUND");
  EXPECT_EQ(handle_line(c, "SET a hello there"), "OK");
  EXPECT_EQ(handle_line(c, "GET a"), "VALUE hello there");
  EXPECT_EQ(handle_line(c, "STATS"), "STATS size=1 capacity=1000");
  EXPECT_EQ(handle_line(c, "DEL a"), "DELETED");
  EXPECT_EQ(handle_line(c, "DEL a"), "NOT_FOUND");
  EXPECT_EQ(handle_line(c, "NOPE"), "ERROR unknown command");
}
