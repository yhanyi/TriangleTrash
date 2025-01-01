#pragma once

#include "../orderbook/orderbook.hpp"
#include "user.hpp"
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace session {

class Session {
public:
  Session(const std::string &session_id);
  ~Session();

  // User management
  bool addUser(const std::string &username, int socket_fd);
  bool removeUser(const std::string &username);
  std::shared_ptr<User> getUser(const std::string &username);
  std::shared_ptr<User> getUserBySocket(int socket_fd);

  // Session info
  const std::string &getSessionId() const;
  size_t getUserCount() const;
  bool isActive() const;

  // Orderbook management
  void createOrderBook(const std::string &symbol);
  orderbook::OrderBook *getOrderBook(const std::string &symbol);
  std::vector<std::string> getAvailableSymbols() const;

private:
  std::string _session_id;
  std::unordered_map<std::string, std::shared_ptr<User>>
      _users; // username -> User
  std::unordered_map<int, std::string>
      _socket_to_username; // socket_fd -> username
  std::unordered_map<std::string, std::unique_ptr<orderbook::OrderBook>>
      _orderbooks; // symbol -> OrderBook
  mutable std::mutex _mutex;
  bool _active;
};

} // namespace session
