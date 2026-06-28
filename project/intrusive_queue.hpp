#pragma once

#include <cstddef>

namespace exchange {

template <typename T>
class IntrusiveQueue {
public:
    struct Node {
        T* next = nullptr;
        T* prev = nullptr;
    };

    T* peek() const { return head_; }

    T* pop() {
        if (!head_) {
            return nullptr;
        }

        T* item = head_;
        remove(item);
        return item;
    }

    void append(T* item) {
        Node& node = get_node(item);
        if (!head_) {
            head_ = tail_ = item;
            node.next = node.prev = nullptr;
        } else {
            Node& tail_node = get_node(tail_);
            tail_node.next = item;
            node.prev = tail_;
            node.next = nullptr;
            tail_ = item;
        }
        ++size_;
    }

    void remove(T* item) {
        Node& node = get_node(item);
        if (node.prev) {
            get_node(node.prev).next = node.next;
        } else {
            head_ = node.next;
        }

        if (node.next) {
            get_node(node.next).prev = node.prev;
        } else {
            tail_ = node.prev;
        }

        node.next = node.prev = nullptr;
        --size_;
    }

    T* head() const { return head_; }
    T* tail() const { return tail_; }
    [[nodiscard]] std::size_t size() const { return size_; }
    [[nodiscard]] bool empty() const { return size_ == 0; }

private:
    // This assumes T has a member named 'node' of type IntrusiveList<T>::Node
    // or provides a way to get the node.
    static Node& get_node(T* item) { return item->list_node; }

    T* head_ = nullptr;
    T* tail_ = nullptr;
    std::size_t size_ = 0;
};

}  // namespace exchange
