#include "order_book.hpp"

namespace exchange {

Order* OrderBook::addOrder(uint64_t order_id, uint64_t client_id, const std::string& symbol,
                          uint32_t size, uint64_t price, bool is_buy_side, OrderType order_type) {
    Order* order = order_pool_.allocate(order_id, client_id, symbol, size, price, is_buy_side, order_type);
    if (!order) return nullptr;

    auto& levels = is_buy_side ? buy_levels_ : sell_levels_;
    levels[price].append(order);
    return order;
}

void OrderBook::removeOrder(Order* order) {
    if (!order) return;
    auto& levels = order->is_buy_side ? buy_levels_ : sell_levels_;
    auto it = levels.find(order->price);
    if (it != levels.end()) {
        it->second.remove(order);
        if (it->second.empty()) {
            levels.erase(it);
        }
    }
    order_pool_.deallocate(order);
}

} // exchange