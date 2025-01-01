#pragma once

#include <cstdint>

namespace orderbook {

class OrderBook;

} // namespace orderbook

namespace network {

class NetworkServer {
public:
  NetworkServer(uint16_t port, orderbook::OrderBook &orderbook);
  ~NetworkServer();
  void start();
  void stop();

private:
  class Impl;
  Impl *_pimpl;
};

} // namespace network
