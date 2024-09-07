#pragma once
#include <map>
#include <string>
#include <vector>

#include "order.hpp"

class OrderBook {
   public:
    void addOrder(const Order& order);
    void matchOrders();
    std::string getOrderBookDisplay() const;

   private:
    std::map<double, std::vector<Order>, std::greater<double>> bids;
    std::map<double, std::vector<Order>, std::less<double>> asks;
};