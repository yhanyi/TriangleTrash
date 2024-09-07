#pragma once
#include <ctime>
#include <string>

#include "player.hpp"

enum class OptionType { CALL,
                        PUT };

class Option {
   public:
    Option(OptionType type, double strikePrice, int quantity, Player* writer, std::time_t expirationTime);

    OptionType type;
    double strikePrice;
    int quantity;
    Player* writer;
    Player* holder;
    std::time_t expirationTime;
    bool isExercised;

    double getPremium(double currentPrice) const;
    bool canExercise(double currentPrice) const;
};