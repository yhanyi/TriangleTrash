#pragma once
#include "player.hpp"

enum class OrderType { BID,
                       ASK,
                       MARKET_BUY,
                       MARKET_SELL };

class Order {
   public:
    Order(int quantity, double price, OrderType type, Player* player);
    Order(int quantity, OrderType type, Player* player);

    int quantity;
    double price;
    OrderType type;
    Player* player;
};