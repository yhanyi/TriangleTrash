#include "../../include/orderbook/orderbook.hpp"
#include "../../include/network/thread_pool.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <shared_mutex>

namespace orderbook {

class OrderBook::Impl {
public:
  Impl() : _thread_pool(std::make_unique<network::ThreadPool>()) {
    // Initialise with less threads to avoid oversubscription
    _thread_pool->init(std::max(2u, std::thread::hardware_concurrency() / 2));
  }

  struct PriceLevel {
    std::vector<Order> orders;
    double total_quantity{0.0};
  };

  std::map<double, PriceLevel, std::greater<>> _bids; // Higher prices first
  std::map<double, PriceLevel> _asks;                 // Lower prices first
  mutable std::shared_mutex _book_mutex;
  std::unique_ptr<network::ThreadPool> _thread_pool;
};

OrderBook::OrderBook() : _pimpl(new Impl) {}
OrderBook::~OrderBook() { delete _pimpl; }

bool OrderBook::addOrder(const Order &order) {
  std::unique_lock<std::shared_mutex> lock(_pimpl->_book_mutex);

  if (order.getSide() == Side::BUY) {
    // Try matching with existing sell orders
    if (!_pimpl->_asks.empty()) {
      auto ask_it = _pimpl->_asks.begin();
      if (ask_it->first <= order.getPrice()) {
        auto &level = ask_it->second;
        if (!level.orders.empty()) {
          // Match found
          auto &matching_order = level.orders.front();
          uint32_t trade_quantity =
              std::min(order.getQuantity(), matching_order.getQuantity());

          level.total_quantity -= trade_quantity;

          if (matching_order.getQuantity() == trade_quantity) {
            level.orders.erase(level.orders.begin());
            if (level.orders.empty()) {
              _pimpl->_asks.erase(ask_it);
            }
          }
          return true;
        }
      }
    }

    // No match found, add to bids
    auto &level = _pimpl->_bids[order.getPrice()];
    level.orders.push_back(order);
    level.total_quantity += order.getQuantity();
  } else {
    // Try matching with existing buy orders
    if (!_pimpl->_bids.empty()) {
      auto bid_it = _pimpl->_bids.begin();
      if (bid_it->first >= order.getPrice()) {
        auto &level = bid_it->second;
        if (!level.orders.empty()) {
          // Match found
          auto &matching_order = level.orders.front();
          uint32_t trade_quantity =
              std::min(order.getQuantity(), matching_order.getQuantity());

          level.total_quantity -= trade_quantity;

          if (matching_order.getQuantity() == trade_quantity) {
            level.orders.erase(level.orders.begin());
            if (level.orders.empty()) {
              _pimpl->_bids.erase(bid_it);
            }
          }
          return true;
        }
      }
    }

    // No match found, add to asks
    auto &level = _pimpl->_asks[order.getPrice()];
    level.orders.push_back(order);
    level.total_quantity += order.getQuantity();
  }

  return true;
}

void OrderBook::clear() {
  std::unique_lock<std::shared_mutex> lock(_pimpl->_book_mutex);
  _pimpl->_bids.clear();
  _pimpl->_asks.clear();
}

std::optional<Order> OrderBook::matchOrder(const Order &order) {
  std::unique_lock write_lock(_pimpl->_book_mutex);

  if (order.getSide() == Side::BUY) {
    if (_pimpl->_asks.empty())
      return std::nullopt;

    // Try to match with best ask
    auto ask_it = _pimpl->_asks.begin();
    if (ask_it->first > order.getPrice()) {
      return std::nullopt; // No matching price
    }

    auto &level = ask_it->second;
    if (!level.orders.empty()) {
      // Make a copy of the matching order before potentially erasing it
      Order matched_order = level.orders.front();
      uint32_t trade_quantity =
          std::min(order.getQuantity(), matched_order.getQuantity());

      level.total_quantity -= trade_quantity;

      if (matched_order.getQuantity() == trade_quantity) {
        level.orders.erase(level.orders.begin());
        if (level.orders.empty()) {
          _pimpl->_asks.erase(ask_it);
        }
      }

      return matched_order;
    }
  } else {
    if (_pimpl->_bids.empty())
      return std::nullopt;

    // Try to match with best bid
    auto bid_it = _pimpl->_bids.begin();
    if (bid_it->first < order.getPrice()) {
      return std::nullopt; // No matching price
    }

    auto &level = bid_it->second;
    if (!level.orders.empty()) {
      // Make a copy of the matching order before potentially erasing it
      Order matched_order = level.orders.front();
      uint32_t trade_quantity =
          std::min(order.getQuantity(), matched_order.getQuantity());

      level.total_quantity -= trade_quantity;

      if (matched_order.getQuantity() == trade_quantity) {
        level.orders.erase(level.orders.begin());
        if (level.orders.empty()) {
          _pimpl->_bids.erase(bid_it);
        }
      }

      return matched_order;
    }
  }

  return std::nullopt;
}

bool OrderBook::cancelOrder(uint64_t orderId) {
  std::unique_lock<std::shared_mutex> lock(_pimpl->_book_mutex);

  auto remove_order = [](auto &orders, uint64_t id,
                         double &total_quantity) -> bool {
    auto it = std::find_if(orders.begin(), orders.end(),
                           [id](const Order &o) { return o.getId() == id; });

    if (it != orders.end()) {
      total_quantity -= it->getQuantity();
      orders.erase(it);
      return true;
    }
    return false;
  };

  for (auto &[price, level] : _pimpl->_bids) {
    if (remove_order(level.orders, orderId, level.total_quantity)) {
      if (level.orders.empty()) {
        _pimpl->_bids.erase(price);
      }
      return true;
    }
  }

  for (auto &[price, level] : _pimpl->_asks) {
    if (remove_order(level.orders, orderId, level.total_quantity)) {
      if (level.orders.empty()) {
        _pimpl->_asks.erase(price);
      }
      return true;
    }
  }

  return false;
}

double OrderBook::getBestBid() const {
  std::shared_lock<std::shared_mutex> lock(_pimpl->_book_mutex);
  if (_pimpl->_bids.empty()) {
    return 0.0;
  }
  return _pimpl->_bids.begin()->first;
}

double OrderBook::getBestAsk() const {
  std::shared_lock<std::shared_mutex> lock(_pimpl->_book_mutex);
  if (_pimpl->_asks.empty()) {
    return 0.0;
  }
  return _pimpl->_asks.begin()->first;
}

} // namespace orderbook
