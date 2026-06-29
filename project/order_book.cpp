#include "order_book.hpp"

#include <fmt/format.h>

#include <set>
#include <sstream>

#include "spdlog/spdlog.h"

namespace exchange {

OrderBook::OrderBook(const std::string &symbol, const std::size_t pool_size)
    : symbol_(symbol), order_pool_(pool_size) {}

Order *OrderBook::addOrder(
    uint64_t order_id, uint64_t client_id, uint32_t size, uint64_t price, bool is_buy_side, OrderType order_type) {
    Order *order =
        order_pool_.allocate(order_id, client_id, std::string_view(symbol_), size, price, is_buy_side, order_type);

    if (!order) [[unlikely]]
        return nullptr;

    spdlog::info("(Incoming): {}", order->toString());

    auto &levels = is_buy_side ? bid_levels_ : ask_levels_;

    // Check if this crosses the order book (triggers a match)
    // If it does, find the best opposite order by price-time priority.
    // If it does not, rest it on the order book at its limit price
    // Any remaining units rest on the order book.

    const uint32_t matched_size =
        is_buy_side ? matchBidOrder(price, size, order_type, 0) : matchAskOrder(price, size, order_type, 0);
    order->decreaseSize(matched_size);

    if (matched_size < size) {
        // Any remaining unmatched quantity rests on the book
        spdlog::info("(Resting): {}", order->toString());

        levels[price].append(order);
    } else {
        spdlog::info("(Fully Matched): {}", order->toString());
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

    spdlog::info("(Cancelled): {}", order->toString());
    order_pool_.deallocate(order);
}

uint32_t OrderBook::matchBidOrder(const uint64_t price, const uint32_t size, OrderType type, uint32_t total_matched) {
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
    return total_matched;
}

uint32_t OrderBook::matchAskOrder(const uint64_t price, const uint32_t size, OrderType type, uint32_t total_matched) {
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
    return total_matched;
}

void OrderBook::matchAtLevel(PriceLevel &level, uint32_t &total_matched, const uint32_t size) {
    while (!level.empty() && total_matched < size) {
        Order *order = level.peek();
        const uint32_t amount_to_match = size - total_matched;
        const uint32_t matched = order->decreaseSize(amount_to_match);
        spdlog::info(fmt::format("(Matched {:.3f} with): {}", matched / 1000.0, order->toString()));

        total_matched += matched;

        if (order->size == 0) {
            level.remove(order);
            // Consider other events that may trigger here (or go to an event queue) for closing out an Order before it
            // is deallocated.
            spdlog::info("(Closing): {}", order->toString());
            order_pool_.deallocate(order);
        }
    }
}

std::string OrderBook::toString() const {
    std::set<uint64_t, std::greater<>> prices;
    for (const auto &[price, _] : bid_levels_) prices.insert(price);
    for (const auto &[price, _] : ask_levels_) prices.insert(price);

    std::stringstream ss;
    ss << "Order Book for " << symbol_ << ":\n";
    ss << fmt::format("{:>10} | {:>10} | {:>10}\n", "Bids", "Price", "Asks");
    ss << "-------------------------------------\n";

    for (const uint64_t price : prices) {
        std::string bid_count = "0";
        if (auto it = bid_levels_.find(price); it != bid_levels_.end()) {
            bid_count = std::to_string(it->second.size());
        }

        std::string ask_count = "0";
        if (auto it = ask_levels_.find(price); it != ask_levels_.end()) {
            ask_count = std::to_string(it->second.size());
        }

        ss << fmt::format("{:>10} | {:>10.2f} | {:>10}\n", bid_count, price / 100.0, ask_count);
    }

    double mid_price = 0.0;
    if (!bid_levels_.empty() && !ask_levels_.empty()) {
        mid_price =
            (static_cast<double>(bid_levels_.rbegin()->first) + static_cast<double>(ask_levels_.begin()->first)) /
            200.0;
    }
    ss << "-------------------------------------\n";
    ss << fmt::format("Mid Price: {:.2f}\n", mid_price);

    return ss.str();
}

}  // namespace exchange
