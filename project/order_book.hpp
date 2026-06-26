#pragma once

#include <map>
#include <memory>
#include "order.hpp"
#include "mem_pool.hpp"
#include "intrusive_list.hpp"

namespace exchange {

using PriceLevel = IntrusiveList<Order>;

class OrderBook {
public:
    explicit OrderBook(std::size_t pool_size = 1024) : order_pool_(pool_size) {}

    Order* addOrder(uint64_t order_id, uint64_t client_id, const std::string& symbol,
                   uint32_t size, uint64_t price, bool is_buy_side, OrderType order_type);

    void removeOrder(Order* order);

    const std::map<uint64_t, PriceLevel>& getBuyLevels() const { return buy_levels_; }
    const std::map<uint64_t, PriceLevel>& getSellLevels() const { return sell_levels_; }

private:
    MemPool<Order> order_pool_;
    std::map<uint64_t, PriceLevel> buy_levels_;  // Price -> PriceLevel
    std::map<uint64_t, PriceLevel> sell_levels_; // Price -> PriceLevel
};

} // exchange
