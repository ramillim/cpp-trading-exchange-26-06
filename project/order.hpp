#pragma once

#include <string_view>

#include "intrusive_queue.hpp"
#include "order_type.hpp"

namespace exchange {

struct Order {
    uint64_t order_id;   // unique identifier for order
    uint64_t client_id;  // client that the order belongs to
    std::string_view symbol;
    uint32_t size = 0;   // these should support fractional shares and be multiplied by a scaling factor
    uint64_t price = 0;  // price * 100 to avoid floating point operations
    bool is_buy_side = false;
    OrderType order_type;

    IntrusiveQueue<Order>::Node list_node;

    /**
     * Cannot remove more than the available size. Returns the amount removed.
     * @param amount the amount to remove
     * @return the amount that was removed
     */
    uint32_t decreaseSize(const uint32_t amount) {
        if (amount >= size) {
            const uint32_t removed = size;
            size = 0;
            return removed;
        }
        size -= amount;
        return amount;
    };
};

}  // namespace exchange
