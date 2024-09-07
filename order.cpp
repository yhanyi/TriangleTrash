#include "order.hpp"

Order::Order(int quantity, double price, OrderType type, Player* player)
    : quantity(quantity), price(price), type(type), player(player) {}