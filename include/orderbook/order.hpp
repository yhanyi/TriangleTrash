#pragma once

#include <cstddef>
#include <cstdint>

namespace orderbook {

class OrderAllocator;
template <typename, size_t> class MemoryPool;

enum class Side { BUY, SELL };

class Order {
  friend class OrderAllocator;
  template <typename T, size_t S> friend class MemoryPool;

public:
  uint64_t getId() const;
  Side getSide() const;
  double getPrice() const;
  uint32_t getQuantity() const;

  // Allow copy/move for STL containers
  Order(const Order &) = default;
  Order &operator=(const Order &) = default;
  Order(Order &&) = default;
  Order &operator=(Order &&) = default;

private:
  Order(uint64_t id, Side side, double price, uint32_t quantity);
  uint64_t _id;
  Side _side;
  double _price;
  uint32_t _quantity;
};

} // namespace orderbook
