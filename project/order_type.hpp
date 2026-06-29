#pragma once

#include <string_view>

namespace exchange {
    enum class OrderType {
        Limit,
        Market
    };

    [[nodiscard]] constexpr std::string_view to_string(const OrderType type) {
        switch (type) {
            case OrderType::Limit: return "Limit";
            case OrderType::Market: return "Market";
        }
        return "Unknown";
    }
}
