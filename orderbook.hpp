#pragma once
#include <map>
#include <string>
#include <vector>

#include "order.hpp"

class OrderBook {
   public:
    void addOrder(Order order);
    void matchOrders();
    std::string getOrderBookDisplay() const;
    double getLowestAskPrice() const;
    double getHighestBidPrice() const;

   private:
    std::map<double, std::vector<Order>, std::greater<double>> bids;
    std::map<double, std::vector<Order>, std::less<double>> asks;
    void executeMarketOrder(Order& order);
    void executeTransaction(Player* buyer, Player* seller, int quantity, double price);
};