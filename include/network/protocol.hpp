#pragma once

#include <cstdint>
#include <netinet/in.h>
#include <string>
#include <vector>

namespace network {

enum class MessageType : uint8_t {
  JOIN = 1,
  NEW_ORDER = 2,
  ORDER_ACK = 3,
  TRADE = 4,
  MARKET_DATA = 5
};

#pragma pack(push, 1)
struct MessageHeader {
  MessageType type;
  uint16_t length;
  uint32_t seq_num;
};

struct JoinMessage {
  MessageHeader header;
  char username[32];
  char session_id[32];
};

struct NewOrderMessage {
  MessageHeader header;
  uint64_t order_id;
  uint8_t side; // 0 = buy, 1 = sell
  double price;
  uint32_t quantity;
  char symbol[8];
  char session_id[32];
};

struct MarketDataMessage {
  MessageHeader header;
  char symbol[8];
  double best_bid;
  double best_ask;
  uint32_t bid_size;
  uint32_t ask_size;
  uint64_t timestamp;
};
#pragma pack(pop)

// Protocol serialisation/deserialisation helper
class BinaryProtocol {
public:
  static std::vector<uint8_t> serializeJoin(const std::string &username,
                                            const std::string &session_id);
  static std::vector<uint8_t> serializeNewOrder(uint64_t order_id, bool is_buy,
                                                double price, uint32_t quantity,
                                                const std::string &symbol,
                                                const std::string &session_id);
  static std::vector<uint8_t>
  serializeMarketData(const std::string &symbol, double best_bid,
                      double best_ask, uint32_t bid_size, uint32_t ask_size);

  static uint16_t hton16(uint16_t host) { return htons(host); }
  static uint32_t hton32(uint32_t host) { return htonl(host); }
  static uint64_t hton64(uint64_t host);
  static double htonDouble(double host);

  static uint16_t ntoh16(uint16_t net) { return ntohs(net); }
  static uint32_t ntoh32(uint32_t net) { return ntohl(net); }
  static uint64_t ntoh64(uint64_t net);
  static double ntohDouble(double net);
};

class MarketDataPublisher {
public:
  MarketDataPublisher(const std::string &multicast_addr, uint16_t port);
  ~MarketDataPublisher();

  bool init();
  void publish(const MarketDataMessage &msg);

private:
  std::string _multicast_addr;
  uint16_t _port;
  int _socket;
  struct sockaddr_in _addr;
};

} // namespace network
