#include "project/order_book.hpp"

#include <gtest/gtest.h>

namespace exchange {

TEST(OrderBookTest, AddOrder) {
    OrderBook order_book("AAPL", 10);

    Order* o1 = order_book.addOrder(1, 101, 100, 15000, true, OrderType::Limit);
    ASSERT_NE(o1, nullptr);
    EXPECT_EQ(o1->order_id, 1u);
    EXPECT_EQ(o1->price, 15000u);
    EXPECT_TRUE(o1->is_buy_side);

    Order* o2 = order_book.addOrder(2, 102, 50, 15000, true, OrderType::Limit);
    ASSERT_NE(o2, nullptr);
    EXPECT_EQ(o2->order_id, 2u);

    auto& buy_levels = order_book.getBuyLevels();
    ASSERT_EQ(buy_levels.size(), 1u);
    auto it = buy_levels.find(15000);
    ASSERT_NE(it, buy_levels.end());

    const PriceLevel& level = it->second;
    EXPECT_EQ(level.head(), o1);
    EXPECT_EQ(level.tail(), o2);
    EXPECT_EQ(o1->list_node.next, o2);
    EXPECT_EQ(o2->list_node.prev, o1);
    EXPECT_EQ(o2->list_node.next, nullptr);
    EXPECT_EQ(o1->list_node.prev, nullptr);
}

TEST(OrderBookTest, CancelOrder) {
    OrderBook order_book("AAPL", 3);

    Order* o1 = order_book.addOrder(1, 101, 100, 15000, true, OrderType::Limit);
    Order* o2 = order_book.addOrder(2, 101, 100, 15000, true, OrderType::Limit);
    Order* o3 = order_book.addOrder(3, 101, 100, 15000, true, OrderType::Limit);

    auto& buy_levels = order_book.getBuyLevels();
    ASSERT_EQ(buy_levels.at(15000).size(), 3u);

    // Remove middle
    order_book.cancelOrder(o2);
    EXPECT_EQ(buy_levels.at(15000).size(), 2u);
    EXPECT_EQ(o1->list_node.next, o3);
    EXPECT_EQ(o3->list_node.prev, o1);

    // Remove head
    order_book.cancelOrder(o1);
    EXPECT_EQ(buy_levels.at(15000).size(), 1u);
    EXPECT_EQ(buy_levels.at(15000).head(), o3);
    EXPECT_EQ(o3->list_node.prev, nullptr);

    // Remove last order
    // The level is also removed from the map.
    order_book.cancelOrder(o3);
    EXPECT_FALSE(buy_levels.contains(15000));
}

TEST(OrderBookTest, TryMatchPerfromsAPartialMatch) {
    OrderBook order_book("AAPL", 10);
    const Order* sell100 = order_book.addOrder(1, 101, 100, 15100, false, OrderType::Limit);

    const uint32_t matched = order_book.tryMatch(true, 15100, 5);
    EXPECT_EQ(matched, 5u);
    EXPECT_EQ(sell100->size, 95u);
}

TEST(OrderBookTest, TryMatchDoesNotExceedAvailableSize) {
    OrderBook order_book("AAPL", 10);
    const Order* sell100 = order_book.addOrder(1, 101, 100, 15100, false, OrderType::Limit);

    const uint32_t matched = order_book.tryMatch(true, 15100, 200);
    EXPECT_EQ(matched, 100u);
    EXPECT_EQ(sell100->size, 0u);
}

TEST(OrderBookTest, TryMatchMatchesSharesUsingFIFO) {
    OrderBook order_book("AAPL", 10);
    const Order* o1 = order_book.addOrder(1, 101, 100, 15100, false, OrderType::Limit);
    const Order* o2 = order_book.addOrder(1, 101, 100, 15100, false, OrderType::Limit);

    const uint32_t matched = order_book.tryMatch(true, 15100, 198);
    EXPECT_EQ(matched, 198u);
    EXPECT_EQ(o1->size, 0u);
    EXPECT_EQ(o2->size, 2u);
}

// TEST(OrderBookTest, RemoveSharesOnlyRemovesUpToCurrentSize) {
//     OrderBook order_book("AAPL", 10);
//     Order* o1 = order_book.addOrder(1, 101, 100, 15100, false, OrderType::Limit);
//     const uint32_t removed = order_book.removeShares(o1, 200);
//     EXPECT_EQ(o1->size, 0u);
//     EXPECT_EQ(removed, 100u);
// }

TEST(OrderBookTest, SellSide) {
    OrderBook order_book("AAPL", 10);

    Order* o1 = order_book.addOrder(1, 101, 100, 15100, false, OrderType::Limit);
    ASSERT_NE(o1, nullptr);
    EXPECT_FALSE(o1->is_buy_side);

    auto& sell_levels = order_book.getSellLevels();
    ASSERT_EQ(sell_levels.size(), 1u);
    EXPECT_EQ(sell_levels.at(15100).head(), o1);
}

TEST(OrderBookTest, PoolExhaustion) {
    OrderBook order_book("AAPL", 1);

    Order* o1 = order_book.addOrder(1, 101, 100, 15000, true, OrderType::Limit);
    ASSERT_NE(o1, nullptr);

    Order* o2 = order_book.addOrder(2, 102, 100, 15000, true, OrderType::Limit);
    EXPECT_EQ(o2, nullptr);
}

}  // namespace exchange
