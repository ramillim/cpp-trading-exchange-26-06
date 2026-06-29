#pragma once

#include "spdlog/fmt/fmt.h"
#include <string_view>
#include <string>

#include "intrusive_queue.hpp"
#include "order_type.hpp"

namespace exchange {

// Max size that can be represented with unsigned 32-bit integer with scaling factor 1000 is 4,294,967.295
// Max price that can be represented by an unsigned 64-bit integer with scaling factor 100 is 184,467,440,737,095,516.15
struct Order {
    uint64_t order_id;   // unique identifier for order
    uint64_t client_id;  // client that the order belongs to
    std::string_view symbol;
    uint32_t size = 0;   // minimum precision is 0.001, so size / 1000 to avoid floating point operations
    uint64_t price = 0;  // minimum tick size is $0.01, so price / 100 to avoid floating point operations
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

    [[nodiscard]] std::string toString() const {
        const char* color = is_buy_side ? "\033[92m" : "\033[31m";
        const char* reset = "\033[0m";
        return fmt::format("{}{}, {}: {}, {:.3f} @ ${:.2f} [id: {}]{}",
                           color, is_buy_side ? "Bid" : "Ask", to_string(order_type), symbol, size / 1000.0, price / 100.0, order_id, reset); // NOLINT(*-narrowing-conversions)
    }
};

}  // namespace exchange
