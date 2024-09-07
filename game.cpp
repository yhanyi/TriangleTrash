#include "game.hpp"

Game::Game(const std::string& roomCode) : roomCode(roomCode) {}

void Game::addPlayer(Player* player) {
    players.push_back(player);
}

void Game::processOrder(const std::string& playerName, const std::string& orderStr) {
    // TODO: Implement game logic later.
}

std::string Game::getGameState() const {
    // TODO: Implement gamestate retrieval.
    return "Game State";  // Placeholder
}