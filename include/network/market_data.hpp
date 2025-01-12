#pragma once

#include "protocol.hpp"
#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <unordered_set>

namespace network {

class MarketDataReceiver {
public:
  using DataCallback = std::function<void(const MarketDataMessage &)>;

  MarketDataReceiver(const std::string &multicast_addr, uint16_t port);
  ~MarketDataReceiver();

  bool start();
  void stop();
  void setCallback(DataCallback callback);

private:
  void receiveLoop();

  std::string _multicast_addr;
  uint16_t _port;
  int _socket;
  std::atomic<bool> _running{false};
  std::thread _receive_thread;
  DataCallback _callback;
};

class MarketDataClient {
public:
  MarketDataClient();
  ~MarketDataClient();

  bool subscribe(const std::string &symbol);
  bool unsubscribe(const std::string &symbol);

  void onMarketData(const MarketDataMessage &msg);

private:
  std::unique_ptr<MarketDataReceiver> _receiver;
  std::unordered_set<std::string> _subscribed_symbols;
  std::mutex _mutex;
};

} // namespace network
