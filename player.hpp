#pragma once
#include <string>

class Player {
public:
  Player(const std::string &name, double initialBalance);
  virtual ~Player() = default;
  std::string name;
  double balance;
  double initialBalance;
  int stocksOwned;
  bool canBuy(int quantity, double price) const;
  bool canSell(int quantity) const;
  double getProfit() const;
};
