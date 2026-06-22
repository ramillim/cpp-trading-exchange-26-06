#pragma once

#include <cstddef>
#include <cstdio>
#include <new>
#include <utility>
#include <vector>

template <typename T>
class MemPool final {
public:
    explicit MemPool(std::size_t num_elems)
        : store_(num_elems), next_free_index_(0) {
        // Build the free-list: each element starts as free, with its storage area holding the
        // index of the next free element.
        ASSERT(num_elems > 0, "MemPool requires at least one element");
        for (std::size_t i = 0; i < num_elems; ++i) {
            store_[i].is_free_or_in_use_ = true;
            next_free_of(store_[i]) = i + 1;
        }
        // Sentinel: the last element's next_free == num_elems means "no more".
    }

    /**
     * Serves a chunk of memory from the pool for storing a new object.
     *
     * @tparam Args
     * @param args
     * @return
     */
    template <typename... Args>
    T *allocate(Args &&...args) noexcept {
        if (next_free_index_ >= store_.size()) [[unlikely]] {
            return nullptr;
        }
        auto &elem = store_[next_free_index_];
        next_free_index_ = next_free_of(elem);
        elem.is_free_or_in_use_ = false;
        // Construct T in-place inside the element's object storage.
        return ::new (&elem.object) T(std::forward<Args>(args)...);
    }

    /**
     * Frees the memory and returns it to the pool.
     *
     * @param ptr
     */
    void deallocate(const T *ptr) noexcept {
        // Recover the Element that contains this T.
        auto *elem = reinterpret_cast<Element *>(
            reinterpret_cast<unsigned char *>(const_cast<T *>(ptr)) -
            offsetof(Element, object));
        // Destroy the object.
        ptr->~T();
        // Mark as free and push back onto the free list.
        elem->is_free_or_in_use_ = true;
        next_free_of(*elem) = next_free_index_;
        next_free_index_ = static_cast<std::size_t>(elem - &store_[0]);
    }

    // Deleted special members: the pool owns its backing storage and the free
    // list; copying would alias both.
    MemPool() = delete("a MemPool owns its backing storage and free lists");
    MemPool(const MemPool &) = delete("a MemPool owns its backing storage and free lists");
    MemPool(const MemPool &&) = delete("a MemPool owns its backing storage and free lists");
    MemPool &operator=(const MemPool &) = delete("a MemPool owns its backing storage and free lists");
    MemPool &operator=(const MemPool &&) = delete("a MemPool owns its backing storage and free lists");

private:
    // Each element stores whether it is free or in use, alongside the object storage.
    // When free, the object storage holds the index of the next free element (free-list link).
    // A free-list link is used because it allows the allocator to achieve true O(1) allocation
    // and deallocation speeds with zero runtime memory overhead if the link is implemented
    // intrusively (inside the unallocated memory blocks).
    struct Element {
        bool is_free_or_in_use_{true};

        // While the slot is allocated: it contains a T object.
        // While the slot is free: it contains a std::size_t free-list index.
        // Storage must be large enough for either a T object or a std::size_t free-list index.
        static constexpr std::size_t kStorageSize =
            sizeof(T) > sizeof(std::size_t) ? sizeof(T) : sizeof(std::size_t);
        alignas(T) alignas(std::size_t) unsigned char object[kStorageSize]{};
    };

    // Access the next-free index stored inside a free element's object area.
    static std::size_t &next_free_of(Element &elem) {
        return *reinterpret_cast<std::size_t *>(elem.object);
    }

    static inline auto ASSERT(bool cond, const char *msg) -> void {
        if (!cond) [[unlikely]] {
            // Use fprintf + abort instead of exceptions so this stays noexcept-safe.
            std::fprintf(stderr, "FATAL MemPool assertion failed: %s\n", msg);
            std::abort();
        }
    }

    std::vector<Element> store_;
    std::size_t next_free_index_;
};
