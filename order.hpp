#pragma once
#include <string>

enum class OrderType { BID,
                       ASK };

class Player;

class Order {
   public:
    Order(int quantity, double price, OrderType type, Player* player);
    int quantity;
    double price;
    OrderType type;
    Player* player;
};