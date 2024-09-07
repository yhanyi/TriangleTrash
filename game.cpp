#include "game.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

Game::Game(const std::string& roomCode) : roomCode(roomCode) {}

void Game::addPlayer(Player* player) {
    players.push_back(player);
}

void Game::processOrder(const std::string& playerName, const std::string& orderStr) {
    std::istringstream iss(orderStr);
    std::string action;
    int quantity;
    iss >> action >> quantity;

    Player* player = findPlayer(playerName);
    if (!player) {
        std::cout << "Player not found.\n";
        return;
    }

    if (action == "bid" || action == "ask") {
        char at;
        double price;
        iss >> at >> price;

        if (action == "bid") {
            if (!player->canBuy(quantity, price)) {
                std::cout << "Insufficient funds to place this bid.\n";
                return;
            }
            orderBook.addOrder(Order(quantity, price, OrderType::BID, player));
        } else {
            if (!player->canSell(quantity)) {
                std::cout << "Insufficient stocks to place this ask.\n";
                return;
            }
            orderBook.addOrder(Order(quantity, price, OrderType::ASK, player));
        }
    } else if (action == "buy" || action == "sell") {
        if (action == "buy") {
            double lowestAskPrice = orderBook.getLowestAskPrice();
            if (lowestAskPrice == std::numeric_limits<double>::max()) {
                std::cout << "No asks available for market buy.\n";
                return;
            }
            if (!player->canBuy(quantity, lowestAskPrice)) {
                std::cout << "Insufficient funds to place this market buy order.\n";
                return;
            }
            orderBook.addOrder(Order(quantity, OrderType::MARKET_BUY, player));
        } else {
            if (!player->canSell(quantity)) {
                std::cout << "Insufficient stocks to place this market sell order.\n";
                return;
            }
            orderBook.addOrder(Order(quantity, OrderType::MARKET_SELL, player));
        }
    } else {
        std::cout << "Invalid action. Use 'bid', 'ask', 'buy', or 'sell'.\n";
        return;
    }
}

std::string Game::getGameState() const {
    std::ostringstream oss;
    oss << "Room Code: " << roomCode << "\n";
    oss << "Players:\n";
    for (const auto& player : players) {
        oss << player->name << " - Balance: $" << std::fixed << std::setprecision(2)
            << player->balance << ", Stocks: " << player->stocksOwned << "\n";
    }
    oss << "\n"
        << orderBook.getOrderBookDisplay();
    return oss.str();
}

Player* Game::findPlayer(const std::string& name) {
    auto it = std::find_if(players.begin(), players.end(),
                           [&name](const Player* p) { return p->name == name; });
    return (it != players.end()) ? *it : nullptr;
}