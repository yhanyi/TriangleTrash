#pragma once
#include "orderbook.hpp"
#include "player.hpp"
#include <random>

class Bot : public Player {
public:
  Bot(const std::string &name, double initialBalance);
  void makeDecision(OrderBook &orderBook);

private:
  std::mt19937 rng;
  std::uniform_real_distribution<> actionDist;
  std::uniform_real_distribution<> priceDist;
  std::uniform_int_distribution<> quantityDist;
};
