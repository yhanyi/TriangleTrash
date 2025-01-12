#pragma once

#include <cstdint>
#include <string>

namespace orderbook {
class OrderBook;
} // namespace orderbook

namespace session {
class Session;
} // namespace session

namespace network {

class NetworkServer {
public:
  NetworkServer(uint16_t port, bool use_binary_protocol = false);
  ~NetworkServer();

  void start();
  void stop();
  void createSession(const std::string &session_id);
  session::Session *getSession(const std::string &session_id);

  void enableMarketData(const std::string &multicast_addr, uint16_t port);
  void publishMarketData(const std::string &symbol, double best_bid,
                         double best_ask, uint32_t bid_size, uint32_t ask_size);

private:
  class Impl;
  Impl *_pimpl;
};

} // namespace network
