#include "option.hpp"

#include <cmath>

Option::Option(OptionType type, double strikePrice, int quantity, Player* writer, std::time_t expirationTime)
    : type(type), strikePrice(strikePrice), quantity(quantity), writer(writer), holder(nullptr), expirationTime(expirationTime), isExercised(false) {}

double Option::getPremium(double currentPrice) const {
    // TODO: Improve calculation
    double timeFactor = std::max(0.0, static_cast<double>(expirationTime - std::time(nullptr)) / (24 * 3600));
    return std::abs(currentPrice - strikePrice) * 0.1 * timeFactor;
}

bool Option::canExercise(double currentPrice) const {
    if (std::time(nullptr) > expirationTime) return false;
    if (type == OptionType::CALL) {
        return currentPrice > strikePrice;
    } else {
        return currentPrice < strikePrice;
    }
}