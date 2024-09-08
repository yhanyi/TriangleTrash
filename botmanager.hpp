#pragma once
#include "bot.hpp"
#include <vector>

class BotManager {
public:
  BotManager(int numBots, double initialBalance);
  void updateBots(OrderBook &orderBook);
  std::vector<Bot> &getBots() { return bots; }

private:
  std::vector<Bot> bots;
};
