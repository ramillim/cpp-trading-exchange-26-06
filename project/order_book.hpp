#pragma once

#include <map>
#include <memory>
#include <string>

#include "intrusive_queue.hpp"
#include "mem_pool.hpp"
#include "order.hpp"

namespace exchange {

using PriceLevel = IntrusiveQueue<Order>;

class OrderBook {
public:
    explicit OrderBook(const std::string& symbol, const std::size_t pool_size = 1024);

    Order* addOrder(
        uint64_t order_id, uint64_t client_id, uint32_t size, uint64_t price, bool is_buy_side, OrderType order_type);

    /**
     * Removes an order from the order book and deallocates it from the memory pool.
     * @param order the order to remove
     */
    void cancelOrder(Order* order);

    /**
     * Attempts to match a bid order against the ask levels.
     */
    uint32_t matchBidOrder(uint64_t price, uint32_t size, OrderType type, uint32_t total_matched);

    /**
     * Attempts to match an ask order against the bid levels.
     */
    uint32_t matchAskOrder(uint64_t price, uint32_t size, OrderType type, uint32_t total_matched);

    [[nodiscard]] const std::map<uint64_t, PriceLevel>& getBidLevels() const { return bid_levels_; }
    [[nodiscard]] const std::map<uint64_t, PriceLevel>& getAskLevels() const { return ask_levels_; }

    [[nodiscard]] std::string toString() const;

private:
    void matchAtLevel(PriceLevel& level, uint32_t& total_matched, uint32_t size);

    std::string symbol_;
    MemPool<Order> order_pool_;
    std::map<uint64_t, PriceLevel> bid_levels_;  // Price -> PriceLevel
    std::map<uint64_t, PriceLevel> ask_levels_;  // Price -> PriceLevel
};

}  // namespace exchange
