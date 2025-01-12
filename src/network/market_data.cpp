#include "../../include/network/market_data.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace network {

MarketDataReceiver::MarketDataReceiver(const std::string &multicast_addr,
                                       uint16_t port)
    : _multicast_addr(multicast_addr), _port(port), _socket(-1) {}

MarketDataReceiver::~MarketDataReceiver() { stop(); }

bool MarketDataReceiver::start() {
  _socket = socket(AF_INET, SOCK_DGRAM, 0);
  if (_socket < 0)
    return false;

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(_port);

  if (bind(_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(_socket);
    return false;
  }

  struct ip_mreq mreq;
  mreq.imr_multiaddr.s_addr = inet_addr(_multicast_addr.c_str());
  mreq.imr_interface.s_addr = htonl(INADDR_ANY);

  if (setsockopt(_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) <
      0) {
    close(_socket);
    return false;
  }

  _running = true;
  _receive_thread = std::thread(&MarketDataReceiver::receiveLoop, this);

  return true;
}

void MarketDataReceiver::stop() {
  _running = false;
  if (_receive_thread.joinable()) {
    _receive_thread.join();
  }
  if (_socket >= 0) {
    close(_socket);
    _socket = -1;
  }
}

void MarketDataReceiver::setCallback(DataCallback callback) {
  _callback = std::move(callback);
}

void MarketDataReceiver::receiveLoop() {
  MarketDataMessage msg;

  while (_running) {
    ssize_t received = recv(_socket, &msg, sizeof(msg), 0);
    if (received == sizeof(msg)) {
      msg.header.length = BinaryProtocol::ntoh16(msg.header.length);
      msg.header.seq_num = BinaryProtocol::ntoh32(msg.header.seq_num);
      msg.best_bid = BinaryProtocol::ntohDouble(msg.best_bid);
      msg.best_ask = BinaryProtocol::ntohDouble(msg.best_ask);
      msg.bid_size = BinaryProtocol::ntoh32(msg.bid_size);
      msg.ask_size = BinaryProtocol::ntoh32(msg.ask_size);
      msg.timestamp = BinaryProtocol::ntoh64(msg.timestamp);

      if (_callback) {
        _callback(msg);
      }
    }
  }
}

MarketDataClient::MarketDataClient() {}

MarketDataClient::~MarketDataClient() {}

bool MarketDataClient::subscribe(const std::string &symbol) {
  std::lock_guard<std::mutex> lock(_mutex);
  return _subscribed_symbols.insert(symbol).second;
}

bool MarketDataClient::unsubscribe(const std::string &symbol) {
  std::lock_guard<std::mutex> lock(_mutex);
  return _subscribed_symbols.erase(symbol) > 0;
}

void MarketDataClient::onMarketData(const MarketDataMessage &msg) {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_subscribed_symbols.find(msg.symbol) != _subscribed_symbols.end()) {
    // TODO: Process market data update / add callbacks for client notif
  }
}

} // namespace network
