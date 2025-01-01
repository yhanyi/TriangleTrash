#pragma once

#include <cstdint>

namespace orderbook {

enum class Side { BUY, SELL };

class Order {
public:
  Order(uint64_t id, Side side, double price, uint32_t quantity);
  uint64_t getId() const;
  Side getSide() const;
  double getPrice() const;
  uint32_t getQuantity() const;

private:
  uint64_t _id;
  Side _side;
  double _price;
  uint32_t _quantity;
};

} // namespace orderbook
