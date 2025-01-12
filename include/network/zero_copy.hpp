#pragma once

#include <cstdint>
#include <sys/uio.h>
#include <vector>

namespace network {

class ZeroCopyHandler {
public:
  void initBuffers(size_t buffer_size, size_t num_buffers = 4);
  void addToBuffer(const void *data, size_t length);
  ssize_t writeBuffers(int fd);
  ssize_t readToBuffers(int fd);
  std::vector<uint8_t> getReadData();
  void clear();

private:
  std::vector<std::vector<uint8_t>> _buffers;
  std::vector<struct iovec> _iovecs;
  size_t _current_buffer{0};
  size_t _buffer_size{0};
};

// Socket options helper
class SocketOptimiser {
public:
  static bool optimiseSocket(int socket_fd);

private:
  static bool setTcpNoDelay(int socket_fd);
  static bool setReuseAddr(int socket_fd);
  static bool setRecvBuffer(int socket_fd, int size);
  static bool setSendBuffer(int socket_fd, int size);
  static bool setKeepAlive(int socket_fd);
};

} // namespace network
