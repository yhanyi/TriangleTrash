#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace session {

class User {
public:
  User(std::string username, int socket_fd);
  ~User();

  // Getters
  const std::string &getUsername() const;
  int getSocketFd() const;
  double getBalance() const;

  // Trade related
  void updateBalance(double amount);
  bool canAffordTrade(double price, uint32_t quantity) const;
  void addPosition(const std::string &symbol, uint32_t quantity);
  void removePosition(const std::string &symbol, uint32_t quantity);
  uint32_t getPosition(const std::string &symbol) const;

  // Session management
  bool isActive() const;
  void setActive(bool active);

private:
  std::string _username;
  int _socket_fd;
  double _balance;
  bool _active;
  std::unordered_map<std::string, uint32_t> _positions; // symbol -> quantity
};

} // namespace session
