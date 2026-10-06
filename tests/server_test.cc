#include "shardcache/server.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "fake_cache.h"
#include "shardcache/client.h"

using namespace shardcache;

class ServerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ServerOptions opts;
    opts.port = 0;  // ephemeral
    opts.max_line_bytes = 4096;
    server_ = std::make_unique<Server>(cache_, opts);
    server_->start();
  }
  void TearDown() override { server_->stop(); }

  FakeCache cache_;
  std::unique_ptr<Server> server_;
};

TEST_F(ServerTest, RequestResponse) {
  Client c("127.0.0.1", server_->port());
  EXPECT_EQ(c.request("PING"), "PONG");
  EXPECT_EQ(c.request("SET user:1 Umer Hammad"), "OK");
  EXPECT_EQ(c.request("GET user:1"), "VALUE Umer Hammad");
  EXPECT_EQ(c.request("DEL user:1"), "DELETED");
  EXPECT_EQ(c.request("GET user:1"), "NOT_FOUND");
  EXPECT_EQ(c.request("BOGUS"), "ERROR unknown command");
}

TEST_F(ServerTest, PipelinedRequestsAnsweredInOrder) {
  Client c("127.0.0.1", server_->port());
  c.send_raw("SET a 1\nSET b 2\r\nGET a\nGET b\nGET c\n");
  EXPECT_EQ(c.read_line(), "OK");
  EXPECT_EQ(c.read_line(), "OK");
  EXPECT_EQ(c.read_line(), "VALUE 1");
  EXPECT_EQ(c.read_line(), "VALUE 2");
  EXPECT_EQ(c.read_line(), "NOT_FOUND");
}

TEST_F(ServerTest, RequestSplitAcrossPackets) {
  Client c("127.0.0.1", server_->port());
  c.send_raw("SET spl");
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  c.send_raw("it value\nGET split\n");
  EXPECT_EQ(c.read_line(), "OK");
  EXPECT_EQ(c.read_line(), "VALUE value");
}

TEST_F(ServerTest, OverlongLineIsRejectedAndClosed) {
  Client c("127.0.0.1", server_->port());
  c.send_raw("SET k " + std::string(10'000, 'x'));  // no newline, over the 4096 limit
  EXPECT_EQ(c.read_line(), "ERROR line too long");
  EXPECT_THROW(c.read_line(), std::runtime_error);
}

TEST_F(ServerTest, ManyConcurrentClients) {
  constexpr int kClients = 16, kOps = 300;
  std::vector<std::thread> pool;
  std::atomic<int> failures{0};
  for (int t = 0; t < kClients; ++t) {
    pool.emplace_back([&, t] {
      Client c("127.0.0.1", server_->port());
      for (int i = 0; i < kOps; ++i) {
        std::string k = "c" + std::to_string(t) + ":" + std::to_string(i);
        if (c.request("SET " + k + " " + k) != "OK") ++failures;
        if (c.request("GET " + k) != "VALUE " + k) ++failures;
      }
    });
  }
  for (auto& th : pool) th.join();
  EXPECT_EQ(failures.load(), 0);
  EXPECT_EQ(cache_.size(), static_cast<std::size_t>(kClients * kOps));
}

TEST_F(ServerTest, StopClosesIdleConnectionsPromptly) {
  Client c("127.0.0.1", server_->port());
  EXPECT_EQ(c.request("PING"), "PONG");
  auto t0 = std::chrono::steady_clock::now();
  server_->stop();
  EXPECT_LT(std::chrono::steady_clock::now() - t0, std::chrono::seconds(2));
  EXPECT_THROW(c.read_line(), std::runtime_error);
}
