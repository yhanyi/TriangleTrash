#pragma once
#include <map>
#include <string>
#include <vector>

#include "option.hpp"
#include "order.hpp"

class OrderBook {
public:
  void addOrder(Order order);
  void matchOrders();
  std::string getOrderBookDisplay() const;
  double getLowestAskPrice() const;
  double getHighestBidPrice() const;
  void writeOption(Option option);
  void exerciseOption(Option &option, Player *player);
  std::vector<Option> getAvailableOptions() const;
  double getCurrentPrice() const;
  bool isMarketActive() const;

private:
  std::map<double, std::vector<Order>, std::greater<double>> bids;
  std::map<double, std::vector<Order>, std::less<double>> asks;
  void executeMarketOrder(Order &order);
  void executeTransaction(Player *buyer, Player *seller, int quantity,
                          double price);
  std::vector<Option> options;
};
