#include "../include/orderbook/orderbook.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace orderbook;

class OrderBookTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Create a fresh order book for each test
    book.clear();
    next_id = 0;
  }

  OrderBook book;

  // Helper to create orders with unique IDs
  static uint64_t next_id;
  Order createBuyOrder(double price, uint32_t quantity) {
    return Order(++next_id, Side::BUY, price, quantity);
  }

  Order createSellOrder(double price, uint32_t quantity) {
    return Order(++next_id, Side::SELL, price, quantity);
  }
};

uint64_t OrderBookTest::next_id = 0;

TEST_F(OrderBookTest, CanAddBuyOrder) {
  Order order = createBuyOrder(100.0, 10);
  EXPECT_TRUE(book.addOrder(order));
  EXPECT_EQ(book.getBestBid(), 100.0);
}

TEST_F(OrderBookTest, CanAddSellOrder) {
  Order order = createSellOrder(100.0, 10);
  EXPECT_TRUE(book.addOrder(order));
  EXPECT_EQ(book.getBestAsk(), 100.0);
}

TEST_F(OrderBookTest, MaintainsBestBidPrices) {
  book.addOrder(createBuyOrder(100.0, 10));
  book.addOrder(createBuyOrder(101.0, 10));
  book.addOrder(createBuyOrder(99.0, 10));

  EXPECT_EQ(book.getBestBid(), 101.0);
}

TEST_F(OrderBookTest, MaintainsBestAskPrices) {
  book.addOrder(createSellOrder(100.0, 10));
  book.addOrder(createSellOrder(101.0, 10));
  book.addOrder(createSellOrder(99.0, 10));

  EXPECT_EQ(book.getBestAsk(), 99.0);
}

TEST_F(OrderBookTest, MatchesBuyWithExistingSell) {
  book.addOrder(createSellOrder(100.0, 10));
  Order buy = createBuyOrder(100.0, 10);
  auto result = book.matchOrder(buy);

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(book.getBestAsk(), 0.0);
}

TEST_F(OrderBookTest, MatchesSellWithExistingBuy) {
  book.addOrder(createBuyOrder(100.0, 10));

  Order sell = createSellOrder(100.0, 10);
  auto result = book.matchOrder(sell);

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(book.getBestBid(), 0.0);
}

TEST_F(OrderBookTest, HandlesPartialFills) {
  book.addOrder(createSellOrder(100.0, 10));
  Order buy = createBuyOrder(100.0, 5);
  auto result = book.matchOrder(buy);

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(book.getBestAsk(), 100.0);
}

TEST_F(OrderBookTest, RespectsLimitPrices) {
  book.addOrder(createBuyOrder(100.0, 10));
  Order sell = createSellOrder(101.0, 10);
  auto result = book.matchOrder(sell);
  EXPECT_FALSE(result.has_value());
}

TEST_F(OrderBookTest, HandlesConcurrentOrders) {
  const int num_threads = 4;
  std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([this, i]() {
      for (int j = 0; j < 100; ++j) {
        if (i % 2 == 0) {
          book.addOrder(createBuyOrder(100.0, 1));
        } else {
          book.addOrder(createSellOrder(100.0, 1));
        }
      }
    });
  }
  for (auto &thread : threads) {
    thread.join();
  }

  EXPECT_NO_THROW(book.getBestBid());
  EXPECT_NO_THROW(book.getBestAsk());
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
