#include "project/mem_pool.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace {

struct Trivial {
    int x;
    double y;
};

TEST(MemPool, AllocateReturnNonNull) {
    MemPool<Trivial> pool(4);
    auto *p = pool.allocate(1, 2.0);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->x, 1);
    EXPECT_DOUBLE_EQ(p->y, 2.0);
    pool.deallocate(p);
}

TEST(MemPool, ExhaustPoolReturnsNull) {
    MemPool<int> pool(2);
    auto *a = pool.allocate(10);
    auto *b = pool.allocate(20);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(*a, 10);
    EXPECT_EQ(*b, 20);
    // Pool is full — should return nullptr.
    EXPECT_EQ(pool.allocate(30), nullptr);
    pool.deallocate(a);
    pool.deallocate(b);
}

TEST(MemPool, DeallocateAndReuse) {
    MemPool<int> pool(1);
    auto *a = pool.allocate(42);
    ASSERT_NE(a, nullptr);
    pool.deallocate(a);
    // After deallocation the slot should be available again.
    auto *b = pool.allocate(99);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(*b, 99);
    pool.deallocate(b);
}

TEST(MemPool, AllSlotsAreContiguous) {
    constexpr std::size_t N = 64;
    MemPool<uint64_t> pool(N);
    std::vector<uint64_t *> ptrs;
    ptrs.reserve(N);
    for (std::size_t i = 0; i < N; ++i) {
        ptrs.push_back(pool.allocate(i));
        ASSERT_NE(ptrs.back(), nullptr);
    }
    // All allocations should reside within a contiguous backing array,
    // so the distance between the first and last pointer should be bounded.
    auto lo = reinterpret_cast<uintptr_t>(ptrs.front());
    auto hi = reinterpret_cast<uintptr_t>(ptrs.back());
    // The block size may be larger than sizeof(uint64_t) due to the
    // next_free field, but the total span should be well under N * 256.
    EXPECT_LT(hi - lo, N * 256);

    for (auto *p : ptrs) pool.deallocate(p);
}

TEST(MemPool, MultipleAllocDealloc) {
    MemPool<int> pool(3);
    auto *a = pool.allocate(1);
    auto *b = pool.allocate(2);
    pool.deallocate(a);
    auto *c = pool.allocate(3);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(*c, 3);
    auto *d = pool.allocate(4);
    ASSERT_NE(d, nullptr);
    // Pool should be full now (b, c, d).
    EXPECT_EQ(pool.allocate(5), nullptr);
    pool.deallocate(b);
    pool.deallocate(c);
    pool.deallocate(d);
}

struct Counted {
    static int live;
    Counted() { ++live; }
    ~Counted() { --live; }
    Counted(const Counted &) { ++live; }
};
int Counted::live = 0;

TEST(MemPool, DestructorCalledOnDeallocate) {
    Counted::live = 0;
    MemPool<Counted> pool(2);
    auto *a = pool.allocate();
    auto *b = pool.allocate();
    EXPECT_EQ(Counted::live, 2);
    pool.deallocate(a);
    EXPECT_EQ(Counted::live, 1);
    pool.deallocate(b);
    EXPECT_EQ(Counted::live, 0);
}

TEST(MemPool, WorksWithNonTrivialType) {
    MemPool<std::string> pool(4);
    auto *s = pool.allocate("hello world");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(*s, "hello world");
    pool.deallocate(s);
}

TEST(MemPool, IntAllocateSingleValue) {
    MemPool<int> pool(8);
    auto *p = pool.allocate(42);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 42);
    pool.deallocate(p);
}

TEST(MemPool, IntAllocateZeroAndNegative) {
    MemPool<int> pool(4);
    auto *zero = pool.allocate(0);
    auto *neg = pool.allocate(-1);
    auto *min_val = pool.allocate(std::numeric_limits<int>::min());
    auto *max_val = pool.allocate(std::numeric_limits<int>::max());
    ASSERT_NE(zero, nullptr);
    ASSERT_NE(neg, nullptr);
    ASSERT_NE(min_val, nullptr);
    ASSERT_NE(max_val, nullptr);
    EXPECT_EQ(*zero, 0);
    EXPECT_EQ(*neg, -1);
    EXPECT_EQ(*min_val, std::numeric_limits<int>::min());
    EXPECT_EQ(*max_val, std::numeric_limits<int>::max());
    pool.deallocate(zero);
    pool.deallocate(neg);
    pool.deallocate(min_val);
    pool.deallocate(max_val);
}

TEST(MemPool, IntAllocateAllSlots) {
    constexpr std::size_t N = 16;
    MemPool<int> pool(N);
    std::vector<int *> ptrs;
    ptrs.reserve(N);
    for (std::size_t i = 0; i < N; ++i) {
        auto *p = pool.allocate(static_cast<int>(i * 10));
        ASSERT_NE(p, nullptr);
        ptrs.push_back(p);
    }
    // Verify each slot holds the correct value.
    for (std::size_t i = 0; i < N; ++i) {
        EXPECT_EQ(*ptrs[i], static_cast<int>(i * 10));
    }
    // Pool is full.
    EXPECT_EQ(pool.allocate(999), nullptr);
    for (auto *p : ptrs) pool.deallocate(p);
}

TEST(MemPool, IntDeallocateAllThenReallocate) {
    constexpr std::size_t N = 8;
    MemPool<int> pool(N);
    std::vector<int *> ptrs;
    // Fill the pool.
    for (std::size_t i = 0; i < N; ++i) {
        ptrs.push_back(pool.allocate(static_cast<int>(i)));
    }
    // Free everything.
    for (auto *p : ptrs) pool.deallocate(p);
    ptrs.clear();
    // Re-fill: all N slots should be available again.
    for (std::size_t i = 0; i < N; ++i) {
        auto *p = pool.allocate(static_cast<int>(i + 100));
        ASSERT_NE(p, nullptr);
        EXPECT_EQ(*p, static_cast<int>(i + 100));
        ptrs.push_back(p);
    }
    EXPECT_EQ(pool.allocate(0), nullptr);
    for (auto *p : ptrs) pool.deallocate(p);
}

TEST(MemPool, IntDeallocateMiddleSlot) {
    MemPool<int> pool(3);
    auto *a = pool.allocate(10);
    auto *b = pool.allocate(20);
    auto *c = pool.allocate(30);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);
    // Free only the middle slot.
    pool.deallocate(b);
    // The freed slot should be reusable.
    auto *d = pool.allocate(40);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(*d, 40);
    // Remaining original slots should be intact.
    EXPECT_EQ(*a, 10);
    EXPECT_EQ(*c, 30);
    pool.deallocate(a);
    pool.deallocate(c);
    pool.deallocate(d);
}

TEST(MemPool, IntRepeatedAllocDeallocSameSlot) {
    MemPool<int> pool(1);
    for (int i = 0; i < 100; ++i) {
        auto *p = pool.allocate(i);
        ASSERT_NE(p, nullptr);
        EXPECT_EQ(*p, i);
        pool.deallocate(p);
    }
}

}  // namespace
