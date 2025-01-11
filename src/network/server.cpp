#include "../../include/network/server.hpp"
#include "../../include/network/thread_pool.hpp"
#include "../../include/orderbook/order.hpp"
#include "../../include/orderbook/order_allocator.hpp"
#include "../../include/orderbook/orderbook.hpp"
#include "../../include/session/session.hpp"
#include <atomic>
#include <cstring>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <nlohmann/json.hpp>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace network {

class NetworkServer::Impl {
public:
  Impl(uint16_t port) : _port(port), _running(false), _serverSocket(-1) {
    _thread_pool.init(std::thread::hardware_concurrency());
    // Create default session
    createSession("default");
  }

  ~Impl() { stop(); }

  void start();
  void stop();
  void createSession(const std::string &session_id);
  session::Session *getSession(const std::string &session_id);

private:
  void acceptLoop();
  void handleClient(int clientSocket);
  void handleJoinMessage(int clientSocket, const nlohmann::json &j);
  void handleOrderMessage(int clientSocket, const nlohmann::json &j);
  void sendResponse(int clientSocket, const std::string &response);

  int _serverSocket;
  uint16_t _port;
  std::atomic<bool> _running;
  std::thread _acceptThread;
  ThreadPool _thread_pool;
  std::vector<std::thread> _clientThreads;
  std::mutex _threads_mutex;

  // Session management
  std::unordered_map<std::string, std::unique_ptr<session::Session>> _sessions;
  std::mutex _sessions_mutex;
};

NetworkServer::NetworkServer(uint16_t port) : _pimpl(new Impl(port)) {}

NetworkServer::~NetworkServer() { delete _pimpl; }

void NetworkServer::start() { _pimpl->start(); }
void NetworkServer::stop() { _pimpl->stop(); }
void NetworkServer::createSession(const std::string &session_id) {
  _pimpl->createSession(session_id);
}
session::Session *NetworkServer::getSession(const std::string &session_id) {
  return _pimpl->getSession(session_id);
}

void NetworkServer::Impl::createSession(const std::string &session_id) {
  std::lock_guard<std::mutex> lock(_sessions_mutex);
  if (_sessions.find(session_id) == _sessions.end()) {
    _sessions[session_id] = std::make_unique<session::Session>(session_id);
    // Create a default trading symbol
    _sessions[session_id]->createOrderBook("STOCK");
  }
}

session::Session *
NetworkServer::Impl::getSession(const std::string &session_id) {
  std::lock_guard<std::mutex> lock(_sessions_mutex);
  auto it = _sessions.find(session_id);
  if (it != _sessions.end()) {
    return it->second.get();
  }
  return nullptr;
}

void NetworkServer::Impl::start() {
  if (_running) {
    std::cout << "Server is already running\n";
    return;
  }

  _running = true;

  // Create TCP socket
  _serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (_serverSocket < 0) {
    throw std::runtime_error("Failed to create socket");
  }

  // Socket options
  int opt = 1;
  if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) <
      0) {
    throw std::runtime_error("Failed to set socket options");
  }

  // Bind socket
  sockaddr_in serverAddress{};
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_addr.s_addr = INADDR_ANY;
  serverAddress.sin_port = htons(_port);

  if (bind(_serverSocket, (struct sockaddr *)&serverAddress,
           sizeof(serverAddress)) < 0) {
    throw std::runtime_error("Failed to bind socket");
  }

  // Start listening
  if (listen(_serverSocket, SOMAXCONN) < 0) {
    throw std::runtime_error("Failed to listen on socket");
  }

  // Start accept thread
  _acceptThread = std::thread(&NetworkServer::Impl::acceptLoop, this);

  std::cout << "Server started on port " << _port << " with "
            << _thread_pool.getSize() << " worker threads\n";
}

void NetworkServer::Impl::stop() {
  if (!_running)
    return;
  _running = false;

  if (_serverSocket != -1) {
    close(_serverSocket);
    _serverSocket = -1;
  }

  if (_acceptThread.joinable()) {
    _acceptThread.join();
  }

  _thread_pool.terminate();

  // {
  //   std::lock_guard<std::mutex> lock(_threads_mutex);
  //   for (auto &thread : _clientThreads) {
  //     if (thread.joinable()) {
  //       thread.join();
  //     }
  //   }
  //   _clientThreads.clear();
  // }
}

void NetworkServer::Impl::acceptLoop() {
  while (_running) {
    sockaddr_in clientAddress{};
    socklen_t clientLen = sizeof(clientAddress);
    int clientSocket =
        accept(_serverSocket, (struct sockaddr *)&clientAddress, &clientLen);

    if (!_running)
      break;

    if (clientSocket < 0) {
      if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN) {
        continue;
      }
      std::cerr << "Failed to accept connection: " << strerror(errno)
                << std::endl;
      continue;
    }

    try {
      // Submit client handling to thread pool
      _thread_pool.async(
          [this, clientSocket]() { handleClient(clientSocket); });
    } catch (const std::exception &e) {
      std::cerr << "Failed to submit client task: " << e.what() << std::endl;
      close(clientSocket);
    }

    // std::cout << "New client connected\n";

    // {
    //   std::lock_guard<std::mutex> lock(_threads_mutex);
    //   _clientThreads.emplace_back(&NetworkServer::Impl::handleClient, this,
    //                               clientSocket);
    // }
  }
}

void NetworkServer::Impl::handleClient(int clientSocket) {
  std::array<char, 4096> buffer;

  try {
    while (_running) {
      std::memset(buffer.data(), 0, buffer.size());
      ssize_t bytesRead = read(clientSocket, buffer.data(), buffer.size() - 1);

      if (bytesRead <= 0) {
        if (bytesRead == 0) {
          std::cout << "Client disconnected\n";
        }
        break;
      }

      std::string message(buffer.data(), bytesRead);
      try {
        nlohmann::json j = nlohmann::json::parse(message);

        // Handle different message types
        std::string type = j["type"];
        if (type == "join") {
          handleJoinMessage(clientSocket, j);
        } else if (type == "new_order") {
          handleOrderMessage(clientSocket, j);
        } else {
          throw std::runtime_error("Unknown message type");
        }

      } catch (const std::exception &e) {
        std::string errorResponse = "{\"status\":\"error\",\"message\":\"" +
                                    std::string(e.what()) + "\"}";
        sendResponse(clientSocket, errorResponse);
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "Client handler error: " << e.what() << std::endl;
  }

  close(clientSocket);
}

void NetworkServer::Impl::handleJoinMessage(int clientSocket,
                                            const nlohmann::json &j) {
  std::string username = j["username"];
  std::string session_id = j.value("session_id", "default");

  auto *session = getSession(session_id);
  if (!session) {
    throw std::runtime_error("Session not found");
  }

  if (session->addUser(username, clientSocket)) {
    nlohmann::json response = {{"status", "success"},
                               {"message", "Joined session"},
                               {"session_id", session_id},
                               {"username", username}};
    sendResponse(clientSocket, response.dump());
  } else {
    throw std::runtime_error("Username already taken");
  }
}

void NetworkServer::Impl::handleOrderMessage(int clientSocket,
                                             const nlohmann::json &j) {
  std::string session_id = j.value("session_id", "default");
  auto *session = getSession(session_id);
  if (!session) {
    throw std::runtime_error("Session not found");
  }

  auto user = session->getUserBySocket(clientSocket);
  if (!user) {
    throw std::runtime_error("User not found");
  }

  std::string symbol = j.value("symbol", "STOCK");
  auto *orderbook = session->getOrderBook(symbol);
  if (!orderbook) {
    throw std::runtime_error("Symbol not found");
  }

  orderbook::Side side =
      j["side"] == "buy" ? orderbook::Side::BUY : orderbook::Side::SELL;
  uint64_t order_id = j["order_id"];
  double price = j["price"];
  uint32_t quantity = j["quantity"];

  // Check if user can afford the trade
  if (side == orderbook::Side::BUY && !user->canAffordTrade(price, quantity)) {
    throw std::runtime_error("Insufficient funds");
  }

  // For sell orders, check if user has enough position
  if (side == orderbook::Side::SELL && user->getPosition(symbol) < quantity) {
    throw std::runtime_error("Insufficient position");
  }

  // orderbook::Order order(order_id, side, price, quantity);
  orderbook::Order *order =
      orderbook::OrderAllocator::create(order_id, side, price, quantity);

  // Try to match the order
  auto match_result = orderbook->matchOrder(*order);

  if (match_result) {
    // Update user balances and positions
    if (side == orderbook::Side::BUY) {
      user->updateBalance(-price * quantity);
      user->addPosition(symbol, quantity);
    } else {
      user->updateBalance(price * quantity);
      user->removePosition(symbol, quantity);
    }

    nlohmann::json response = {{"status", "success"},
                               {"message", "Order matched"},
                               {"order_id", order_id}};
    sendResponse(clientSocket, response.dump());
    orderbook::OrderAllocator::destroy(order);
  } else {
    // Add to orderbook if no match
    if (orderbook->addOrder(*order)) {
      nlohmann::json response = {{"status", "success"},
                                 {"message", "Order added to book"},
                                 {"order_id", order_id}};
      sendResponse(clientSocket, response.dump());
    } else {
      orderbook::OrderAllocator::destroy(order);
      throw std::runtime_error("Failed to add order");
    }
  }
}

void NetworkServer::Impl::sendResponse(int clientSocket,
                                       const std::string &response) {
  send(clientSocket, response.c_str(), response.length(), 0);
}

} // namespace network
