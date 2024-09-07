#include "player.hpp"

Player::Player(const std::string& name, double initialBalance)
    : name(name), balance(initialBalance), stocksOwned(0) {}