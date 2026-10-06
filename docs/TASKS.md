# Your tasks

Everything around the cache is built: CMake, the TCP server and protocol, a
client, the benchmark, the load generator, and 28 tests. What's left is the
part an interviewer will ask about, so it's yours to write. Use Claude or
anyone else to *review* your code and explain concepts, not to write it. If you
can't explain a line, it shouldn't be on your resume.

## 0. Setup (macOS)

```bash
xcode-select --install          # compiler, if you don't have it
brew install cmake ninja
chmod +x scripts/run_bench.sh
cmake -S . -B build -G Ninja && cmake --build build
./build/shardcache-tests        # protocol + server tests pass now; cache tests fail until you finish 1 and 2
```

## 1. LruCache (`include/shardcache/lru_cache.h`, `src/lru_cache.cc`)

Contract is in the header. get / put / erase must be O(1).

Hints, read only if stuck:
- Two structures: something that finds a key in O(1), and something that
  keeps recency order and can move or remove any element in O(1).
- `std::list::splice` moves a node without invalidating iterators.
- Watch out: storing a `std::string_view` key in the map that points into a
  list node is a classic dangling-reference bug. Store the key, or think hard.

Done when: `./build/shardcache-tests --gtest_filter='LruCache*'` passes,
including `MatchesReferenceModel` (50,000 random ops against a reference).

## 2. ShardedCache (`include/shardcache/sharded_cache.h`, `src/sharded_cache.cc`)

Contract is in the header. Each shard = one LruCache + one mutex.

Hints:
- `std::mutex` can't be copied or moved, so a `std::vector<Shard>` won't
  work directly. Why? What are your options?
- Pick the shard with a hash of the key. Is `std::hash<std::string>{}(key) % n` good enough?
- Split the capacity exactly: 10 slots over 4 shards = 3, 3, 2, 2.
- Two shards' mutexes sitting on the same 64-byte cache line will slow each
  other down even though they never share data. Look up "false sharing" and `alignas(64)`.

Done when every test passes, and the ThreadSanitizer build is clean:

```bash
cmake -S . -B build-tsan -G Ninja -DSHARDCACHE_TSAN=ON && cmake --build build-tsan
./build-tsan/shardcache-tests      # must print no "WARNING: ThreadSanitizer"
```

## 3. Benchmark and experiments

```bash
./scripts/run_bench.sh            # writes bench/results/RESULTS.md with a headline line
```

Then run these experiments and write one sentence on each in docs/DESIGN.md:

| Experiment | Command | Question |
|---|---|---|
| Shard count | `./build/shardcache-bench --impl sharded --threads 8 --shards 1` (repeat with 4, 16, 64, 256) | Where does adding shards stop helping, and why? |
| Skew | `--zipf 0` vs `--zipf 1.2` | Why does heavy skew hurt sharding? (hint: where does the hottest key live?) |
| Write-heavy | `--read-ratio 0.5` vs `0.99` | Does the gap change? |
| Network | `./scripts/run_bench.sh` net section | Why is the network number so much lower than in-process? Where does the time go? |

## 4. Write docs/DESIGN.md in your own words

Answer each one in 2 to 4 sentences. These are the questions you'll get.

1. Why a hash map plus a linked list? What's the complexity of each operation?
2. `get()` only reads, so why does it need an exclusive lock? Why wouldn't `std::shared_mutex` help here?
3. Sharded LRU only approximates global LRU. When does that matter, and why is it an acceptable trade?
4. One key gets 30% of all traffic. What happens to your cache, and how would you fix it? (hot-key replication, client-side caching)
5. Why `unique_ptr<Shard>` (or whatever you chose)? What is false sharing?
6. `size()` locks shards one at a time, so it isn't an exact snapshot. Why is that fine?
7. The server uses one thread per connection. When does that break, and what would you use instead? (epoll / kqueue event loop)
8. Why `TCP_NODELAY`? What does pipelining buy you?
9. How would you scale this across many machines? Read "Scaling Memcache at Facebook" (NSDI 2013, about 14 pages). It's Meta's own paper, and being able to discuss it in an interview is a real signal.

## 5. Stretch (only after 1 to 4)

- Make reads take a shared lock by replacing strict LRU with CLOCK (a reference
  bit instead of moving nodes). Benchmark it against ShardedCache and explain the trade-off.
- Replace thread-per-connection with a kqueue/epoll event loop and compare the network numbers at 512 connections.

## Resume bullets (fill in from bench/results/RESULTS.md, don't round up)

- Scaled cache throughput **N×** (**A → B** M ops/s at 8 threads, p99 **C → D** ns) by replacing a global lock with lock-striped LRU shards in a C++20 key-value server
- Served **X** req/s at **Y** µs p99 over TCP with request pipelining, verified by 28 GoogleTest cases and clean ThreadSanitizer runs
