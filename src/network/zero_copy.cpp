#include "../../include/network/zero_copy.hpp"
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

namespace network {

void ZeroCopyHandler::initBuffers(size_t buffer_size, size_t num_buffers) {
  _buffer_size = buffer_size;
  _buffers.resize(num_buffers);
  _iovecs.resize(num_buffers);

  for (auto &buffer : _buffers) {
    buffer.resize(buffer_size);
  }

  clear();
}

void ZeroCopyHandler::addToBuffer(const void *data, size_t length) {
  if (_current_buffer >= _buffers.size()) {
    return;
  }

  const uint8_t *src = static_cast<const uint8_t *>(data);
  size_t remaining = length;

  while (remaining > 0 && _current_buffer < _buffers.size()) {
    size_t space_in_buffer = _buffer_size - _iovecs[_current_buffer].iov_len;
    if (space_in_buffer == 0) {
      _current_buffer++;
      if (_current_buffer >= _buffers.size()) {
        break;
      }
      continue;
    }

    size_t to_copy = std::min(remaining, space_in_buffer);
    memcpy(static_cast<uint8_t *>(_iovecs[_current_buffer].iov_base) +
               _iovecs[_current_buffer].iov_len,
           src, to_copy);

    _iovecs[_current_buffer].iov_len += to_copy;
    src += to_copy;
    remaining -= to_copy;
  }
}

ssize_t ZeroCopyHandler::writeBuffers(int fd) {
  size_t num_active_buffers = 0;
  for (const auto &iov : _iovecs) {
    if (iov.iov_len > 0) {
      num_active_buffers++;
    }
  }

  if (num_active_buffers == 0) {
    return 0;
  }

  ssize_t written = writev(fd, _iovecs.data(), num_active_buffers);
  if (written > 0) {
    size_t remaining = written;
    for (auto &iov : _iovecs) {
      if (remaining == 0)
        break;
      if (iov.iov_len <= remaining) {
        remaining -= iov.iov_len;
        iov.iov_len = 0;
      } else {
        memmove(iov.iov_base, static_cast<uint8_t *>(iov.iov_base) + remaining,
                iov.iov_len - remaining);
        iov.iov_len -= remaining;
        remaining = 0;
      }
    }
  }
  return written;
}

ssize_t ZeroCopyHandler::readToBuffers(int fd) {
  std::vector<struct iovec> read_iovecs(_iovecs.size());
  for (size_t i = 0; i < read_iovecs.size(); i++) {
    read_iovecs[i].iov_base = _buffers[i].data();
    read_iovecs[i].iov_len = _buffer_size;
  }

  ssize_t bytes_read = readv(fd, read_iovecs.data(), read_iovecs.size());

  if (bytes_read > 0) {
    for (auto &iov : _iovecs) {
      iov.iov_len = 0;
    }

    size_t remaining = bytes_read;
    size_t buffer_idx = 0;

    while (remaining > 0 && buffer_idx < _iovecs.size()) {
      size_t buffer_bytes = std::min(remaining, _buffer_size);
      _iovecs[buffer_idx].iov_len = buffer_bytes;
      remaining -= buffer_bytes;
      buffer_idx++;
    }
  }

  return bytes_read;
}

std::vector<uint8_t> ZeroCopyHandler::getReadData() {
  std::vector<uint8_t> result;
  size_t total_size = 0;

  for (const auto &iov : _iovecs) {
    total_size += iov.iov_len;
  }

  result.reserve(total_size);

  for (const auto &iov : _iovecs) {
    if (iov.iov_len > 0) {
      result.insert(result.end(), static_cast<const uint8_t *>(iov.iov_base),
                    static_cast<const uint8_t *>(iov.iov_base) + iov.iov_len);
    }
  }

  return result;
}

void ZeroCopyHandler::clear() {
  _current_buffer = 0;
  for (size_t i = 0; i < _iovecs.size(); i++) {
    _iovecs[i].iov_base = _buffers[i].data();
    _iovecs[i].iov_len = 0;
  }
}

bool SocketOptimiser::optimiseSocket(int socket_fd) {
  return setTcpNoDelay(socket_fd) && setReuseAddr(socket_fd) &&
         setRecvBuffer(socket_fd, 1024 * 1024) && // 1MB buffer
         setSendBuffer(socket_fd, 1024 * 1024) && setKeepAlive(socket_fd);
}

bool SocketOptimiser::setTcpNoDelay(int socket_fd) {
  int flag = 1;
  return setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) ==
         0;
}

bool SocketOptimiser::setReuseAddr(int socket_fd) {
  int flag = 1;
  return setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) ==
         0;
}

bool SocketOptimiser::setRecvBuffer(int socket_fd, int size) {
  return setsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) == 0;
}

bool SocketOptimiser::setSendBuffer(int socket_fd, int size) {
  return setsockopt(socket_fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) == 0;
}

bool SocketOptimiser::setKeepAlive(int socket_fd) {
  int flag = 1;
  return setsockopt(socket_fd, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag)) ==
         0;
}

} // namespace network
