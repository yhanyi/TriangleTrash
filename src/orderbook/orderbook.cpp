#include "../../include/orderbook/orderbook.hpp"

#include <algorithm>
#include <condition_variable>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <vector>

namespace orderbook {

class OrderBook::Impl {
public:
  struct PriceLevel {
    std::vector<Order> orders;
    double total_quantity{0.0};
  };

  std::map<double, PriceLevel, std::greater<>> _bids; // Higher prices first
  std::map<double, PriceLevel> _asks;                 // Lower prices first

  mutable std::shared_mutex _book_mutex; // Protects both maps
  std::condition_variable_any _match_cv;

  bool tryMatchBuy(Order &buy_order);
  bool tryMatchSell(Order &sell_order);
  void cleanEmptyLevels();
};

OrderBook::OrderBook() : _pimpl(new Impl) {}
OrderBook::~OrderBook() { delete _pimpl; }

bool OrderBook::addOrder(const Order &order) {
  std::unique_lock write_lock(_pimpl->_book_mutex);

  if (order.getSide() == Side::BUY) {
    // Try matching with existing sell orders
    Order mutable_order = order;
    if (_pimpl->tryMatchBuy(mutable_order)) {
      return true;
    }

    // If no match, add to bids
    auto &level = _pimpl->_bids[order.getPrice()];
    level.orders.push_back(order);
    level.total_quantity += order.getQuantity();
  } else {
    // Try matching with existing buy orders
    Order mutable_order = order;
    if (_pimpl->tryMatchSell(mutable_order)) {
      return true;
    }

    // If no match, add to asks
    auto &level = _pimpl->_asks[order.getPrice()];
    level.orders.push_back(order);
    level.total_quantity += order.getQuantity();
  }

  _pimpl->_match_cv.notify_all();
  return true;
}

void OrderBook::clear() {
  std::unique_lock write_lock(_pimpl->_book_mutex);
  _pimpl->_bids.clear();
  _pimpl->_asks.clear();
}

bool OrderBook::Impl::tryMatchSell(Order &sell_order) {
  if (_bids.empty())
    return false;

  bool matched = false;

  for (auto it = _bids.begin(); it != _bids.end();) {
    if (it->first < sell_order.getPrice()) {
      break; // Bid price too low
    }

    auto &level = it->second;

    // Match orders at this price level
    auto order_it = level.orders.begin();
    while (order_it != level.orders.end()) {
      uint32_t trade_quantity =
          std::min(sell_order.getQuantity(), order_it->getQuantity());

      level.total_quantity -= trade_quantity;
      matched = true;

      if (order_it->getQuantity() == trade_quantity) {
        order_it = level.orders.erase(order_it);
      } else {
        ++order_it;
      }

      if (level.orders.empty()) {
        it = _bids.erase(it);
        break;
      }
    }

    if (it != _bids.end()) {
      ++it;
    }
  }

  return matched;
}

bool OrderBook::Impl::tryMatchBuy(Order &buy_order) {
  if (_asks.empty())
    return false;

  bool matched = false;

  for (auto it = _asks.begin(); it != _asks.end();) {
    if (it->first > buy_order.getPrice()) {
      break; // Ask price too high
    }

    auto &level = it->second;

    // Match orders at this price level
    auto order_it = level.orders.begin();
    while (order_it != level.orders.end()) {
      uint32_t trade_quantity =
          std::min(buy_order.getQuantity(), order_it->getQuantity());

      level.total_quantity -= trade_quantity;
      matched = true;

      if (order_it->getQuantity() == trade_quantity) {
        order_it = level.orders.erase(order_it);
      } else {
        ++order_it;
      }

      if (level.orders.empty()) {
        it = _asks.erase(it);
        break;
      }
    }

    if (it != _asks.end()) {
      ++it;
    }
  }

  return matched;
}

bool OrderBook::cancelOrder(uint64_t orderId) {
  std::unique_lock write_lock(_pimpl->_book_mutex);

  // Search in bids
  for (auto &[price, level] : _pimpl->_bids) {
    auto it = std::find_if(
        level.orders.begin(), level.orders.end(),
        [orderId](const Order &o) { return o.getId() == orderId; });

    if (it != level.orders.end()) {
      level.total_quantity -= it->getQuantity();
      level.orders.erase(it);
      _pimpl->cleanEmptyLevels();
      return true;
    }
  }

  // Search in asks
  for (auto &[price, level] : _pimpl->_asks) {
    auto it = std::find_if(
        level.orders.begin(), level.orders.end(),
        [orderId](const Order &o) { return o.getId() == orderId; });

    if (it != level.orders.end()) {
      level.total_quantity -= it->getQuantity();
      level.orders.erase(it);
      _pimpl->cleanEmptyLevels();
      return true;
    }
  }

  return false;
}

void OrderBook::Impl::cleanEmptyLevels() {
  auto bid_it = _bids.begin();
  while (bid_it != _bids.end()) {
    if (bid_it->second.orders.empty()) {
      bid_it = _bids.erase(bid_it);
    } else {
      ++bid_it;
    }
  }

  auto ask_it = _asks.begin();
  while (ask_it != _asks.end()) {
    if (ask_it->second.orders.empty()) {
      ask_it = _asks.erase(ask_it);
    } else {
      ++ask_it;
    }
  }
}

double OrderBook::getBestBid() const {
  std::shared_lock read_lock(_pimpl->_book_mutex);
  if (_pimpl->_bids.empty())
    return 0.0;
  return _pimpl->_bids.begin()->first;
}

double OrderBook::getBestAsk() const {
  std::shared_lock read_lock(_pimpl->_book_mutex);
  if (_pimpl->_asks.empty())
    return 0.0;
  return _pimpl->_asks.begin()->first;
}

std::optional<Order> OrderBook::matchOrder(const Order &order) {
  std::unique_lock write_lock(_pimpl->_book_mutex);

  Order mutable_order = order;
  bool matched = false;

  if (order.getSide() == Side::BUY) {
    matched = _pimpl->tryMatchBuy(mutable_order);
  } else {
    matched = _pimpl->tryMatchSell(mutable_order);
  }

  if (matched) {
    return mutable_order;
  }

  return std::nullopt;
}

} // namespace orderbook
