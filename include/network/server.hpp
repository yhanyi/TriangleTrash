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
  NetworkServer(uint16_t port);
  ~NetworkServer();
  void start();
  void stop();
  void createSession(const std::string &session_id);
  session::Session *getSession(const std::string &session_id);

private:
  class Impl;
  Impl *_pimpl;
};

} // namespace network
