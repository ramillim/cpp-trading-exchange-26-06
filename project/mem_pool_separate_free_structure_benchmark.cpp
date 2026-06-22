#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <vector>

#include "common/perf_utils.h"
#include "project/mem_pool_separate_free_structure.hpp"

// ----------------------------------------------------------------------------
// What this benchmark measures
// ----------------------------------------------------------------------------
// We compare three allocation strategies for fixed-size objects:
//   - MemPoolSeparateFreeStructure<T> — pool allocator with a separate
//     next_free index stored alongside (but outside) the object storage
//   - new / delete  — the default C++ heap allocator
//   - malloc / free — the C heap allocator
//
// Operations benchmarked:
//   1. Allocate N objects (burst allocation)
//   2. Deallocate N objects (burst deallocation)
//   3. Allocate N objects then deallocate in random order
//   4. Steady-state churn: randomly allocate or deallocate one object at a time
//      while keeping the pool ~50% occupied
//
// Timings are in raw CPU clock cycles (via RDTSC / CNTVCT_EL0) so they are
// independent of wall-clock frequency scaling.
//
// Why MemPoolSeparateFreeStructure wins for this kind of workload:
//   - MemPoolSeparateFreeStructure stores elements contiguously in a
//     pre-allocated vector, so all slots are cache-friendly and there is zero
//     system-allocator overhead.
//   - allocate() and deallocate() are each a single index read/write — true
//     O(1) with no locks, no syscalls, and no fragmentation.
//   - The separate next_free field avoids aliasing the object storage with
//     the free-list link, at the cost of slightly larger per-block memory.
//   - new/delete and malloc/free must traverse free-lists or size-class caches
//     maintained by the system allocator, touching more memory and incurring
//     higher per-operation overhead.
// ----------------------------------------------------------------------------

// Prevent the compiler from optimizing away reads.
static volatile uint64_t sink = 0;

struct Order {
    uint64_t order_id;
    uint64_t price;
    uint64_t qty;
    uint64_t side;

    Order(uint64_t id, uint64_t p, uint64_t q, uint64_t s)
        : order_id(id), price(p), qty(q), side(s) {}
};

// ---------------------------------------------------------------------------
// Cache eviction (same approach as container_benchmark)
// ---------------------------------------------------------------------------
static std::vector<char> g_flush_buffer(64u << 20);  // 64 MB

static void evict_cache() {
    volatile char s = 0;
    for (std::size_t i = 0; i < g_flush_buffer.size(); i += 128) {
        s += g_flush_buffer[i];
    }
    sink += static_cast<uint64_t>(s);
}

// ---------------------------------------------------------------------------
// MemPoolSeparateFreeStructure benchmarks
// ---------------------------------------------------------------------------

static uint64_t bench_pool_allocate(MemPoolSeparateFreeStructure<Order>& pool,
                                    std::vector<Order*>& ptrs, std::size_t n) {
    const auto start = Common::rdtsc();
    for (std::size_t i = 0; i < n; ++i) {
        ptrs[i] = pool.allocate(i, i * 100, i * 10, i & 1);
    }
    return Common::rdtsc() - start;
}

static uint64_t bench_pool_deallocate(MemPoolSeparateFreeStructure<Order>& pool,
                                      std::vector<Order*>& ptrs, std::size_t n) {
    const auto start = Common::rdtsc();
    for (std::size_t i = 0; i < n; ++i) {
        sink += ptrs[i]->order_id;
        pool.deallocate(ptrs[i]);
    }
    return Common::rdtsc() - start;
}

static uint64_t bench_pool_dealloc_random(MemPoolSeparateFreeStructure<Order>& pool,
                                          std::vector<Order*>& ptrs,
                                          const std::vector<std::size_t>& order) {
    const auto start = Common::rdtsc();
    for (std::size_t idx : order) {
        sink += ptrs[idx]->order_id;
        pool.deallocate(ptrs[idx]);
    }
    return Common::rdtsc() - start;
}

static uint64_t bench_pool_churn(MemPoolSeparateFreeStructure<Order>& pool,
                                 std::size_t ops, std::mt19937& rng) {
    // Pre-fill pool to ~50%.
    const std::size_t half = ops / 2;
    std::vector<Order*> live;
    live.reserve(ops);
    for (std::size_t i = 0; i < half; ++i) {
        live.push_back(pool.allocate(i, i * 100, i * 10, i & 1));
    }

    std::uniform_int_distribution<int> coin(0, 1);
    const auto start = Common::rdtsc();
    for (std::size_t i = 0; i < ops; ++i) {
        if (coin(rng) == 0 && !live.empty()) {
            // Deallocate a random live element.
            std::uniform_int_distribution<std::size_t> pick(0, live.size() - 1);
            auto idx = pick(rng);
            sink += live[idx]->order_id;
            pool.deallocate(live[idx]);
            live[idx] = live.back();
            live.pop_back();
        } else {
            auto* p = pool.allocate(i, i * 100, i * 10, i & 1);
            if (p) live.push_back(p);
        }
    }
    const auto elapsed = Common::rdtsc() - start;

    // Cleanup remaining live objects.
    for (auto* p : live) pool.deallocate(p);
    return elapsed;
}

// ---------------------------------------------------------------------------
// new/delete benchmarks
// ---------------------------------------------------------------------------

static uint64_t bench_new_allocate(std::vector<Order*>& ptrs, std::size_t n) {
    const auto start = Common::rdtsc();
    for (std::size_t i = 0; i < n; ++i) {
        ptrs[i] = new Order(i, i * 100, i * 10, i & 1);
    }
    return Common::rdtsc() - start;
}

static uint64_t bench_new_deallocate(std::vector<Order*>& ptrs, std::size_t n) {
    const auto start = Common::rdtsc();
    for (std::size_t i = 0; i < n; ++i) {
        sink += ptrs[i]->order_id;
        delete ptrs[i];
    }
    return Common::rdtsc() - start;
}

static uint64_t bench_new_dealloc_random(std::vector<Order*>& ptrs,
                                         const std::vector<std::size_t>& order) {
    const auto start = Common::rdtsc();
    for (std::size_t idx : order) {
        sink += ptrs[idx]->order_id;
        delete ptrs[idx];
    }
    return Common::rdtsc() - start;
}

static uint64_t bench_new_churn(std::size_t ops, std::mt19937& rng) {
    const std::size_t half = ops / 2;
    std::vector<Order*> live;
    live.reserve(ops);
    for (std::size_t i = 0; i < half; ++i) {
        live.push_back(new Order(i, i * 100, i * 10, i & 1));
    }

    std::uniform_int_distribution<int> coin(0, 1);
    const auto start = Common::rdtsc();
    for (std::size_t i = 0; i < ops; ++i) {
        if (coin(rng) == 0 && !live.empty()) {
            std::uniform_int_distribution<std::size_t> pick(0, live.size() - 1);
            auto idx = pick(rng);
            sink += live[idx]->order_id;
            delete live[idx];
            live[idx] = live.back();
            live.pop_back();
        } else {
            live.push_back(new Order(i, i * 100, i * 10, i & 1));
        }
    }
    const auto elapsed = Common::rdtsc() - start;

    for (auto* p : live) delete p;
    return elapsed;
}

// ---------------------------------------------------------------------------
// malloc/free benchmarks
// ---------------------------------------------------------------------------

static uint64_t bench_malloc_allocate(std::vector<Order*>& ptrs, std::size_t n) {
    const auto start = Common::rdtsc();
    for (std::size_t i = 0; i < n; ++i) {
        auto* p = static_cast<Order*>(std::malloc(sizeof(Order)));
        new (p) Order(i, i * 100, i * 10, i & 1);
        ptrs[i] = p;
    }
    return Common::rdtsc() - start;
}

static uint64_t bench_malloc_deallocate(std::vector<Order*>& ptrs, std::size_t n) {
    const auto start = Common::rdtsc();
    for (std::size_t i = 0; i < n; ++i) {
        sink += ptrs[i]->order_id;
        ptrs[i]->~Order();
        std::free(ptrs[i]);
    }
    return Common::rdtsc() - start;
}

static uint64_t bench_malloc_dealloc_random(std::vector<Order*>& ptrs,
                                            const std::vector<std::size_t>& order) {
    const auto start = Common::rdtsc();
    for (std::size_t idx : order) {
        sink += ptrs[idx]->order_id;
        ptrs[idx]->~Order();
        std::free(ptrs[idx]);
    }
    return Common::rdtsc() - start;
}

static uint64_t bench_malloc_churn(std::size_t ops, std::mt19937& rng) {
    const std::size_t half = ops / 2;
    std::vector<Order*> live;
    live.reserve(ops);
    for (std::size_t i = 0; i < half; ++i) {
        auto* p = static_cast<Order*>(std::malloc(sizeof(Order)));
        new (p) Order(i, i * 100, i * 10, i & 1);
        live.push_back(p);
    }

    std::uniform_int_distribution<int> coin(0, 1);
    const auto start = Common::rdtsc();
    for (std::size_t i = 0; i < ops; ++i) {
        if (coin(rng) == 0 && !live.empty()) {
            std::uniform_int_distribution<std::size_t> pick(0, live.size() - 1);
            auto idx = pick(rng);
            sink += live[idx]->order_id;
            live[idx]->~Order();
            std::free(live[idx]);
            live[idx] = live.back();
            live.pop_back();
        } else {
            auto* p = static_cast<Order*>(std::malloc(sizeof(Order)));
            new (p) Order(i, i * 100, i * 10, i & 1);
            live.push_back(p);
        }
    }
    const auto elapsed = Common::rdtsc() - start;

    for (auto* p : live) {
        p->~Order();
        std::free(p);
    }
    return elapsed;
}

// ---------------------------------------------------------------------------
// Cold allocation benchmark — evict cache before each allocation so the
// allocator pays the cost of fetching its metadata from memory.
// ---------------------------------------------------------------------------
static constexpr std::size_t kColdSamples = 256;

static uint64_t bench_pool_allocate_cold(MemPoolSeparateFreeStructure<Order>& pool,
                                         std::vector<Order*>& ptrs) {
    const std::size_t count = std::min(ptrs.size(), kColdSamples);
    uint64_t total = 0;
    for (std::size_t i = 0; i < count; ++i) {
        evict_cache();
        const auto start = Common::rdtsc();
        ptrs[i] = pool.allocate(i, i * 100, i * 10, i & 1);
        total += Common::rdtsc() - start;
    }
    return total / count;
}

static uint64_t bench_new_allocate_cold(std::vector<Order*>& ptrs) {
    const std::size_t count = std::min(ptrs.size(), kColdSamples);
    uint64_t total = 0;
    for (std::size_t i = 0; i < count; ++i) {
        evict_cache();
        const auto start = Common::rdtsc();
        ptrs[i] = new Order(i, i * 100, i * 10, i & 1);
        total += Common::rdtsc() - start;
    }
    return total / count;
}

static uint64_t bench_malloc_allocate_cold(std::vector<Order*>& ptrs) {
    const std::size_t count = std::min(ptrs.size(), kColdSamples);
    uint64_t total = 0;
    for (std::size_t i = 0; i < count; ++i) {
        evict_cache();
        const auto start = Common::rdtsc();
        auto* p = static_cast<Order*>(std::malloc(sizeof(Order)));
        new (p) Order(i, i * 100, i * 10, i & 1);
        ptrs[i] = p;
        total += Common::rdtsc() - start;
    }
    return total / count;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

static void run_benchmark(std::size_t n) {
    std::mt19937 rng(42);

    // Build a random deallocation order.
    std::vector<std::size_t> random_order(n);
    for (std::size_t i = 0; i < n; ++i) random_order[i] = i;
    std::shuffle(random_order.begin(), random_order.end(), rng);

    // ---- MemPoolSeparateFreeStructure ----
    MemPoolSeparateFreeStructure<Order> pool(n);
    std::vector<Order*> pool_ptrs(n);

    const auto pool_alloc = bench_pool_allocate(pool, pool_ptrs, n);
    const auto pool_dealloc = bench_pool_deallocate(pool, pool_ptrs, n);

    // Re-allocate for random deallocation test.
    bench_pool_allocate(pool, pool_ptrs, n);
    const auto pool_dealloc_rand = bench_pool_dealloc_random(pool, pool_ptrs, random_order);

    // Churn test (re-seed for fairness).
    MemPoolSeparateFreeStructure<Order> pool2(n);
    std::mt19937 rng_pool(123);
    const auto pool_churn = bench_pool_churn(pool2, n, rng_pool);

    // Cold allocation test.
    const std::size_t cold_n = std::min(n, kColdSamples);
    MemPoolSeparateFreeStructure<Order> pool3(n);
    std::vector<Order*> pool_cold_ptrs(cold_n);
    const auto pool_alloc_cold = bench_pool_allocate_cold(pool3, pool_cold_ptrs);
    for (std::size_t i = 0; i < cold_n; ++i) pool3.deallocate(pool_cold_ptrs[i]);

    // ---- new/delete ----
    std::vector<Order*> new_ptrs(n);

    const auto new_alloc = bench_new_allocate(new_ptrs, n);
    const auto new_dealloc = bench_new_deallocate(new_ptrs, n);

    bench_new_allocate(new_ptrs, n);
    const auto new_dealloc_rand = bench_new_dealloc_random(new_ptrs, random_order);

    std::mt19937 rng_new(123);
    const auto new_churn = bench_new_churn(n, rng_new);

    std::vector<Order*> new_cold_ptrs(cold_n);
    const auto new_alloc_cold = bench_new_allocate_cold(new_cold_ptrs);
    for (std::size_t i = 0; i < cold_n; ++i) delete new_cold_ptrs[i];

    // ---- malloc/free ----
    std::vector<Order*> malloc_ptrs(n);

    const auto malloc_alloc = bench_malloc_allocate(malloc_ptrs, n);
    const auto malloc_dealloc = bench_malloc_deallocate(malloc_ptrs, n);

    bench_malloc_allocate(malloc_ptrs, n);
    const auto malloc_dealloc_rand = bench_malloc_dealloc_random(malloc_ptrs, random_order);

    std::mt19937 rng_malloc(123);
    const auto malloc_churn = bench_malloc_churn(n, rng_malloc);

    std::vector<Order*> malloc_cold_ptrs(cold_n);
    const auto malloc_alloc_cold = bench_malloc_allocate_cold(malloc_cold_ptrs);
    for (std::size_t i = 0; i < cold_n; ++i) {
        malloc_cold_ptrs[i]->~Order();
        std::free(malloc_cold_ptrs[i]);
    }

    // ---- results ----
    std::cout << "\n=== MemPoolSeparateFreeStructure Benchmark (N=" << n << ") ===\n\n";

    auto row = [](const char* label, uint64_t pool_cycles, uint64_t new_cycles, uint64_t malloc_cycles) {
        printf("  %-36s  MemPool: %10llu   new/delete: %10llu   malloc/free: %10llu  cycles\n",
               label,
               (unsigned long long)pool_cycles,
               (unsigned long long)new_cycles,
               (unsigned long long)malloc_cycles);
    };

    row("Allocate N (total)",            pool_alloc,        new_alloc,        malloc_alloc);
    row("Deallocate N (total)",          pool_dealloc,      new_dealloc,      malloc_dealloc);
    row("Dealloc random order (total)",  pool_dealloc_rand, new_dealloc_rand, malloc_dealloc_rand);
    row("Alloc cold (avg cycles/op)",    pool_alloc_cold,   new_alloc_cold,   malloc_alloc_cold);
    row("Churn N ops (total)",           pool_churn,        new_churn,        malloc_churn);
}

int main() {
    // Three tiers spanning the memory hierarchy:
    //   N=100       -> pool fits in L1; constant factors dominate.
    //   N=100,000   -> pool overflows L1 but fits in L2.
    //   N=1,000,000 -> pool spills to DRAM; locality matters.
    run_benchmark(100);
    run_benchmark(100'000);
    run_benchmark(1'000'000);

    std::cout << "\nNotes:\n"
              << "  - MemPoolSeparateFreeStructure pre-allocates all slots in a contiguous vector —\n"
              << "    zero system-allocator calls. The free-list index is stored in a separate field\n"
              << "    alongside (but outside) the object storage.\n"
              << "  - new/delete and malloc/free go through the system allocator on every call.\n"
              << "  - 'Alloc cold' evicts the cache (64 MB buffer scan) before each allocation;\n"
              << "    sampled over " << kColdSamples << " operations.\n"
              << "  - 'Churn' randomly interleaves allocations and deallocations at ~50% occupancy.\n"
              << "  - Object type: Order (4 x uint64_t = 32 bytes), representative of an order book entry.\n"
              << "  - sink=" << sink << " (prevents dead-code elimination)\n\n";

    return 0;
}
