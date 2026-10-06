# Status and study plan

Tasks 1 (LruCache) and 2 (ShardedCache) are implemented: all 28 tests pass,
and ThreadSanitizer, AddressSanitizer + UBSan, GCC and Clang builds are all
clean. The code is commented to be studied. **This does not go on your resume
until you can do all of the following without looking:**

- [ ] Write LruCache from scratch in under 20 minutes (it's LeetCode 146; do it there in Java too)
- [ ] Explain why get() needs an exclusive lock and why shared_mutex wouldn't help
- [ ] Explain the shard routing, the capacity split, alignas(64), and why unique_ptr
- [ ] Answer every question in docs/DESIGN.md out loud, in your own words
- [ ] Run the benchmark on your Mac and explain each number in the results

## 0. Setup (macOS)

```bash
xcode-select --install          # compiler, if you don't have it
brew install cmake ninja
chmod +x scripts/run_bench.sh
cmake -S . -B build -G Ninja && cmake --build build
./build/shardcache-tests        # 28/28 should pass

# data races / memory errors
cmake -S . -B build-tsan -G Ninja -DSHARDCACHE_TSAN=ON && cmake --build build-tsan && ./build-tsan/shardcache-tests
```

## 1. Benchmark and experiments

```bash
./scripts/run_bench.sh            # writes bench/results/RESULTS.md with a headline line
```

Then rerun these experiments with `--threads 8` and compare to docs/DESIGN.md (measured on 2 cores):

| Experiment | Command | Question |
|---|---|---|
| Shard count | `./build/shardcache-bench --impl sharded --threads 8 --shards 1` (repeat with 4, 16, 64, 256) | Where does adding shards stop helping, and why? |
| Skew | `--zipf 0` vs `--zipf 1.2` | Why does heavy skew hurt sharding? (hint: where does the hottest key live?) |
| Write-heavy | `--read-ratio 0.5` vs `0.99` | Does the gap change? |
| Network | `./scripts/run_bench.sh` net section | Why is the network number so much lower than in-process? Where does the time go? |

## 2. Interview questions (answered in docs/DESIGN.md, practice them out loud)

1. Why a hash map plus a linked list? What's the complexity of each operation?
2. `get()` only reads, so why does it need an exclusive lock? Why wouldn't `std::shared_mutex` help here?
3. Sharded LRU only approximates global LRU. When does that matter, and why is it an acceptable trade?
4. One key gets 30% of all traffic. What happens to your cache, and how would you fix it? (hot-key replication, client-side caching)
5. Why `unique_ptr<Shard>` (or whatever you chose)? What is false sharing?
6. `size()` locks shards one at a time, so it isn't an exact snapshot. Why is that fine?
7. The server uses one thread per connection. When does that break, and what would you use instead? (epoll / kqueue event loop)
8. Why `TCP_NODELAY`? What does pipelining buy you?
9. How would you scale this across many machines? Read "Scaling Memcache at Facebook" (NSDI 2013, about 14 pages). It's Meta's own paper, and being able to discuss it in an interview is a real signal.

## 3. Stretch

- Make reads take a shared lock by replacing strict LRU with CLOCK (a reference
  bit instead of moving nodes). Benchmark it against ShardedCache and explain the trade-off.
- Replace thread-per-connection with a kqueue/epoll event loop and compare the network numbers at 512 connections.

## Resume bullets (fill in from bench/results/RESULTS.md, don't round up)

- Scaled cache throughput **N×** (**A → B** M ops/s at 8 threads, p99 **C → D** ns) by replacing a global lock with lock-striped LRU shards in a C++20 key-value server
- Served **X** req/s at **Y** µs p99 over TCP with request pipelining, verified by 28 GoogleTest cases and clean ThreadSanitizer runs
