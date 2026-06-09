# cpp-trading-exchange

A high-performance trading exchange engine written in C++26.

The exchange implements core matching-engine components: order intake, a price-time-priority matching algorithm, and market-data dissemination — built with low-latency primitives (lock-free queues, memory pools, RDTSC timing).

## Quick start

```bash
bazel build //...   # build everything
bazel test //...    # run all tests
```

See [BAZEL_BUILD.md](BAZEL_BUILD.md) for full build instructions, available targets, and compiler flags.

## Project layout

```
common/      # shared low-latency utilities (lock-free queue, memory pool, logging)
exchange/
  matcher/        # price-time-priority matching engine
  order_server/   # order intake and session management
  market_data/    # market-data feed handler
examples/    # standalone benchmarks and demos
```

## Development environment setup

### Issues

1. Had to upgrade to minimum Ubuntu 22.04 LTS to install gcc-14 and g++-14 in order to run c++26.
2. Conda/Miniconda was preventing GLIBCXX_3.4.32 from being found. Had to disable with
```bash
conda deactivate
unset LD_LIBRARY_PATH
hash -r
```

## Week 1

Benchmark results:

```
INFO: Running command line: bazel-bin/examples/container_benchmark

=== Container Benchmark (N=100) ===

  Insert (total cycles)         vector:     9030   map:   183820   unordered_map:   157395  cycles
  Lookup (avg cycles/op)        vector:      442   map:      431   unordered_map:      186  cycles
  Lookup cold (avg cycles/op)   vector:     7589   map:     9264   unordered_map:     7783  cycles
  Iterate (total cycles)        vector:     2870   map:    13265   unordered_map:     6685  cycles

=== Container Benchmark (N=100000) ===

  Insert (total cycles)         vector: 10415615   map: 171017700   unordered_map: 56561400  cycles
  Lookup (avg cycles/op)        vector:      932   map:     1127   unordered_map:      280  cycles
  Lookup cold (avg cycles/op)   vector:    13107   map:    19879   unordered_map:    10316  cycles
  Iterate (total cycles)        vector:  2028740   map:  7790580   unordered_map:  5051375  cycles


=== Container Benchmark (N=10000000) ===

  Insert (total cycles)         vector: 1114309455   map: 23671997720   unordered_map: 6374298630  cycles
  Lookup (avg cycles/op)        vector:     2955   map:     9997   unordered_map:     1180  cycles
  Lookup cold (avg cycles/op)   vector:    20953   map:    35111   unordered_map:    13223  cycles
  Iterate (total cycles)        vector: 205914345   map: 884257640   unordered_map: 480204130  cycles

Notes:
  - vector lookup uses binary search (O(log N)), same asymptotic cost as map.
  - vector iteration is a contiguous memory scan — maximally cache-friendly.
  - map nodes are heap-allocated individually; each traversal step risks a cache miss.
  - unordered_map is pre-sized (reserve) to avoid rehashing penalty.
  - 'Lookup cold' evicts the cache (64 MB buffer scan) before each probe so
    the lookup pays the cost of refetching from memory; sampled over 256 keys.
  - at small N, timings are dominated by rdtsc overhead and quantization.
  - sink=600068103923820 (prevents dead-code elimination)
```