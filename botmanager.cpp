#include "botmanager.hpp"

BotManager::BotManager(int numBots, double initialBalance) {
  for (int i = 0; i < numBots; ++i) {
    bots.emplace_back("Bot" + std::to_string(i + 1), initialBalance);
  }
}

void BotManager::updateBots(OrderBook &orderBook) {
  if (orderBook.isMarketActive()) {
    for (auto &bot : bots) {
      bot.makeDecision(orderBook);
    }
  }
}
