#include "bot.hpp"
#include <cmath>
#include <ctime>

Bot::Bot(const std::string &name, double initialBalance)
    : Player(name, initialBalance), rng(std::random_device{}()),
      actionDist(0.0, 1.0), priceDist(0.95, 1.05), quantityDist(1, 10) {}

void Bot::makeDecision(OrderBook &orderBook) {
  if (!orderBook.isMarketActive())
    return; // Do nothing if market is inactive.

  double currentPrice = orderBook.getCurrentPrice();
  if (currentPrice <= 0)
    return; // Do nothing if invalid price.

  double action = actionDist(rng);

  if (action < 0.4) { // 40% chance to buy
    int quantity = quantityDist(rng);
    int price = static_cast<int>(std::round(currentPrice * priceDist(rng)));
    if (canBuy(quantity, price)) {
      orderBook.addOrder(Order(quantity, price, OrderType::BID, this));
    }
  } else if (action < 0.8) { // 40% chance to sell
    int quantity = std::min(quantityDist(rng), stocksOwned);
    int price = static_cast<int>(std::round(currentPrice * priceDist(rng)));
    if (quantity > 0) {
      orderBook.addOrder(Order(quantity, price, OrderType::ASK, this));
    }
  }
  // 20% chance to do nothing
}
