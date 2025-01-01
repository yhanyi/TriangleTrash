#include "../../include/network/server.hpp"
#include "../../include/orderbook/orderbook.hpp"
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
#include <vector>

namespace network {

class NetworkServer::Impl {
public:
  Impl(uint16_t port, orderbook::OrderBook &orderbook)
      : _orderbook(orderbook), _port(port), _running(false) {}

  ~Impl() { stop(); }

  void start();
  void stop();

private:
  void acceptLoop();
  void handleClient(int clientSocket);
  orderbook::Order parseOrderMessage(const std::string &message);
  void sendResponse(int clientSocket, const std::string &response);

  orderbook::OrderBook &_orderbook;
  int _serverSocket;
  uint16_t _port;
  std::atomic<bool> _running;
  std::thread _acceptThread;
  std::vector<std::thread> _clientThreads;
  std::mutex _threads_mutex;
};

/**
 * Protocol message format:
 * {
 *   "type": "new_order",
 *   "side": "buy"|"sell",
 *   "price": 100.0,
 *   "quantity": 10,
 *   "order_id": 12345
 * }
 */

NetworkServer::NetworkServer(uint16_t port, orderbook::OrderBook &orderbook)
    : _pimpl(new Impl(port, orderbook)) {}

NetworkServer::~NetworkServer() { delete _pimpl; }

void NetworkServer::start() { _pimpl->start(); }
void NetworkServer::stop() { _pimpl->stop(); }

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
    std::cerr << "Failed to bind socket: " << strerror(errno) << std::endl;
    close(_serverSocket);
    throw std::runtime_error("Failed to bind socket");
  }

  // Start listening
  if (listen(_serverSocket, SOMAXCONN) < 0) {
    std::cerr << "Failed to listen on socket: " << strerror(errno) << std::endl;
    close(_serverSocket);
    throw std::runtime_error("Failed to listen on socket");
  }

  _running = true;

  // Start accept thread
  _acceptThread = std::thread(&NetworkServer::Impl::acceptLoop, this);

  std::cout << "Server successfully started and listening on port " << _port
            << std::endl;
}

void NetworkServer::Impl::stop() {
  if (!_running)
    return;
  _running = false;

  // Close the server socket to interrupt accept()
  if (_serverSocket != -1) {
    close(_serverSocket);
    _serverSocket = -1;
  }

  // Wait for accept thread to finish
  if (_acceptThread.joinable()) {
    _acceptThread.join();
  }

  // Clean up client threads
  {
    std::lock_guard<std::mutex> lock(_threads_mutex);
    for (auto &thread : _clientThreads) {
      if (thread.joinable()) {
        thread.join();
      }
    }
    _clientThreads.clear();
  }

  std::cout << "Server stopped\n";
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
        // These errors are expected during shutdown or when non-blocking
        continue;
      }
      std::cerr << "Failed to accept connection: " << strerror(errno)
                << std::endl;
      continue;
    }

    std::cout << "New client connected\n";

    {
      std::lock_guard<std::mutex> lock(_threads_mutex);
      _clientThreads.emplace_back(&NetworkServer::Impl::handleClient, this,
                                  clientSocket);
    }
  }
}

void NetworkServer::Impl::handleClient(int clientSocket) {
  std::array<char, 4096> buffer;

  try {
    while (_running) {
      std::memset(buffer.data(), 0, buffer.size());
      ssize_t bytesRead = read(clientSocket, buffer.data(), buffer.size() - 1);

      if (bytesRead < 0) {
        if (errno == EINTR)
          continue;
        std::cerr << "Read error: " << strerror(errno) << std::endl;
        break;
      }

      if (bytesRead == 0) {
        std::cout << "Client disconnected\n";
        break;
      }

      std::string message(buffer.data(), bytesRead);
      try {
        // Parse and process the order
        orderbook::Order order = parseOrderMessage(message);
        bool success = _orderbook.addOrder(order);

        // Send response
        std::string response =
            success
                ? "{\"status\":\"success\",\"order_id\":" +
                      std::to_string(order.getId()) + "}"
                : "{\"status\":\"error\",\"message\":\"Failed to add order\"}";

        sendResponse(clientSocket, response);

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

orderbook::Order
NetworkServer::Impl::parseOrderMessage(const std::string &message) {
  try {
    nlohmann::json j = nlohmann::json::parse(message);

    if (j["type"] != "new_order") {
      throw std::runtime_error("Invalid message type");
    }

    orderbook::Side side =
        j["side"] == "buy" ? orderbook::Side::BUY : orderbook::Side::SELL;
    uint64_t orderId = j["order_id"];
    double price = j["price"];
    uint32_t quantity = j["quantity"];

    return orderbook::Order(orderId, side, price, quantity);

  } catch (const std::exception &e) {
    throw std::runtime_error("Failed to parse order: " + std::string(e.what()));
  }
}

void NetworkServer::Impl::sendResponse(int clientSocket,
                                       const std::string &response) {
  send(clientSocket, response.c_str(), response.length(), 0);
}

} // namespace network
