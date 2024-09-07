#pragma once
#include <map>
#include <string>

#include "game.hpp"

class Server {
   public:
    Server(int port);
    void start();
    void createRoom(const std::string& roomCode);
    Game* getRoom(const std::string& roomCode);

   private:
    int port;
    std::map<std::string, Game> rooms;
    void handleClient(int clientSocket);
};