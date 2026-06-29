//
// Created by RAMIL LIM on 6/29/26.
//

#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <vector>

#include "order_book.hpp"
#include "random_order_generator.hpp"
#include "spdlog/spdlog.h"

int main() {
    spdlog::info("Initializing the Order Book");
    exchange::OrderBook order_book("AAPL", 1024);
    RandomOrderDataGenerator generator;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> type_dis(0, 1);
    std::uniform_int_distribution<> side_dis(0, 1);
    std::uniform_real_distribution<> cancel_dis(0.0, 1.0);

    std::vector<exchange::Order*> orders;

    for (int i = 0; i < 100; ++i) {
        const uint64_t order_id = generator.get_next_order_id();
        const uint64_t client_id = generator.generate_uuid_v4_as_uint64();
        const uint32_t size = generator.generate_random_size();
        const uint64_t price = generator.generate_random_price();
        const bool is_buy_side = side_dis(gen) == 0;
        const exchange::OrderType order_type =
            type_dis(gen) == 0 ? exchange::OrderType::Limit : exchange::OrderType::Market;

        auto* order = order_book.addOrder(order_id, client_id, size, price, is_buy_side, order_type);
        if (order) {
            orders.push_back(order);
        }

        // Randomly cancel an order with < 10% probability
        if (!orders.empty() && cancel_dis(gen) < 0.10) {
            size_t index_to_cancel = std::uniform_int_distribution<size_t>(0, orders.size() - 1)(gen);
            order_book.cancelOrder(orders[index_to_cancel]);
            orders.erase(orders.begin() + index_to_cancel);
        }
    }

    spdlog::info("Order Book after adding and randomly cancelling orders:\n{}", order_book.toString());

    return 0;
}
