#include "order_book.hpp"

namespace exchange {

Order *OrderBook::addOrder(
    uint64_t order_id, uint64_t client_id, uint32_t size, uint64_t price, bool is_buy_side, OrderType order_type) {
    Order *order = order_pool_.allocate(order_id, client_id, symbol_, size, price, is_buy_side, order_type);
    if (!order) return nullptr;

    auto &levels = is_buy_side ? buy_levels_ : sell_levels_;

    // Check if this crosses the order book (triggers a match)
    // If it does not, rest it on the order book at its limit price
    levels[price].append(order);

    // If it does, find the best opposite order by price-time priority.
    return order;
}

void OrderBook::cancelOrder(Order *order) {
    if (!order) return;

    const uint32_t price = order->price;
    if (auto &levels = order->is_buy_side ? buy_levels_ : sell_levels_; levels.contains(price)) {
        levels[price].remove(order);
        // Remove the level list if it's empty.
        if (levels[price].empty()) levels.erase(price);
    }

    order_pool_.deallocate(order);
}

uint32_t OrderBook::tryMatch(bool is_buy_side, uint64_t price, uint32_t size) {
    // Use levels from the opposite side of the book
    auto &levels = is_buy_side ? sell_levels_ : buy_levels_;
    if (!levels.contains(price)) return 0;
    uint32_t total_matched = 0;

    while (!levels[price].empty() && total_matched < size) {
        Order *order = levels[price].peek();
        const uint32_t amount_to_match = size - total_matched;
        const uint32_t matched = order->removeShares(amount_to_match);
        total_matched += matched;
        if (order->size == 0) {
            // In the future, may need to distinguish between canceled and completed order
            cancelOrder(order);
        }
    }

    return total_matched;
}

}  // namespace exchange
