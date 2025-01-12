#include "../../include/network/protocol.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>

namespace network {

uint64_t BinaryProtocol::hton64(uint64_t host) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
  return (((uint64_t)htonl(host & 0xFFFFFFFF) << 32) | htonl(host >> 32));
#else
  return host;
#endif
}

double BinaryProtocol::htonDouble(double host) {
  union {
    double d;
    uint64_t i;
  } u;
  u.d = host;
  u.i = hton64(u.i);
  return u.d;
}

uint64_t BinaryProtocol::ntoh64(uint64_t net) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
  return (((uint64_t)ntohl(net & 0xFFFFFFFF) << 32) | ntohl(net >> 32));
#else
  return net;
#endif
}

double BinaryProtocol::ntohDouble(double net) {
  union {
    double d;
    uint64_t i;
  } u;
  u.d = net;
  u.i = ntoh64(u.i);
  return u.d;
}

std::vector<uint8_t>
BinaryProtocol::serializeJoin(const std::string &username,
                              const std::string &session_id) {

  JoinMessage msg{};
  msg.header.type = MessageType::JOIN;
  msg.header.length = sizeof(JoinMessage) - sizeof(MessageHeader);
  // Use sequence generator in practice
  msg.header.seq_num = hton32(1);

  strncpy(msg.username, username.c_str(), sizeof(msg.username) - 1);
  strncpy(msg.session_id, session_id.c_str(), sizeof(msg.session_id) - 1);

  std::vector<uint8_t> buffer(sizeof(msg));
  memcpy(buffer.data(), &msg, sizeof(msg));
  return buffer;
}

MarketDataPublisher::MarketDataPublisher(const std::string &multicast_addr,
                                         uint16_t port)
    : _multicast_addr(multicast_addr), _port(port), _socket(-1) {}

MarketDataPublisher::~MarketDataPublisher() {
  if (_socket >= 0) {
    close(_socket);
  }
}

bool MarketDataPublisher::init() {
  _socket = socket(AF_INET, SOCK_DGRAM, 0);
  if (_socket < 0) {
    return false;
  }

  int ttl = 32;
  if (setsockopt(_socket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) <
      0) {
    close(_socket);
    return false;
  }

  memset(&_addr, 0, sizeof(_addr));
  _addr.sin_family = AF_INET;
  _addr.sin_port = htons(_port);
  inet_pton(AF_INET, _multicast_addr.c_str(), &_addr.sin_addr);

  return true;
}

void MarketDataPublisher::publish(const MarketDataMessage &msg) {
  if (_socket >= 0) {
    sendto(_socket, &msg, sizeof(msg), 0, (struct sockaddr *)&_addr,
           sizeof(_addr));
  }
}

} // namespace network
