#include "order.hpp"

Order::Order(int quantity, double price, OrderType type, Player* player)
    : quantity(quantity), price(price), type(type), player(player) {}

Order::Order(int quantity, OrderType type, Player* player)
    : quantity(quantity), price(0), type(type), player(player) {}