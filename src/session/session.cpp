#include "../../include/session/session.hpp"

namespace session {

Session::Session(const std::string &session_id)
    : _session_id(session_id), _active(true) {}

Session::~Session() = default;

bool Session::addUser(const std::string &username, int socket_fd) {
  std::lock_guard<std::mutex> lock(_mutex);

  // Check if username already exists
  if (_users.find(username) != _users.end()) {
    return false;
  }

  // Create new user and add to maps
  auto user = std::make_shared<User>(username, socket_fd);
  _users[username] = user;
  _socket_to_username[socket_fd] = username;

  return true;
}

bool Session::removeUser(const std::string &username) {
  std::lock_guard<std::mutex> lock(_mutex);

  auto it = _users.find(username);
  if (it == _users.end()) {
    return false;
  }

  // Remove from socket map
  _socket_to_username.erase(it->second->getSocketFd());

  // Remove from users map
  _users.erase(it);

  return true;
}

std::shared_ptr<User> Session::getUser(const std::string &username) {
  std::lock_guard<std::mutex> lock(_mutex);

  auto it = _users.find(username);
  if (it != _users.end()) {
    return it->second;
  }
  return nullptr;
}

std::shared_ptr<User> Session::getUserBySocket(int socket_fd) {
  std::lock_guard<std::mutex> lock(_mutex);

  auto it = _socket_to_username.find(socket_fd);
  if (it != _socket_to_username.end()) {
    return _users[it->second];
  }
  return nullptr;
}

const std::string &Session::getSessionId() const { return _session_id; }

size_t Session::getUserCount() const {
  std::lock_guard<std::mutex> lock(_mutex);
  return _users.size();
}

bool Session::isActive() const { return _active; }

void Session::createOrderBook(const std::string &symbol) {
  std::lock_guard<std::mutex> lock(_mutex);

  if (_orderbooks.find(symbol) == _orderbooks.end()) {
    _orderbooks[symbol] = std::make_unique<orderbook::OrderBook>();
  }
}

orderbook::OrderBook *Session::getOrderBook(const std::string &symbol) {
  std::lock_guard<std::mutex> lock(_mutex);

  auto it = _orderbooks.find(symbol);
  if (it != _orderbooks.end()) {
    return it->second.get();
  }
  return nullptr;
}

std::vector<std::string> Session::getAvailableSymbols() const {
  std::lock_guard<std::mutex> lock(_mutex);

  std::vector<std::string> symbols;
  symbols.reserve(_orderbooks.size());

  for (const auto &[symbol, _] : _orderbooks) {
    symbols.push_back(symbol);
  }

  return symbols;
}

} // namespace session
