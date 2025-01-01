#include "../include/network/server.hpp"
#include "../include/orderbook/orderbook.hpp"
#include <chrono>
#include <iostream>
#include <thread>

using namespace orderbook;
using namespace network;

void printOrderBookStatus(const OrderBook &book) {
  std::cout << "Best bid: " << book.getBestBid() << "\n";
  std::cout << "Best ask: " << book.getBestAsk() << "\n";
  std::cout << "-------------------\n";
}

int main() {
  std::cout << "Starting OrderBook Server...\n";
  OrderBook book;
  Order buy1(1, Side::BUY, 100.0, 10);  // Buy 10 units at $100
  Order buy2(2, Side::BUY, 101.0, 5);   // Buy 5 units at $101
  Order sell1(3, Side::SELL, 102.0, 7); // Sell 7 units at $102

  std::cout << "Adding buy order 1...\n";
  book.addOrder(buy1);
  printOrderBookStatus(book);

  std::cout << "Adding buy order 2...\n";
  book.addOrder(buy2);
  printOrderBookStatus(book);

  std::cout << "Adding sell order 1...\n";
  book.addOrder(sell1);
  printOrderBookStatus(book);

  std::cout << "Attempting to match new sell order...\n";
  Order sell2(4, Side::SELL, 101.0, 3); // Sell 3 units at $101
  auto match_result = book.matchOrder(sell2);

  if (match_result) {
    std::cout << "Match found!\n";
  } else {
    std::cout << "No match found.\n";
  }

  printOrderBookStatus(book);

  try {
    NetworkServer server(8080, book);
    std::cout << "Network server initialized on port 8080\n";
    // Start the server
    server.start();
    // Keep main thread alive
    std::cout << "Server running. Press Ctrl+C to exit.\n";
    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
