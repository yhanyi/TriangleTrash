#include "game.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>

Game::Game(const std::string& roomCode) : roomCode(roomCode) {}

void Game::addPlayer(Player* player) {
    players.push_back(player);
}

void Game::processOrder(const std::string& playerName, const std::string& orderStr) {
    std::istringstream iss(orderStr);
    std::string action;
    int quantity;
    char at;
    double price;

    iss >> action >> quantity >> at >> price;

    Player* player = findPlayer(playerName);
    if (!player) {
        std::cout << "Player not found.\n";
        return;
    }

    OrderType orderType = (action == "bid") ? OrderType::BID : OrderType::ASK;
    Order order(quantity, price, orderType, player);

    orderBook.addOrder(order);
    orderBook.matchOrders();
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