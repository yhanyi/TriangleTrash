#include "../../include/orderbook/order.hpp"

namespace orderbook {

Order::Order(uint64_t id, Side side, double price, uint32_t quantity)
    : _id(id), _side(side), _price(price), _quantity(quantity) {}

uint64_t Order::getId() const { return _id; }
Side Order::getSide() const { return _side; }
double Order::getPrice() const { return _price; }
uint32_t Order::getQuantity() const { return _quantity; }

} // namespace orderbook
