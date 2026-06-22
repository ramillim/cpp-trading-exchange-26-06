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

## Week 2

Implement memory pool function. See: Memory Pool - API

- Eliminate heap allocation at runtime (pre-allocate everything upfront)
- Keep hot objects cache-local in a contiguous vector
- Avoid any per-allocation overhead

- Write test cases
    - Serving a mempool with integers
    - Ensure that allocate/deallocate works
- Use the second design choice, but if time permits, benchmark performance 
  with two different data structures. Log time from start to end and see if
  it's different with the different implementations.

Run tests with:
```bash
bazel test //project/tests:mem_pool_test --test_output=all
```

Repurposed `container_benchmark.cpp` to compare the two versions of the MemPool.

```bash
bazel run project:mem_pool_benchmark

=== MemPool Benchmark (N=100) ===

  Allocate N (total)                    MemPool:        917   new/delete:      24042   malloc/free:       1833  cycles
  Deallocate N (total)                  MemPool:        458   new/delete:       3393   malloc/free:        959  cycles
  Dealloc random order (total)          MemPool:        708   new/delete:       1208   malloc/free:       1208  cycles
  Alloc cold (avg cycles/op)            MemPool:         57   new/delete:        724   malloc/free:        470  cycles
  Churn N ops (total)                   MemPool:      11393   new/delete:       8209   malloc/free:       8250  cycles

=== MemPool Benchmark (N=100000) ===

  Allocate N (total)                    MemPool:     789582   new/delete:     961962   malloc/free:     610394  cycles
  Deallocate N (total)                  MemPool:     446371   new/delete:     872302   malloc/free:     746314  cycles
  Dealloc random order (total)          MemPool:     861659   new/delete:    1050805   malloc/free:     983313  cycles
  Alloc cold (avg cycles/op)            MemPool:         53   new/delete:        392   malloc/free:        332  cycles
  Churn N ops (total)                   MemPool:    7600002   new/delete:    7817417   malloc/free:    7520533  cycles

=== MemPool Benchmark (N=1000000) ===

  Allocate N (total)                    MemPool:    8084033   new/delete:    7173339   malloc/free:    7414522  cycles
  Deallocate N (total)                  MemPool:    4591363   new/delete:    8840181   malloc/free:    8448577  cycles
  Dealloc random order (total)          MemPool:   38980114   new/delete:   55219900   malloc/free:   57812834  cycles
  Alloc cold (avg cycles/op)            MemPool:         54   new/delete:        494   malloc/free:        386  cycles
  Churn N ops (total)                   MemPool:   93546936   new/delete:  108036018   malloc/free:  130736929  cycles

Notes:
  - MemPool pre-allocates all slots in a contiguous vector — zero system-allocator calls.
  - new/delete and malloc/free go through the system allocator on every call.
  - 'Alloc cold' evicts the cache (64 MB buffer scan) before each allocation;
    sampled over 256 operations.
  - 'Churn' randomly interleaves allocations and deallocations at ~50% occupancy.
  - Object type: Order (4 x uint64_t = 32 bytes), representative of an order book entry.
  - sink=3468889619391 (prevents dead-code elimination)

```

```bash
bazel run project:mem_pool_separate_free_structure_benchmark

=== MemPoolSeparateFreeStructure Benchmark (N=100) ===

  Allocate N (total)                    MemPool:        917   new/delete:      14333   malloc/free:       1542  cycles
  Deallocate N (total)                  MemPool:        375   new/delete:       3000   malloc/free:        916  cycles
  Dealloc random order (total)          MemPool:        584   new/delete:       1125   malloc/free:       1209  cycles
  Alloc cold (avg cycles/op)            MemPool:         39   new/delete:        588   malloc/free:        307  cycles
  Churn N ops (total)                   MemPool:       7768   new/delete:       8125   malloc/free:       8958  cycles

=== MemPoolSeparateFreeStructure Benchmark (N=100000) ===

  Allocate N (total)                    MemPool:     784855   new/delete:     920028   malloc/free:     918653  cycles
  Deallocate N (total)                  MemPool:     398771   new/delete:     822766   malloc/free:     792582  cycles
  Dealloc random order (total)          MemPool:     955502   new/delete:    1052639   malloc/free:    1045388  cycles
  Alloc cold (avg cycles/op)            MemPool:         37   new/delete:        389   malloc/free:        291  cycles
  Churn N ops (total)                   MemPool:    7699222   new/delete:    7864077   malloc/free:    7401088  cycles

=== MemPoolSeparateFreeStructure Benchmark (N=1000000) ===

  Allocate N (total)                    MemPool:    7354861   new/delete:    7298785   malloc/free:    7523199  cycles
  Deallocate N (total)                  MemPool:    4000095   new/delete:    9213891   malloc/free:    8172128  cycles
  Dealloc random order (total)          MemPool:   38607944   new/delete:   44921083   malloc/free:   56866873  cycles
  Alloc cold (avg cycles/op)            MemPool:         33   new/delete:        222   malloc/free:        467  cycles
  Churn N ops (total)                   MemPool:  102881548   new/delete:  111250549   malloc/free:  105409422  cycles

Notes:
  - MemPoolSeparateFreeStructure pre-allocates all slots in a contiguous vector —
    zero system-allocator calls. The free-list index is stored in a separate field
    alongside (but outside) the object storage.
  - new/delete and malloc/free go through the system allocator on every call.
  - 'Alloc cold' evicts the cache (64 MB buffer scan) before each allocation;
    sampled over 256 operations.
  - 'Churn' randomly interleaves allocations and deallocations at ~50% occupancy.
  - Object type: Order (4 x uint64_t = 32 bytes), representative of an order book entry.
  - sink=3468889619391 (prevents dead-code elimination)
```
