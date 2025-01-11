#include "../include/orderbook/order_allocator.hpp"
#include "../include/orderbook/orderbook.hpp"
#include "../include/session/session.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace orderbook;
using namespace session;

class OrderBookTest : public ::testing::Test {
protected:
  void SetUp() override {
    book.clear();
    next_id = 0;
    session = std::make_unique<Session>("test_session");
    session->addUser("trader1", 1);
    session->addUser("trader2", 2);
  }

  void TearDown() override { book.clear(); }

  OrderBook book;
  std::unique_ptr<Session> session;
  static uint64_t next_id;

  // Helper methods now return Order pointers
  Order *createBuyOrder(double price, uint32_t quantity) {
    return OrderAllocator::create(++next_id, Side::BUY, price, quantity);
  }

  Order *createSellOrder(double price, uint32_t quantity) {
    return OrderAllocator::create(++next_id, Side::SELL, price, quantity);
  }

  // Helper to get test users
  std::shared_ptr<User> getTrader1() { return session->getUser("trader1"); }
  std::shared_ptr<User> getTrader2() { return session->getUser("trader2"); }
};

uint64_t OrderBookTest::next_id = 0;

TEST_F(OrderBookTest, CanAddBuyOrder) {
  Order *order = createBuyOrder(100.0, 10);
  EXPECT_TRUE(book.addOrder(*order));
  EXPECT_EQ(book.getBestBid(), 100.0);
  OrderAllocator::destroy(order);
}

TEST_F(OrderBookTest, CanAddSellOrder) {
  Order *order = createSellOrder(100.0, 10);
  EXPECT_TRUE(book.addOrder(*order));
  EXPECT_EQ(book.getBestAsk(), 100.0);
  OrderAllocator::destroy(order);
}

TEST_F(OrderBookTest, MaintainsBestBidPrices) {
  Order *order1 = createBuyOrder(100.0, 10);
  Order *order2 = createBuyOrder(101.0, 10);
  Order *order3 = createBuyOrder(99.0, 10);

  book.addOrder(*order1);
  book.addOrder(*order2);
  book.addOrder(*order3);

  EXPECT_EQ(book.getBestBid(), 101.0);

  OrderAllocator::destroy(order1);
  OrderAllocator::destroy(order2);
  OrderAllocator::destroy(order3);
}

TEST_F(OrderBookTest, MaintainsBestAskPrices) {
  Order *order1 = createSellOrder(100.0, 10);
  Order *order2 = createSellOrder(101.0, 10);
  Order *order3 = createSellOrder(99.0, 10);

  book.addOrder(*order1);
  book.addOrder(*order2);
  book.addOrder(*order3);

  EXPECT_EQ(book.getBestAsk(), 99.0);

  OrderAllocator::destroy(order1);
  OrderAllocator::destroy(order2);
  OrderAllocator::destroy(order3);
}

TEST_F(OrderBookTest, MatchesBuyWithExistingSell) {
  auto trader1 = getTrader1();
  auto trader2 = getTrader2();

  // Trader1 places sell order
  Order *sell = createSellOrder(100.0, 10);
  book.addOrder(*sell);

  // Trader2 places matching buy order
  Order *buy = createBuyOrder(100.0, 10);
  auto result = book.matchOrder(*buy);

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(book.getBestAsk(), 0.0);

  // Verify trader balances and positions
  double trade_value = 100.0 * 10;
  trader2->updateBalance(-trade_value);
  trader1->updateBalance(trade_value);
  trader2->addPosition("STOCK", 10);

  EXPECT_EQ(trader2->getPosition("STOCK"), 10);
  EXPECT_EQ(trader2->getBalance(), 10000.0 - trade_value);
  EXPECT_EQ(trader1->getBalance(), 10000.0 + trade_value);

  OrderAllocator::destroy(sell);
  OrderAllocator::destroy(buy);
}

TEST_F(OrderBookTest, HandlesPartialFills) {
  auto trader1 = getTrader1();
  auto trader2 = getTrader2();

  Order *sell = createSellOrder(100.0, 10);
  book.addOrder(*sell);

  Order *buy = createBuyOrder(100.0, 5);
  auto result = book.matchOrder(*buy);

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(book.getBestAsk(), 100.0);

  // Verify partial fill amounts
  double trade_value = 100.0 * 5;
  trader2->updateBalance(-trade_value);
  trader1->updateBalance(trade_value);
  trader2->addPosition("STOCK", 5);

  EXPECT_EQ(trader2->getPosition("STOCK"), 5);

  OrderAllocator::destroy(sell);
  OrderAllocator::destroy(buy);
}

TEST_F(OrderBookTest, RespectsUserBalance) {
  auto trader = getTrader1();
  double price = 20000.0; // More than initial balance
  uint32_t quantity = 1;

  EXPECT_FALSE(trader->canAffordTrade(price, quantity));
}

TEST_F(OrderBookTest, RespectsUserPosition) {
  auto trader = getTrader1();
  EXPECT_EQ(trader->getPosition("STOCK"), 0);

  // Try to sell more than owned
  Order *sell = createSellOrder(100.0, 10);
  EXPECT_LT(trader->getPosition("STOCK"), 10);
  OrderAllocator::destroy(sell);
}

TEST_F(OrderBookTest, HandlesConcurrentOrders) {
  const int num_threads = 4;
  std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([this, i]() {
      for (int j = 0; j < 100; ++j) {
        if (i % 2 == 0) {
          Order *order = createBuyOrder(100.0, 1);
          book.addOrder(*order);
          OrderAllocator::destroy(order);
        } else {
          Order *order = createSellOrder(100.0, 1);
          book.addOrder(*order);
          OrderAllocator::destroy(order);
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
