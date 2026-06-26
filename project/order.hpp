#pragma once

#include <string>

#include "order_type.hpp"
#include "intrusive_list.hpp"

namespace exchange {

struct Order {
    uint64_t order_id; // unique identifier for order
    uint64_t client_id; // client that the order belongs to
    std::string symbol; // the ticker symbol. switch this to use something other than string later for faster comparison
    uint32_t size = 0; // these should support fractional shares and be multiplied by a scaling factor
    uint64_t price = 0; // price * 100 to avoid floating point operations
    bool is_buy_side = false;
    OrderType order_type;

    IntrusiveList<Order>::Node list_node;
};

} // exchange
