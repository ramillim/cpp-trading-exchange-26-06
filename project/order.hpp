#pragma once

#include <string>

#include "intrusive_queue.hpp"
#include "order_type.hpp"

namespace exchange {

struct Order {
    uint64_t order_id;   // unique identifier for order
    uint64_t client_id;  // client that the order belongs to
    std::string
        symbol;         // the ticker symbol. switch this to use something other than string later for faster comparison
    uint32_t size = 0;  // these should support fractional shares and be multiplied by a scaling factor
    uint64_t price = 0;  // price * 100 to avoid floating point operations
    bool is_buy_side = false;
    OrderType order_type;

    IntrusiveQueue<Order>::Node list_node;

    /**
     * Cannot remove more than the available shares. Returns the amount removed.
     * @param amount the amount to remove
     * @return the amount that was removed
     */
    uint32_t removeShares(const uint32_t amount) {
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
