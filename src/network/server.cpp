#include "../../include/network/server.hpp"
#include "../../include/network/protocol.hpp"
#include "../../include/network/thread_pool.hpp"
#include "../../include/network/zero_copy.hpp"
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
  Impl(uint16_t port, bool use_binary_protocol)
      : _port(port), _running(false), _serverSocket(-1),
        _use_binary_protocol(use_binary_protocol) {
    _thread_pool.init(std::thread::hardware_concurrency());
    _zero_copy_handler.initBuffers(4096); // 4KB buffers
    createSession("default");
  }

  ~Impl() { stop(); }

  void start() {
    if (_running)
      return;
    _running = true;

    _serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (_serverSocket < 0) {
      throw std::runtime_error("Failed to create socket");
    }

    if (!SocketOptimiser::optimiseSocket(_serverSocket)) {
      throw std::runtime_error("Failed to optimise socket");
    }

    if (_market_data_enabled) {
      if (!_market_data_publisher->init()) {
        throw std::runtime_error("Failed to initialise market data publisher");
      }
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(_port);

    if (bind(_serverSocket, (struct sockaddr *)&serverAddress,
             sizeof(serverAddress)) < 0) {
      throw std::runtime_error("Failed to bind socket");
    }

    if (listen(_serverSocket, SOMAXCONN) < 0) {
      throw std::runtime_error("Failed to listen on socket");
    }

    _acceptThread = std::thread(&NetworkServer::Impl::acceptLoop, this);
    std::cout << "Server started on port " << _port << " with "
              << _thread_pool.getSize() << " worker threads\n";
  }

  void stop() {
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
  }

  void createSession(const std::string &session_id) {
    std::lock_guard<std::mutex> lock(_sessions_mutex);
    if (_sessions.find(session_id) == _sessions.end()) {
      _sessions[session_id] = std::make_unique<session::Session>(session_id);
      _sessions[session_id]->createOrderBook("STOCK");
    }
  }

  session::Session *getSession(const std::string &session_id) {
    std::lock_guard<std::mutex> lock(_sessions_mutex);
    auto it = _sessions.find(session_id);
    if (it != _sessions.end()) {
      return it->second.get();
    }
    return nullptr;
  }

  void enableMarketData(const std::string &multicast_addr, uint16_t port) {
    _market_data_publisher =
        std::make_unique<MarketDataPublisher>(multicast_addr, port);
    _market_data_enabled = true;
  }

  void publishMarketData(const std::string &symbol, double best_bid,
                         double best_ask, uint32_t bid_size,
                         uint32_t ask_size) {
    if (!_market_data_enabled || !_market_data_publisher)
      return;

    MarketDataMessage msg{};
    msg.header.type = MessageType::MARKET_DATA;
    msg.header.length = sizeof(MarketDataMessage) - sizeof(MessageHeader);
    msg.header.seq_num = BinaryProtocol::hton32(_market_data_seq++);

    strncpy(msg.symbol, symbol.c_str(), sizeof(msg.symbol) - 1);
    msg.best_bid = BinaryProtocol::htonDouble(best_bid);
    msg.best_ask = BinaryProtocol::htonDouble(best_ask);
    msg.bid_size = BinaryProtocol::hton32(bid_size);
    msg.ask_size = BinaryProtocol::hton32(ask_size);
    msg.timestamp = BinaryProtocol::hton64(
        std::chrono::system_clock::now().time_since_epoch().count());

    _market_data_publisher->publish(msg);
  }

private:
  void acceptLoop() {
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
        _thread_pool.async(
            [this, clientSocket]() { handleClient(clientSocket); });
      } catch (const std::exception &e) {
        std::cerr << "Failed to submit client task: " << e.what() << std::endl;
        close(clientSocket);
      }
    }
  }

  void handleClient(int clientSocket) {
    ZeroCopyHandler client_handler;
    client_handler.initBuffers(4096);

    try {
      while (_running) {
        if (_use_binary_protocol) {
          handleBinaryMessage(clientSocket, client_handler);
        } else {
          handleJsonMessage(clientSocket, client_handler);
        }
      }
    } catch (const std::exception &e) {
      std::cerr << "Client handler error: " << e.what() << std::endl;
    }

    close(clientSocket);
  }

  void handleJsonMessage(int clientSocket, ZeroCopyHandler &handler) {
    std::array<char, 4096> buffer;
    ssize_t bytesRead = read(clientSocket, buffer.data(), buffer.size() - 1);

    if (bytesRead <= 0)
      return;

    std::string message(buffer.data(), bytesRead);
    try {
      nlohmann::json j = nlohmann::json::parse(message);

      std::string type = j["type"];
      if (type == "join") {
        handleJsonJoin(clientSocket, j);
      } else if (type == "new_order") {
        handleJsonOrder(clientSocket, j);
      }
    } catch (const std::exception &e) {
      std::string errorResponse = "{\"status\":\"error\",\"message\":\"" +
                                  std::string(e.what()) + "\"}";
      send(clientSocket, errorResponse.c_str(), errorResponse.length(), 0);
    }
  }

  void handleBinaryMessage(int clientSocket, ZeroCopyHandler &handler) {
    MessageHeader header;
    ssize_t bytes_read =
        recv(clientSocket, &header, sizeof(header), MSG_WAITALL);

    if (bytes_read <= 0)
      return;

    header.length = BinaryProtocol::ntoh16(header.length);
    header.seq_num = BinaryProtocol::ntoh32(header.seq_num);

    std::vector<uint8_t> body(header.length);
    bytes_read = recv(clientSocket, body.data(), header.length, MSG_WAITALL);

    if (bytes_read <= 0)
      return;

    switch (header.type) {
    case MessageType::JOIN:
      handleBinaryJoin(clientSocket, body);
      break;
    case MessageType::NEW_ORDER:
      handleBinaryOrder(clientSocket, body);
      break;
    default:
      std::cerr << "Unknown message type: " << static_cast<int>(header.type)
                << std::endl;
      break;
    }
  }

  void handleJsonJoin(int clientSocket, const nlohmann::json &j) {
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

  void handleBinaryJoin(int clientSocket, const std::vector<uint8_t> &body) {
    if (body.size() < sizeof(JoinMessage) - sizeof(MessageHeader)) {
      return;
    }

    const auto *join_data = reinterpret_cast<const JoinMessage *>(body.data());
    std::string username(join_data->username);
    std::string session_id(join_data->session_id);

    auto *session = getSession(session_id);
    if (!session) {
      return;
    }

    if (session->addUser(username, clientSocket)) {
      std::vector<uint8_t> response =
          BinaryProtocol::serializeJoin(username, session_id);
      send(clientSocket, response.data(), response.size(), 0);
    }
  }

  void handleJsonOrder(int clientSocket, const nlohmann::json &j) {
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

    if (side == orderbook::Side::BUY &&
        !user->canAffordTrade(price, quantity)) {
      throw std::runtime_error("Insufficient funds");
    }

    if (side == orderbook::Side::SELL && user->getPosition(symbol) < quantity) {
      throw std::runtime_error("Insufficient position");
    }

    auto *order =
        orderbook::OrderAllocator::create(order_id, side, price, quantity);

    auto match_result = orderbook->matchOrder(*order);

    if (match_result) {
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

  void handleBinaryOrder(int clientSocket, const std::vector<uint8_t> &body) {
    if (body.size() < sizeof(NewOrderMessage) - sizeof(MessageHeader)) {
      return;
    }

    const auto *order_data =
        reinterpret_cast<const NewOrderMessage *>(body.data());

    uint64_t order_id = BinaryProtocol::ntoh64(order_data->order_id);
    double price = BinaryProtocol::ntohDouble(order_data->price);
    uint32_t quantity = BinaryProtocol::ntoh32(order_data->quantity);

    std::string session_id(order_data->session_id);
    std::string symbol(order_data->symbol);

    auto *session = getSession(session_id);
    if (!session) {
      sendBinaryError(clientSocket, "Session not found");
      return;
    }

    auto user = session->getUserBySocket(clientSocket);
    if (!user) {
      sendBinaryError(clientSocket, "User not found");
      return;
    }

    auto *orderbook = session->getOrderBook(symbol);
    if (!orderbook) {
      sendBinaryError(clientSocket, "Symbol not found");
      return;
    }

    orderbook::Side side =
        order_data->side == 0 ? orderbook::Side::BUY : orderbook::Side::SELL;

    if (side == orderbook::Side::BUY &&
        !user->canAffordTrade(price, quantity)) {
      sendBinaryError(clientSocket, "Insufficient funds");
      return;
    }

    if (side == orderbook::Side::SELL && user->getPosition(symbol) < quantity) {
      sendBinaryError(clientSocket, "Insufficient position");
      return;
    }

    auto *order =
        orderbook::OrderAllocator::create(order_id, side, price, quantity);

    auto match_result = orderbook->matchOrder(*order);

    if (match_result) {
      if (side == orderbook::Side::BUY) {
        user->updateBalance(-price * quantity);
        user->addPosition(symbol, quantity);
      } else {
        user->updateBalance(price * quantity);
        user->removePosition(symbol, quantity);
      }

      sendBinaryOrderResponse(clientSocket, order_id, true, "Order matched");
      orderbook::OrderAllocator::destroy(order);
    } else {
      if (orderbook->addOrder(*order)) {
        sendBinaryOrderResponse(clientSocket, order_id, true,
                                "Order added to book");
      } else {
        orderbook::OrderAllocator::destroy(order);
        sendBinaryError(clientSocket, "Failed to add order");
      }
    }
  }

  void sendResponse(int clientSocket, const std::string &response) {
    send(clientSocket, response.c_str(), response.length(), 0);
  }

  void sendBinaryResponse(int clientSocket,
                          const std::vector<uint8_t> &response) {
    send(clientSocket, response.data(), response.size(), 0);
  }

  void sendBinaryError(int clientSocket, const std::string &message) {

    struct ErrorResponse {
      MessageHeader header;
      char message[256];
    } response{};

    response.header.type = MessageType::ORDER_ACK;
    response.header.length = sizeof(ErrorResponse) - sizeof(MessageHeader);
    response.header.seq_num = BinaryProtocol::hton32(_market_data_seq++);
    strncpy(response.message, message.c_str(), sizeof(response.message) - 1);

    send(clientSocket, &response, sizeof(response), 0);
  }

  void sendBinaryOrderResponse(int clientSocket, uint64_t order_id,
                               bool success, const std::string &message) {

    struct OrderResponse {
      MessageHeader header;
      uint64_t order_id;
      uint8_t success;
      char message[256];
    } response{};

    response.header.type = MessageType::ORDER_ACK;
    response.header.length = sizeof(OrderResponse) - sizeof(MessageHeader);
    response.header.seq_num = BinaryProtocol::hton32(_market_data_seq++);
    response.order_id = BinaryProtocol::hton64(order_id);
    response.success = success ? 1 : 0;
    strncpy(response.message, message.c_str(), sizeof(response.message) - 1);

    send(clientSocket, &response, sizeof(response), 0);
  }

private:
  int _serverSocket;
  uint16_t _port;
  std::atomic<bool> _running;
  std::thread _acceptThread;
  ThreadPool _thread_pool;
  std::vector<std::thread> _clientThreads;
  std::mutex _threads_mutex;

  bool _use_binary_protocol;
  std::unique_ptr<MarketDataPublisher> _market_data_publisher;
  bool _market_data_enabled{false};
  uint32_t _market_data_seq{0};
  ZeroCopyHandler _zero_copy_handler;

  std::unordered_map<std::string, std::unique_ptr<session::Session>> _sessions;
  std::mutex _sessions_mutex;
};

NetworkServer::NetworkServer(uint16_t port, bool use_binary_protocol)
    : _pimpl(new Impl(port, use_binary_protocol)) {}

NetworkServer::~NetworkServer() { delete _pimpl; }

void NetworkServer::start() { _pimpl->start(); }

void NetworkServer::stop() { _pimpl->stop(); }

void NetworkServer::createSession(const std::string &session_id) {
  _pimpl->createSession(session_id);
}

session::Session *NetworkServer::getSession(const std::string &session_id) {
  return _pimpl->getSession(session_id);
}

void NetworkServer::enableMarketData(const std::string &multicast_addr,
                                     uint16_t port) {
  _pimpl->enableMarketData(multicast_addr, port);
}

void NetworkServer::publishMarketData(const std::string &symbol,
                                      double best_bid, double best_ask,
                                      uint32_t bid_size, uint32_t ask_size) {
  _pimpl->publishMarketData(symbol, best_bid, best_ask, bid_size, ask_size);
}

} // namespace network
