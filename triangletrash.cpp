#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

class Player;
class Order;
class OrderBook;
class Game;

enum class OrderType { BID,
                       ASK };

class Order {
   public:
    Order(int quantity, double price, OrderType type, Player* player)
        : quantity(quantity), price(price), type(type), player(player) {}

    int quantity;
    double price;
    OrderType type;
    Player* player;
};

class Player {
   public:
    Player(const std::string& name, double initialBalance)
        : name(name), balance(initialBalance), stocksOwned(0) {}

    std::string name;
    double balance;
    int stocksOwned;
};

class OrderBook {
   public:
    void addOrder(const Order& order) {
        if (order.type == OrderType::BID) {
            bids[order.price].push_back(order);
        } else {
            asks[order.price].push_back(order);
        }
    }

    void matchOrders() {
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

    std::string getOrderBookDisplay() const {
        std::ostringstream oss;
        oss << "===== Order Book =====\n";
        oss << "      Price | Quantity\n";
        oss << "  ASK:\n";
        for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
            int totalQuantity = 0;
            for (const auto& order : it->second) {
                totalQuantity += order.quantity;
            }
            oss << std::setw(10) << std::fixed << std::setprecision(2) << it->first
                << " | " << std::setw(8) << totalQuantity << "\n";
        }
        oss << "  ----------------------\n";
        oss << "  BID:\n";
        for (auto it = bids.rbegin(); it != bids.rend(); ++it) {
            int totalQuantity = 0;
            for (const auto& order : it->second) {
                totalQuantity += order.quantity;
            }
            oss << std::setw(10) << std::fixed << std::setprecision(2) << it->first
                << " | " << std::setw(8) << totalQuantity << "\n";
        }
        oss << "=====================\n";
        return oss.str();
    }

   private:
    std::map<double, std::vector<Order>, std::greater<double>> bids;
    std::map<double, std::vector<Order>, std::less<double>> asks;
};

class Game {
   public:
    Game(const std::string& roomCode) : roomCode(roomCode) {}

    void addPlayer(Player* player) {
        players.push_back(player);
    }

    void processOrder(const std::string& playerName, const std::string& orderStr) {
        std::istringstream iss(orderStr);
        std::string action;
        int quantity;
        char at;
        double price;

        iss >> action >> quantity >> at >> price;

        Player* player = findPlayer(playerName);
        if (!player) {
            std::cout << "Player not found.\n";
            return;
        }

        OrderType orderType = (action == "bid") ? OrderType::BID : OrderType::ASK;
        Order order(quantity, price, orderType, player);

        orderBook.addOrder(order);
        orderBook.matchOrders();

        // Notify all clients about the update
        notifyClients();
    }

    std::string getGameState() const {
        std::ostringstream oss;
        oss << "Room Code: " << roomCode << "\n";
        oss << "Players:\n";
        for (const auto& player : players) {
            oss << player->name << " - Balance: $" << std::fixed << std::setprecision(2)
                << player->balance << ", Stocks: " << player->stocksOwned << "\n";
        }
        oss << "\n"
            << orderBook.getOrderBookDisplay();
        return oss.str();
    }

    void notifyClients() {
        // TODO: Send updates to all game clients.
        std::cout << "\n"
                  << getGameState() << std::endl;
    }

   private:
    std::string roomCode;
    std::vector<Player*> players;
    OrderBook orderBook;

    Player* findPlayer(const std::string& name) {
        auto it = std::find_if(players.begin(), players.end(),
                               [&name](const Player* p) { return p->name == name; });
        return (it != players.end()) ? *it : nullptr;
    }
};

void clearScreen() {
#ifdef _WIN32
    std::system("cls");
#else
    std::system("clear");
#endif
}

int main() {
    Game game("ABCD1234");

    Player player1("Alice", 10000);
    Player player2("Bob", 10000);

    game.addPlayer(&player1);
    game.addPlayer(&player2);

    std::string input;
    while (true) {
        clearScreen();
        std::cout << game.getGameState();
        std::cout << "\nEnter order (e.g., 'Alice bid 5 @500' or 'Bob ask 3 @600') or 'quit' to exit: ";
        std::getline(std::cin, input);

        if (input == "quit") {
            break;
        }

        std::istringstream iss(input);
        std::string playerName, order;
        iss >> playerName;
        std::getline(iss, order);

        game.processOrder(playerName, order);
    }

    return 0;
}