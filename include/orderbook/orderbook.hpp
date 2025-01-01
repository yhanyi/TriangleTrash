#pragma once

#include "order.hpp"
#include <optional>

namespace orderbook {

class OrderBook {
public:
  OrderBook();
  ~OrderBook();
  bool addOrder(const Order &order);
  bool cancelOrder(uint64_t orderId);
  std::optional<Order> matchOrder(const Order &order);
  double getBestBid() const;
  double getBestAsk() const;
  void clear();

private:
  class Impl;
  Impl *_pimpl;
};

} // namespace orderbook
