# Benchmark results

Machine: Darwin arm64, 8 hardware threads. Generated 2026-10-06 by scripts/run_bench.sh.

```
keys=1000000 capacity=500000 shards=64 reads=90% zipf=0.99 value=64B ops/thread=1000000 repeats=3 (median of repeats reported)
hardware threads: 8

impl     threads  Mops/s      p50 ns    p99 ns    hit rate
locked   1        4.91        125       916       0.803
locked   2        2.11        209       84209     0.826
locked   4        1.55        1333      29458     0.848
locked   8        1.46        583       88375     0.870
sharded  1        4.37        125       1084      0.803
sharded  2        6.39        167       2458      0.826
sharded  4        10.04       208       3458      0.848
sharded  8        8.43        250       18625     0.870
```

Network (32 connections over loopback):
```
locked   connections=32 requests=1600000 throughput=180507 req/s p50=156.7us p99=576.3us errors=0
sharded  connections=32 requests=1600000 throughput=185913 req/s p50=160.8us p99=430.3us errors=0
```

**Headline:** at 8 threads, sharded = 8.43M ops/s vs 1.46M locked (5.8x), p99 18625 ns vs 88375 ns.
