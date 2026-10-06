#!/usr/bin/env bash
# Builds in Release, runs the in-process benchmark and the network load test,
# and writes bench/results/latest.csv + bench/results/RESULTS.md.
#
#   ./scripts/run_bench.sh            # full run (a few minutes)
#   QUICK=1 ./scripts/run_bench.sh    # smoke run
set -euo pipefail
cd "$(dirname "$0")/.."

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSHARDCACHE_BUILD_TESTS=OFF >/dev/null
cmake --build build -j >/dev/null

HW=$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu)
THREADS="1,2,4,8"; [ "$HW" -gt 8 ] && THREADS="1,2,4,8,16"
OPS=1000000; REPEATS=3; REQS=50000
if [ "${QUICK:-0}" = 1 ]; then OPS=200000; REPEATS=1; REQS=5000; fi

mkdir -p bench/results
./build/shardcache-bench --threads "$THREADS" --ops "$OPS" --repeats "$REPEATS" --csv bench/results/latest.csv | tee bench/results/bench.txt

echo; echo "== network (32 connections, loopback) =="
: > bench/results/net.txt
for impl in locked sharded; do
  ./build/shardcache-server --impl "$impl" --port 7171 --capacity 200000 >/dev/null &
  pid=$!; sleep 0.5
  printf "%-8s " "$impl" | tee -a bench/results/net.txt
  ./build/shardcache-loadgen --port 7171 --connections 32 --requests "$REQS" | tee -a bench/results/net.txt
  kill -INT $pid; wait $pid 2>/dev/null || true
done

MAXT=${THREADS##*,}
{
  echo "# Benchmark results"
  echo
  echo "Machine: $(uname -sm), $HW hardware threads. Generated $(date '+%Y-%m-%d') by scripts/run_bench.sh."
  echo
  echo '```'; cat bench/results/bench.txt; echo '```'
  echo
  echo "Network (32 connections over loopback):"
  echo '```'; cat bench/results/net.txt; echo '```'
  echo
  awk -F, -v t="$MAXT" 'NR>1 && $2==t {m[$1]=$3; p[$1]=$5}
    END { if (m["locked"] > 0)
      printf "**Headline:** at %d threads, sharded = %.2fM ops/s vs %.2fM locked (%.1fx), p99 %.0f ns vs %.0f ns.\n",
             t, m["sharded"], m["locked"], m["sharded"]/m["locked"], p["sharded"], p["locked"] }' bench/results/latest.csv
} > bench/results/RESULTS.md
echo; tail -1 bench/results/RESULTS.md
