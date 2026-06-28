#pragma once

#include <memory>
#include <unordered_map>

#include "intrusive_queue.hpp"
#include "mem_pool.hpp"
#include "order.hpp"

namespace exchange {

using PriceLevel = IntrusiveQueue<Order>;

class OrderBook {
public:
    explicit OrderBook(const std::string& symbol, const std::size_t pool_size = 1024)
        : symbol_(symbol), order_pool_(pool_size) {}

    Order* addOrder(
        uint64_t order_id, uint64_t client_id, uint32_t size, uint64_t price, bool is_buy_side, OrderType order_type);

    /**
     * Removes an order from the order book and deallocates it from the memory pool.
     * @param order the order to remove
     */
    void cancelOrder(Order* order);

    /**
     * Attempts to match an order with as many orders of the opposite side as possible. The remainder is returned.
     * @param is_buy_side
     * @param price
     * @param size
     * @return the unmatched size
     */
    uint32_t tryMatch(bool is_buy_side, uint64_t price, uint32_t size);

    [[nodiscard]] const std::unordered_map<uint64_t, PriceLevel>& getBuyLevels() const { return buy_levels_; }
    [[nodiscard]] const std::unordered_map<uint64_t, PriceLevel>& getSellLevels() const { return sell_levels_; }

private:
    std::string symbol_;
    MemPool<Order> order_pool_;
    std::unordered_map<uint64_t, PriceLevel> buy_levels_;   // Price -> PriceLevel
    std::unordered_map<uint64_t, PriceLevel> sell_levels_;  // Price -> PriceLevel
};

}  // namespace exchange
