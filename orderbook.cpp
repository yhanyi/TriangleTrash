#include "orderbook.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

void OrderBook::addOrder(const Order& order) {
    if (order.type == OrderType::BID) {
        bids[order.price].push_back(order);
    } else {
        asks[order.price].push_back(order);
    }
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