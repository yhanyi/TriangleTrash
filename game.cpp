#include "game.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

Game::Game(const std::string &roomCode) : roomCode(roomCode) {}

void Game::addPlayer(Player *player) { players.push_back(player); }

void Game::processOrder(const std::string &playerName,
                        const std::string &orderStr) {
  std::istringstream iss(orderStr);
  std::string action;
  int quantity;
  iss >> action >> quantity;

  Player *player = findPlayer(playerName);
  if (!player) {
    std::cout << "Player not found.\n";
    return;
  }

  if (action == "write_call" || action == "write_put") {
    int quantity;
    double strikePrice;
    int daysToExpire;
    iss >> quantity >> strikePrice >> daysToExpire;

    OptionType optionType =
        (action == "write_call") ? OptionType::CALL : OptionType::PUT;
    std::time_t expirationTime = std::time(nullptr) + daysToExpire * 24 * 3600;

    Option newOption(optionType, strikePrice, quantity, player, expirationTime);
    orderBook.writeOption(newOption);
  } else if (action == "buy_option") {
    int optionIndex;
    iss >> optionIndex;

    auto availableOptions = orderBook.getAvailableOptions();
    if (optionIndex >= 0 && optionIndex < availableOptions.size()) {
      Option &chosenOption = availableOptions[optionIndex];
      double premium = chosenOption.getPremium(orderBook.getCurrentPrice());
      if (player->balance >= premium) {
        player->balance -= premium;
        chosenOption.writer->balance += premium;
        chosenOption.holder = player;
      } else {
        std::cout << "Insufficient funds to buy this option.\n";
      }
    } else {
      std::cout << "Invalid option index.\n";
    }
  } else if (action == "exercise_option") {
    int optionIndex;
    iss >> optionIndex;

    auto availableOptions = orderBook.getAvailableOptions();
    if (optionIndex >= 0 && optionIndex < availableOptions.size()) {
      Option &chosenOption = availableOptions[optionIndex];
      if (chosenOption.holder == player) {
        orderBook.exerciseOption(chosenOption, player);
      } else {
        std::cout << "You don't own this option.\n";
      }
    } else {
      std::cout << "Invalid option index.\n";
    }
  } else {
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
        double currentPrice = orderBook.getCurrentPrice();
        player->balance += quantity * currentPrice;
        player->stocksOwned -= quantity;
        std::cout << "Sold " << quantity << " stocks at $" << currentPrice
                  << " each.\n";
        orderBook.addOrder(Order(quantity, OrderType::MARKET_SELL, player));
      }
    } else {
      std::cout << "Invalid action. Use 'bid', 'ask', 'buy', or 'sell'.\n";
      return;
    }
  }
}

std::string Game::getGameState() const {
  std::ostringstream oss;
  oss << "Room Code: " << roomCode << "\n";
  oss << "Players:\n";

  // Sort players by profit
  std::vector<Player *> sortedPlayers = players;
  std::sort(sortedPlayers.begin(), sortedPlayers.end(),
            [](const Player *a, const Player *b) {
              return a->getProfit() > b->getProfit();
            });

  for (const auto &player : sortedPlayers) {
    double profit = player->getProfit();
    oss << player->name << " - Balance: $" << std::fixed << std::setprecision(2)
        << player->balance << ", Stocks: " << player->stocksOwned
        << ", Profit: $" << std::fixed << std::setprecision(2) << profit
        << (profit > 0 ? " ▲" : (profit < 0 ? " ▼" : " =")) << "\n";
  }
  oss << "\n" << orderBook.getOrderBookDisplay();

  oss << "\nAvailable Options:\n";
  auto availableOptions = orderBook.getAvailableOptions();
  for (size_t i = 0; i < availableOptions.size(); ++i) {
    const auto &option = availableOptions[i];
    oss << i << ": " << (option.type == OptionType::CALL ? "CALL" : "PUT")
        << " Strike: " << option.strikePrice << " Qty: " << option.quantity
        << " Expires: " << std::ctime(&option.expirationTime);
  }

  return oss.str();
}

Player *Game::findPlayer(const std::string &name) {
  auto it = std::find_if(players.begin(), players.end(),
                         [&name](const Player *p) { return p->name == name; });
  return (it != players.end()) ? *it : nullptr;
}
