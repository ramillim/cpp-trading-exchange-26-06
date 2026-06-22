#pragma once  
  
#include <cstddef>  
#include <cstdio>  
#include <new>
#include <utility>  
#include <vector>  
  
template <typename T>  
class MemPoolSeparateFreeStructure final {
public:  
    explicit MemPoolSeparateFreeStructure(std::size_t num_elems)
        : store_(num_elems), next_free_index_(0) {  
        // Build the free-list: each block's next_free points to the following  
        // block, forming a singly-linked stack through indices.
        ASSERT(num_elems > 0, "MemPoolSeparateFreeStructure requires at least one element");
        for (std::size_t i = 0; i < num_elems; ++i) {  
            store_[i].next_free = i + 1;  
        }  
        // Sentinel: the last block's next_free == num_elems means "no more".  
    }  
  
    template <typename... Args>  
    T *allocate(Args &&...args) noexcept {  
        if (next_free_index_ >= store_.size()) [[unlikely]] {  
            return nullptr;  
        }  
        auto &block = store_[next_free_index_];  
        next_free_index_ = block.next_free;  
        // Construct T in-place inside the block's storage.  
        return ::new (&block.storage) T(std::forward<Args>(args)...);  
    }  
  
    void deallocate(const T *elem) noexcept {  
        // Recover the Block that contains this T.  
        auto *block = reinterpret_cast<Block *>(  
            const_cast<T *>(elem));  
        // Destroy the object.  
        elem->~T();  
        // Push the block back onto the free list.  
        block->next_free = next_free_index_;  
        next_free_index_ = static_cast<std::size_t>(block - &store_[0]);  
    }  
  
    // Deleted special members: the pool owns its backing storage and the free  
    // list; copying would alias both.
    MemPoolSeparateFreeStructure() = delete("a MemPool owns its backing storage and free lists");
    MemPoolSeparateFreeStructure(const MemPoolSeparateFreeStructure &) = delete("a MemPool owns its backing storage and free lists");
    MemPoolSeparateFreeStructure(const MemPoolSeparateFreeStructure &&) = delete("a MemPool owns its backing storage and free lists");
    MemPoolSeparateFreeStructure &operator=(const MemPoolSeparateFreeStructure &) = delete("a MemPool owns its backing storage and free lists");
    MemPoolSeparateFreeStructure &operator=(const MemPoolSeparateFreeStructure &&) = delete("a MemPool owns its backing storage and free lists");
  
private:  
    // Each block holds either a live T or a free-list link.  
    struct Block {  
        alignas(T) unsigned char storage[sizeof(T)];  
        std::size_t next_free{0};  
    };  
  
    static inline auto ASSERT(bool cond, const char *msg) -> void {  
        if (!cond) [[unlikely]] {  
            // Use fprintf + abort instead of exceptions so this stays noexcept-safe.  
            std::fprintf(stderr, "FATAL MemPoolSeparateFreeStructure assertion failed: %s\n", msg);
            std::abort();  
        }  
    }  
  
    std::vector<Block> store_;  
    std::size_t next_free_index_;  
};