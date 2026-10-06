# Design notes

Answers to the questions this project raises, plus what the experiments
showed. Experiment numbers below came from a 2-core Linux VM (2 threads), so
they show *shape*, not headline performance. Rerun them on a real multicore
machine with the commands in each section.

## 1. Why a hash map plus a linked list?

An LRU cache needs two O(1) operations that no single standard container
gives you: find a key, and move any entry to the "most recent" end (or drop
the oldest). A hash map finds keys in O(1) but has no order. A doubly linked
list moves or removes a node in O(1), but only if you already hold a pointer
to it; finding a key in a list is O(n). So the map stores key -> list
iterator, and the list stores entries in recency order:

| Operation | Steps | Cost |
|---|---|---|
| get | map lookup, splice node to front | O(1) average |
| put (new key) | push front, map insert, maybe pop back + map erase | O(1) average |
| put (existing) | map lookup, overwrite, splice to front | O(1) average |
| erase | map lookup, list erase, map erase | O(1) average |

`std::list::splice` relinks a node without allocating or copying, and
`std::list` iterators stay valid when other nodes change, which is what makes
storing them in the map safe. This is LeetCode 146 (LRU Cache), a common Meta
interview question.

## 2. get() only reads. Why does it need an exclusive lock?

It doesn't only read. A hit moves the entry to the front of the recency list,
which rewrites list pointers. Two threads doing that at once corrupt the
list. That's also why `std::shared_mutex` (many readers, one writer) wouldn't
help: in strict LRU there are no pure readers. Ways around it, each giving up
something:

- **CLOCK / second-chance**: a hit only sets a "referenced" bit (can be an
  atomic), so reads can take a shared lock. Eviction becomes approximate.
- **Sampled or batched recency**: record hits in a buffer and apply them
  later (Caffeine, a Java cache, does this).
- **Skip promotion on some hits**: memcached only bumps an item if it hasn't
  been bumped in the last 60 seconds.

## 3. Sharded LRU is only approximately LRU. Why is that acceptable?

Each shard evicts its own least recently used entry, so the cache as a whole
can evict an entry that is more recent than one sitting in a different, less
busy shard. With many keys and a decent hash, shards fill evenly and the
difference is small. In the benchmark, the hit rates of `locked` (exact LRU)
and `sharded` agree to the third decimal place across every workload tried.
It would matter with very few shards and badly uneven hashing, or if
exact eviction order were a correctness requirement, which it isn't for a cache.

## 4. One key gets 30% of the traffic. What happens?

Every request for that key goes to the same shard and takes the same lock,
so that shard becomes a mini global lock, and adding shards doesn't help.
Fixes: replicate hot keys into several shards and read any replica; cache
hot keys client-side (Meta does this at scale with "leases" and gutter pools,
see the memcache paper below); or make reads of that shard lock-free.

What the experiment showed: at 2 threads, higher skew made both caches
*faster* (zipf 0 -> 1.5: sharded 1.76 -> 5.35M ops/s), because hot entries
stay in the CPU cache and the hit rate rises, and sharding kept a 3 to 4x lead.
Two threads are too few to pile onto one shard. To see the hot-shard effect,
run with all cores: `./build/shardcache-bench --impl sharded --threads 8 --zipf 1.5`
vs `--zipf 0`.

## 5. Why unique_ptr<Shard>? What is false sharing?

`std::mutex` can't be copied or moved, and `std::vector` must move elements
when it grows, so `std::vector<Shard>` doesn't compile. A vector of
`unique_ptr<Shard>` moves pointers instead. False sharing: CPUs cache memory
in 64-byte lines. If two shards' mutexes sat in the same line, every lock on
shard A would invalidate that line in the core using shard B, so they'd
contend in hardware despite sharing no data. `alignas(64)` puts each shard on
its own line.

## 6. size() isn't an exact snapshot. Why is that fine?

It locks shards one at a time, so other threads can change shards already
counted. Each shard's count is exact and capped by its capacity, so the
total is always <= capacity(). STATS is a monitoring number; freezing the
whole cache to make it exact would stall every request for a dashboard.

## 7. Thread per connection: when does it break?

Each connection costs a thread (a stack of 512 KB to 8 MB, plus scheduler
work). Hundreds of connections are fine; tens of thousands mean too much
memory and constant context switching. Production servers use a few threads
each running an event loop (`epoll` on Linux, `kqueue` on macOS) that
multiplexes thousands of sockets. memcached and Meta's servers work this way.
It was a deliberate simplicity trade-off here because the project measures
lock contention, not connection scaling.

## 8. Why TCP_NODELAY? What does pipelining buy?

Nagle's algorithm holds back small writes to batch them, which adds up to
~40 ms to small request/response messages. `TCP_NODELAY` turns it off. With
pipelining, a client sends many requests without waiting for each response;
the server answers every complete line in its buffer with a single `send()`.
That cuts round trips and syscalls, which is where most network time goes:
the in-process cache does an operation in under a microsecond, while a
loopback round trip costs tens of microseconds.

## 9. Scaling across machines

One server's cache is limited by its RAM and NIC. To scale out: partition
keys across servers with consistent hashing (adding a server only moves
about 1/N of the keys), route through a proxy (Meta's mcrouter), and
replicate hot data. Read "Scaling Memcache at Facebook" (Nishtala et al.,
NSDI 2013) for leases (thundering herds, stale sets), regional pools, and
cross-datacenter consistency.

## Other measured results

**Contention collapse.** The global-lock cache got *slower* going from 1 to 2
threads (about 1.95 -> 0.6M ops/s). Threads spend their time handing the
lock back and forth and bouncing its cache line between cores instead of
doing work. That is the core argument for sharding.

**Shard count** (`--impl sharded --threads 2 --shards N`):

| shards | M ops/s | p99 ns |
|---|---|---|
| 1 | 0.60 | 52,792 |
| 4 | 0.98 | 21,826 |
| 16 | 1.78 | 14,011 |
| 64 | 2.79 | 9,411 |
| 256 | 2.72 | 2,231 |

1 shard matches the global-lock cache (sanity check). Throughput flattens past
about 64 shards once collisions are rare, while p99 keeps improving. More
threads should push the knee higher.

**Benchmark sanity check.** With uniform keys (zipf 0) and capacity equal to
half the keyspace, the measured hit rate is 0.500, exactly what theory
predicts, which suggests the workload generator and accounting are right.
