#include "player.hpp"

Player::Player(const std::string& name, double initialBalance)
    : name(name), balance(initialBalance), stocksOwned(100) {}  // Start with 100 stocks

bool Player::canBuy(int quantity, double price) const {
    return balance >= quantity * price;
}

bool Player::canSell(int quantity) const {
    return stocksOwned >= quantity;
}

double Player::getTotalWorth(double currentStockPrice) const {
    return balance + (stocksOwned * currentStockPrice);
}