#include "order_book.hpp"

#include <iostream>

namespace exchange {

Order *OrderBook::addOrder(
    uint64_t order_id, uint64_t client_id, uint32_t size, uint64_t price, bool is_buy_side, OrderType order_type) {
    Order *order =
        order_pool_.allocate(order_id, client_id, std::string_view(symbol_), size, price, is_buy_side, order_type);
    if (!order) [[unlikely]]
        return nullptr;

    auto &levels = is_buy_side ? bid_levels_ : ask_levels_;

    // Check if this crosses the order book (triggers a match)
    // If it does, find the best opposite order by price-time priority.
    // If it does not, rest it on the order book at its limit price
    // Any remaining units rest on the order book.

    if (const uint32_t matched_size = tryMatch(is_buy_side, price, size, order_type); matched_size < size) {
        // Any remaining unmatched quantity rests on the book
        order->decreaseSize(matched_size);
        levels[price].append(order);
    }

    return order;
}

void OrderBook::cancelOrder(Order *order) {
    if (!order) [[unlikely]]
        return;

    const uint64_t price = order->price;
    auto &levels = order->is_buy_side ? bid_levels_ : ask_levels_;
    if (auto it = levels.find(price); it != levels.end()) {
        it->second.remove(order);
        // Remove the level list if it's empty.
        if (it->second.empty()) levels.erase(it);
    }

    order_pool_.deallocate(order);
}

uint32_t OrderBook::tryMatch(const bool is_buy_side, const uint64_t price, const uint32_t size, OrderType type) {
    uint32_t total_matched = 0;

    if (is_buy_side) {
        // Buy order: match against ask levels (sell orders)
        // Search from lowest level to highest level
        while (total_matched < size) {
            auto it = ask_levels_.begin();
            if (it == ask_levels_.end()) break;

            if (type == OrderType::Limit && it->first > price) break;

            matchAtLevel(it->second, total_matched, size);

            if (it->second.empty()) {
                ask_levels_.erase(it);
            } else {
                break;
            }
        }
    } else {
        // Sell order: match against bid levels (buy orders)
        // Search from highest level to lowest level
        while (total_matched < size) {
            auto it = bid_levels_.rbegin();
            if (it == bid_levels_.rend()) break;

            if (type == OrderType::Limit && it->first < price) break;

            matchAtLevel(it->second, total_matched, size);

            if (it->second.empty()) {
                bid_levels_.erase(std::prev(it.base()));
            } else {
                break;
            }
        }
    }

    return total_matched;
}

void OrderBook::matchAtLevel(PriceLevel &level, uint32_t &total_matched, const uint32_t size) {
    while (!level.empty() && total_matched < size) {
        Order *order = level.peek();
        const uint32_t amount_to_match = size - total_matched;
        const uint32_t matched = order->decreaseSize(amount_to_match);
        total_matched += matched;
        if (order->size == 0) {
            level.remove(order);
            order_pool_.deallocate(order);
        }
    }
}

}  // namespace exchange
