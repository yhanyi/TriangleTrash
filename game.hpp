#pragma once
#include <string>
#include <vector>

#include "botmanager.hpp"
#include "orderbook.hpp"
#include "player.hpp"

class Game {
public:
  Game(const std::string &roomCode, int numBots = 5, bool botsEnabled = true);
  void addPlayer(Player *player);
  void processOrder(const std::string &playerName, const std::string &orderStr);
  std::string getGameState() const;
  void notifyClients();
  void updateBots();
  bool areBotsEnabled() const { return botsEnabled; }

private:
  std::string roomCode;
  std::vector<Player *> players;
  OrderBook orderBook;
  Player *findPlayer(const std::string &name);
  BotManager botManager;
  bool botsEnabled;
};
