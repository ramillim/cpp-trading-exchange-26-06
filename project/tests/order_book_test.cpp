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

    auto& bid_levels = order_book.getBidLevels();
    ASSERT_EQ(bid_levels.size(), 1u);
    auto it = bid_levels.find(15000);
    ASSERT_NE(it, bid_levels.end());

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

    auto& bid_levels = order_book.getBidLevels();
    ASSERT_EQ(bid_levels.at(15000).size(), 3u);

    // Remove middle
    order_book.cancelOrder(o2);
    EXPECT_EQ(bid_levels.at(15000).size(), 2u);
    EXPECT_EQ(o1->list_node.next, o3);
    EXPECT_EQ(o3->list_node.prev, o1);

    // Remove head
    order_book.cancelOrder(o1);
    EXPECT_EQ(bid_levels.at(15000).size(), 1u);
    EXPECT_EQ(bid_levels.at(15000).head(), o3);
    EXPECT_EQ(o3->list_node.prev, nullptr);

    // Remove last order
    // The level is also removed from the map.
    order_book.cancelOrder(o3);
    EXPECT_FALSE(bid_levels.contains(15000));
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

TEST(OrderBookTest, SellSide) {
    OrderBook order_book("AAPL", 10);

    Order* o1 = order_book.addOrder(1, 101, 100, 15100, false, OrderType::Limit);
    ASSERT_NE(o1, nullptr);
    EXPECT_FALSE(o1->is_buy_side);

    auto& ask_levels = order_book.getAskLevels();
    ASSERT_EQ(ask_levels.size(), 1u);
    EXPECT_EQ(ask_levels.at(15100).head(), o1);
}

TEST(OrderBookTest, PoolExhaustion) {
    OrderBook order_book("AAPL", 1);

    Order* o1 = order_book.addOrder(1, 101, 100, 15000, true, OrderType::Limit);
    ASSERT_NE(o1, nullptr);

    Order* o2 = order_book.addOrder(2, 102, 100, 15000, true, OrderType::Limit);
    EXPECT_EQ(o2, nullptr);
}

TEST(OrderBookTest, MarketBuyOrderSweepsMultipleLevels) {
    OrderBook order_book("AAPL", 10);
    // Add two sell levels: 100 @ 15000, 100 @ 15100
    order_book.addOrder(1, 101, 100, 15000, false, OrderType::Limit);
    order_book.addOrder(2, 102, 100, 15100, false, OrderType::Limit);

    // Market buy for 150 should match both levels
    uint32_t matched = order_book.tryMatch(true, 0, 150, OrderType::Market);
    EXPECT_EQ(matched, 150u);

    auto& ask_levels = order_book.getAskLevels();
    EXPECT_FALSE(ask_levels.contains(15000));
    ASSERT_TRUE(ask_levels.contains(15100));
    EXPECT_EQ(ask_levels.at(15100).size(), 1u);
    EXPECT_EQ(ask_levels.at(15100).peek()->size, 50u);
}

TEST(OrderBookTest, MarketSellOrderSweepsMultipleLevels) {
    OrderBook order_book("AAPL", 10);
    // Add two buy levels: 100 @ 15000, 100 @ 14900
    order_book.addOrder(1, 101, 100, 15000, true, OrderType::Limit);
    order_book.addOrder(2, 102, 100, 14900, true, OrderType::Limit);

    // Market sell for 150 should match both levels
    uint32_t matched = order_book.tryMatch(false, 0, 150, OrderType::Market);
    EXPECT_EQ(matched, 150u);

    auto& bid_levels = order_book.getBidLevels();
    EXPECT_FALSE(bid_levels.contains(15000));
    ASSERT_TRUE(bid_levels.contains(14900));
    EXPECT_EQ(bid_levels.at(14900).size(), 1u);
    EXPECT_EQ(bid_levels.at(14900).peek()->size, 50u);
}

TEST(OrderBookTest, LimitBuyOrderMatchesUpToPrice) {
    OrderBook order_book("AAPL", 10);
    // Add sell levels: 100 @ 15000, 100 @ 15100, 100 @ 15200
    order_book.addOrder(1, 101, 100, 15000, false, OrderType::Limit);
    order_book.addOrder(2, 102, 100, 15100, false, OrderType::Limit);
    order_book.addOrder(3, 103, 100, 15200, false, OrderType::Limit);

    // Limit buy for 250 @ 15100 should match 100 @ 15000 and 100 @ 15100
    uint32_t matched = order_book.tryMatch(true, 15100, 250, OrderType::Limit);
    EXPECT_EQ(matched, 200u);

    auto& ask_levels = order_book.getAskLevels();
    EXPECT_FALSE(ask_levels.contains(15000));
    EXPECT_FALSE(ask_levels.contains(15100));
    ASSERT_TRUE(ask_levels.contains(15200));
    EXPECT_EQ(ask_levels.at(15200).peek()->size, 100u);
}

TEST(OrderBookTest, LimitSellOrderMatchesDownToPrice) {
    OrderBook order_book("AAPL", 10);
    // Add buy levels: 100 @ 15000, 100 @ 14900, 100 @ 14800
    order_book.addOrder(1, 101, 100, 15000, true, OrderType::Limit);
    order_book.addOrder(2, 102, 100, 14900, true, OrderType::Limit);
    order_book.addOrder(3, 103, 100, 14800, true, OrderType::Limit);

    // Limit sell for 250 @ 14900 should match 100 @ 15000 and 100 @ 14900
    uint32_t matched = order_book.tryMatch(false, 14900, 250, OrderType::Limit);
    EXPECT_EQ(matched, 200u);

    auto& bid_levels = order_book.getBidLevels();
    EXPECT_FALSE(bid_levels.contains(15000));
    EXPECT_FALSE(bid_levels.contains(14900));
    ASSERT_TRUE(bid_levels.contains(14800));
    EXPECT_EQ(bid_levels.at(14800).peek()->size, 100u);
}

TEST(OrderBookTest, AddOrderTriggersMatchAndRestsRemainder) {
    OrderBook order_book("AAPL", 10);
    // Add sell level: 100 @ 15000
    order_book.addOrder(1, 101, 100, 15000, false, OrderType::Limit);

    // Add Limit buy order for 150 @ 15000. 100 should match, 50 should rest.
    Order* o1 = order_book.addOrder(2, 102, 150, 15000, true, OrderType::Limit);
    ASSERT_NE(o1, nullptr);
    EXPECT_EQ(o1->size, 50u);

    auto& bid_levels = order_book.getBidLevels();
    ASSERT_TRUE(bid_levels.contains(15000));
    EXPECT_EQ(bid_levels.at(15000).head(), o1);

    auto& ask_levels = order_book.getAskLevels();
    EXPECT_FALSE(ask_levels.contains(15000));
}

}  // namespace exchange
