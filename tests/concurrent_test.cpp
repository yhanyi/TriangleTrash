#include "../include/orderbook/order_allocator.hpp"
#include "../include/orderbook/orderbook.hpp"
#include "../include/session/session.hpp"
#include <future>
#include <gtest/gtest.h>
#include <random>
#include <thread>
#include <vector>

using namespace orderbook;
using namespace session;

class ConcurrentOrderBookTest : public ::testing::Test {
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

  Order *createRandomOrder(Side side, double price_min, double price_max,
                           uint32_t qty_min, uint32_t qty_max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> price_dist(price_min, price_max);
    static std::uniform_int_distribution<> qty_dist(qty_min, qty_max);

    return OrderAllocator::create(++next_id, side, price_dist(gen),
                                  qty_dist(gen));
  }

  std::vector<Order *> generateRandomOrders(size_t count, Side side,
                                            double price_min, double price_max,
                                            uint32_t qty_min,
                                            uint32_t qty_max) {
    std::vector<Order *> orders;
    orders.reserve(count);
    for (size_t i = 0; i < count; ++i) {
      orders.push_back(
          createRandomOrder(side, price_min, price_max, qty_min, qty_max));
    }
    return orders;
  }

  void destroyOrders(std::vector<Order *> &orders) {
    for (auto *order : orders) {
      OrderAllocator::destroy(order);
    }
    orders.clear();
  }
};

uint64_t ConcurrentOrderBookTest::next_id = 0;

TEST_F(ConcurrentOrderBookTest, ConcurrentOrderAddition) {
  const size_t num_orders = 1000;
  const int num_threads = 4;
  std::vector<std::thread> threads;

  // Generate random orders
  auto buy_orders =
      generateRandomOrders(num_orders, Side::BUY, 90.0, 110.0, 1, 100);
  auto sell_orders =
      generateRandomOrders(num_orders, Side::SELL, 90.0, 110.0, 1, 100);

  // Add orders concurrently
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([this, &buy_orders, &sell_orders, i, num_threads]() {
      size_t start_idx = (i * buy_orders.size()) / num_threads;
      size_t end_idx = ((i + 1) * buy_orders.size()) / num_threads;

      for (size_t j = start_idx; j < end_idx; ++j) {
        book.addOrder(*buy_orders[j]);
        book.addOrder(*sell_orders[j]);
      }
    });
  }

  for (auto &thread : threads) {
    thread.join();
  }

  // Verify no price inversions
  double last_bid = book.getBestBid();
  double last_ask = book.getBestAsk();
  EXPECT_GT(last_bid, 0.0);
  EXPECT_GT(last_ask, 0.0);
  EXPECT_GE(last_ask, last_bid);

  // Cleanup
  destroyOrders(buy_orders);
  destroyOrders(sell_orders);
}

TEST_F(ConcurrentOrderBookTest, BatchOrderProcessing) {
  const size_t batch_size = 100;
  auto buy_orders =
      generateRandomOrders(batch_size, Side::BUY, 95.0, 105.0, 1, 50);
  auto sell_orders =
      generateRandomOrders(batch_size, Side::SELL, 95.0, 105.0, 1, 50);

  // Process batch of orders
  std::vector<std::future<bool>> results;

  // Submit buy orders
  for (const auto *order : buy_orders) {
    results.push_back(std::async(
        std::launch::async, [this, order]() { return book.addOrder(*order); }));
  }

  // Submit sell orders
  for (const auto *order : sell_orders) {
    results.push_back(std::async(
        std::launch::async, [this, order]() { return book.addOrder(*order); }));
  }

  // Wait for all orders to be processed
  for (auto &result : results) {
    EXPECT_TRUE(result.get());
  }

  // Cleanup
  destroyOrders(buy_orders);
  destroyOrders(sell_orders);
}

TEST_F(ConcurrentOrderBookTest, StressTestMatching) {
  const size_t num_orders = 500;
  const int num_iterations = 10;

  std::vector<std::future<std::optional<Order>>> match_results;
  match_results.reserve(num_orders * 2);

  for (int iter = 0; iter < num_iterations; ++iter) {
    auto buy_orders =
        generateRandomOrders(num_orders, Side::BUY, 98.0, 102.0, 1, 20);
    auto sell_orders =
        generateRandomOrders(num_orders, Side::SELL, 98.0, 102.0, 1, 20);

    // Submit matching requests concurrently
    for (const auto *order : buy_orders) {
      match_results.push_back(std::async(std::launch::async, [this, order]() {
        return book.matchOrder(*order);
      }));
    }

    for (const auto *order : sell_orders) {
      match_results.push_back(std::async(std::launch::async, [this, order]() {
        return book.matchOrder(*order);
      }));
    }

    // Cleanup this iteration's orders
    destroyOrders(buy_orders);
    destroyOrders(sell_orders);
  }

  // Verify all matches completed
  for (auto &result : match_results) {
    result.wait();
  }
}

TEST_F(ConcurrentOrderBookTest, ConsistencyUnderLoad) {
  const size_t num_orders = 1000;
  std::atomic<size_t> matched_orders{0};
  std::atomic<size_t> unmatched_orders{0};

  auto buy_orders =
      generateRandomOrders(num_orders, Side::BUY, 95.0, 105.0, 1, 50);
  auto sell_orders =
      generateRandomOrders(num_orders, Side::SELL, 95.0, 105.0, 1, 50);

  std::vector<std::future<void>> tasks;

  // Submit buy and sell orders concurrently
  for (size_t i = 0; i < num_orders; ++i) {
    tasks.push_back(
        std::async(std::launch::async, [this, &matched_orders,
                                        &unmatched_orders, &buy_orders, i]() {
          auto result = book.matchOrder(*buy_orders[i]);
          if (result)
            matched_orders++;
          else
            unmatched_orders++;
        }));

    tasks.push_back(
        std::async(std::launch::async, [this, &matched_orders,
                                        &unmatched_orders, &sell_orders, i]() {
          auto result = book.matchOrder(*sell_orders[i]);
          if (result)
            matched_orders++;
          else
            unmatched_orders++;
        }));
  }

  // Wait for all tasks to complete
  for (auto &task : tasks) {
    task.wait();
  }

  // Verify total number of orders processed
  EXPECT_EQ(matched_orders + unmatched_orders, num_orders * 2);
}
