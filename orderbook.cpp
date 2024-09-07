#include "orderbook.hpp"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>

double OrderBook::getLowestAskPrice() const {
    return asks.empty() ? std::numeric_limits<double>::max() : asks.begin()->first;
}

double OrderBook::getHighestBidPrice() const {
    return bids.empty() ? 0 : bids.begin()->first;
}

void OrderBook::writeOption(Option option) {
    options.push_back(option);
}

void OrderBook::exerciseOption(Option& option, Player* player) {
    double currentPrice = getCurrentPrice();
    if (option.canExercise(currentPrice)) {
        if (option.type == OptionType::CALL) {
            option.writer->balance += option.strikePrice * option.quantity;
            option.writer->stocksOwned -= option.quantity;
            player->balance -= option.strikePrice * option.quantity;
            player->stocksOwned += option.quantity;
        } else {  // PUT
            option.writer->balance -= option.strikePrice * option.quantity;
            option.writer->stocksOwned += option.quantity;
            player->balance += option.strikePrice * option.quantity;
            player->stocksOwned -= option.quantity;
        }
        option.isExercised = true;
    }
}

std::vector<Option> OrderBook::getAvailableOptions() const {
    std::vector<Option> availableOptions;
    for (const auto& option : options) {
        if (!option.isExercised && option.holder == nullptr) {
            availableOptions.push_back(option);
        } 
    }
    return availableOptions;
}

double OrderBook::getCurrentPrice() const {
    if (asks.empty() || bids.empty()) {
        return 0.0;
    }
    return (asks.begin()->first + bids.rbegin()->first) / 2.0;
}

void OrderBook::addOrder(Order order) {
    if (order.type == OrderType::MARKET_BUY || order.type == OrderType::MARKET_SELL) {
        executeMarketOrder(order);
    } else if (order.type == OrderType::BID) {
        bids[order.price].push_back(std::move(order));
    } else if (order.type == OrderType::ASK) {
        asks[order.price].push_back(std::move(order));
    }
    matchOrders();
}

void OrderBook::executeMarketOrder(Order& order) {
    if (order.type == OrderType::MARKET_BUY) {
        for (auto it = asks.begin(); it != asks.end() && order.quantity > 0;) {
            auto& askOrders = it->second;
            for (auto orderIt = askOrders.begin(); orderIt != askOrders.end() && order.quantity > 0;) {
                int matchedQuantity = std::min(order.quantity, orderIt->quantity);
                executeTransaction(order.player, orderIt->player, matchedQuantity, it->first);
                order.quantity -= matchedQuantity;
                orderIt->quantity -= matchedQuantity;
                if (orderIt->quantity == 0) {
                    orderIt = askOrders.erase(orderIt);
                } else {
                    ++orderIt;
                }
            }
            if (askOrders.empty()) {
                it = asks.erase(it);
            } else {
                ++it;
            }
        }
    } else if (order.type == OrderType::MARKET_SELL) {
        for (auto it = bids.rbegin(); it != bids.rend() && order.quantity > 0;) {
            auto& bidOrders = it->second;
            for (auto orderIt = bidOrders.begin(); orderIt != bidOrders.end() && order.quantity > 0;) {
                int matchedQuantity = std::min(order.quantity, orderIt->quantity);
                executeTransaction(orderIt->player, order.player, matchedQuantity, it->first);
                order.quantity -= matchedQuantity;
                orderIt->quantity -= matchedQuantity;
                if (orderIt->quantity == 0) {
                    orderIt = bidOrders.erase(orderIt);
                } else {
                    ++orderIt;
                }
            }
            if (bidOrders.empty()) {
                it = std::map<double, std::vector<Order>, std::greater<double>>::reverse_iterator(bids.erase((++it).base()));
            } else {
                ++it;
            }
        }
    }
}

void OrderBook::executeTransaction(Player* buyer, Player* seller, int quantity, double price) {
    double totalPrice = quantity * price;
    buyer->balance -= totalPrice;
    buyer->stocksOwned += quantity;
    seller->balance += totalPrice;
    seller->stocksOwned -= quantity;
}

void OrderBook::matchOrders() {
    while (!bids.empty() && !asks.empty()) {
        auto highestBid = bids.rbegin();
        auto lowestAsk = asks.begin();

        if (highestBid->first >= lowestAsk->first) {
            // Match found
            auto& bidOrder = highestBid->second.front();
            auto& askOrder = lowestAsk->second.front();

            int matchedQuantity = std::min(bidOrder.quantity, askOrder.quantity);
            double matchPrice = lowestAsk->first;

            // Update player balances and stocks
            bidOrder.player->balance -= matchedQuantity * matchPrice;
            bidOrder.player->stocksOwned += matchedQuantity;
            askOrder.player->balance += matchedQuantity * matchPrice;
            askOrder.player->stocksOwned -= matchedQuantity;

            // Update order quantities
            bidOrder.quantity -= matchedQuantity;
            askOrder.quantity -= matchedQuantity;

            // Remove fulfilled orders
            if (bidOrder.quantity == 0) {
                highestBid->second.erase(highestBid->second.begin());
                if (highestBid->second.empty()) {
                    bids.erase(std::next(highestBid).base());
                }
            }
            if (askOrder.quantity == 0) {
                lowestAsk->second.erase(lowestAsk->second.begin());
                if (lowestAsk->second.empty()) {
                    asks.erase(lowestAsk);
                }
            }
        } else {
            break;
        }
    }
}

std::string OrderBook::getOrderBookDisplay() const {
    std::ostringstream oss;
    oss << "========== Orderbook =========\n";

    // Display ASK orders
    for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
        int size_sum = 0;
        for (const auto& order : it->second) {
            size_sum += order.quantity;
        }

        oss << "\t\033[1;31m$" << std::setw(6) << std::fixed << std::setprecision(2)
            << it->first << std::setw(5) << size_sum << "\033[0m ";

        // int barLength = std::min(size_sum / 10, 50);  // Cap bar length at 50 characters
        // oss << std::string(barLength, '█');
        oss << "\n";
    }

    // Print bid-ask spread
    double best_ask = asks.empty() ? 0.0 : asks.begin()->first;
    double best_bid = bids.empty() ? 0.0 : bids.rbegin()->first;
    if (best_ask > 0 && best_bid > 0) {
        oss << "\n\033[1;33m====== " << std::fixed << std::setprecision(2)
            << 10000 * (best_ask - best_bid) / best_bid << "bps ======\033[0m\n\n";
    }

    // Display BID orders
    for (auto it = bids.rbegin(); it != bids.rend(); ++it) {
        int size_sum = 0;
        for (const auto& order : it->second) {
            size_sum += order.quantity;
        }

        oss << "\t\033[1;32m$" << std::setw(6) << std::fixed << std::setprecision(2)
            << it->first << std::setw(5) << size_sum << "\033[0m ";

        // int barLength = std::min(size_sum / 10, 50);  // Cap bar length at 50 characters
        // oss << std::string(barLength, '█');
        oss << "\n";
    }

    oss << "==============================\n";
    return oss.str();
}