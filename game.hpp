#pragma once
#include <string>
#include <vector>

#include "orderbook.hpp"
#include "player.hpp"

class Game {
   public:
    Game(const std::string& roomCode);
    void addPlayer(Player* player);
    void processOrder(const std::string& playerName, const std::string& orderStr);
    std::string getGameState() const;
    void notifyClients();

   private:
    std::string roomCode;
    std::vector<Player*> players;
    OrderBook orderBook;
    Player* findPlayer(const std::string& name);
};