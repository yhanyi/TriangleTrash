#include "../../include/session/user.hpp"

namespace session {

User::User(std::string username, int socket_fd)
    : _username(std::move(username)), _socket_fd(socket_fd),
      _balance(10000.0), // Starting balance, can be made configurable
      _active(true) {}

User::~User() = default;

const std::string &User::getUsername() const { return _username; }

int User::getSocketFd() const { return _socket_fd; }

double User::getBalance() const { return _balance; }

void User::updateBalance(double amount) { _balance += amount; }

bool User::canAffordTrade(double price, uint32_t quantity) const {
  return (_balance >= price * quantity);
}

void User::addPosition(const std::string &symbol, uint32_t quantity) {
  _positions[symbol] += quantity;
}

void User::removePosition(const std::string &symbol, uint32_t quantity) {
  if (_positions.find(symbol) != _positions.end()) {
    if (_positions[symbol] >= quantity) {
      _positions[symbol] -= quantity;
      if (_positions[symbol] == 0) {
        _positions.erase(symbol);
      }
    }
  }
}

uint32_t User::getPosition(const std::string &symbol) const {
  if (auto it = _positions.find(symbol); it != _positions.end()) {
    return it->second;
  }
  return 0;
}

bool User::isActive() const { return _active; }

void User::setActive(bool active) { _active = active; }

} // namespace session
