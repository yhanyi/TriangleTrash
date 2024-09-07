#pragma once
#include <string>

class Player {
   public:
    Player(const std::string& name, double initialBalance);
    std::string name;
    double balance;
    int stocksOwned;
};