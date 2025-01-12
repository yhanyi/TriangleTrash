#include "../include/network/protocol.hpp"
#include "../include/network/zero_copy.hpp"
#include "fcntl.h"
#include <gtest/gtest.h>
#include <thread>

using namespace network;

class ProtocolTest : public ::testing::Test {
protected:
  void SetUp() override {}
};

TEST_F(ProtocolTest, SerializationRoundTrip) {
  // Test join message serialisation
  std::string username = "trader1";
  std::string session_id = "test_session";

  auto join_data = BinaryProtocol::serializeJoin(username, session_id);
  ASSERT_GE(join_data.size(), sizeof(JoinMessage));

  // Verify the serialised data
  auto *msg = reinterpret_cast<const JoinMessage *>(join_data.data());
  EXPECT_EQ(msg->header.type, MessageType::JOIN);
  EXPECT_EQ(std::string(msg->username), username);
  EXPECT_EQ(std::string(msg->session_id), session_id);
}

TEST_F(ProtocolTest, NetworkByteOrderConversion) {
  // Test double conversion
  double original = 123.456;
  double converted =
      BinaryProtocol::ntohDouble(BinaryProtocol::htonDouble(original));
  EXPECT_DOUBLE_EQ(original, converted);

  // Test 64-bit integer conversion
  uint64_t orig_int = 0x1234567890ABCDEF;
  uint64_t conv_int = BinaryProtocol::ntoh64(BinaryProtocol::hton64(orig_int));
  EXPECT_EQ(orig_int, conv_int);
}

TEST_F(ProtocolTest, MarketDataMessage) {
  // Create a market data message
  MarketDataMessage msg{};
  msg.header.type = MessageType::MARKET_DATA;
  msg.header.length = sizeof(MarketDataMessage) - sizeof(MessageHeader);
  msg.header.seq_num = BinaryProtocol::hton32(1);

  const char *symbol = "AAPL";
  strncpy(msg.symbol, symbol, sizeof(msg.symbol) - 1);
  msg.best_bid = BinaryProtocol::htonDouble(150.25);
  msg.best_ask = BinaryProtocol::htonDouble(150.30);
  msg.bid_size = BinaryProtocol::hton32(100);
  msg.ask_size = BinaryProtocol::hton32(150);

  // Verify fields
  EXPECT_EQ(std::string(msg.symbol), "AAPL");
  EXPECT_DOUBLE_EQ(BinaryProtocol::ntohDouble(msg.best_bid), 150.25);
  EXPECT_DOUBLE_EQ(BinaryProtocol::ntohDouble(msg.best_ask), 150.30);
  EXPECT_EQ(BinaryProtocol::ntoh32(msg.bid_size), 100);
  EXPECT_EQ(BinaryProtocol::ntoh32(msg.ask_size), 150);
}

TEST_F(ProtocolTest, ZeroCopyBufferHandling) {
  // Create handler with multiple small buffers to test buffer boundary
  // conditions
  ZeroCopyHandler handler;
  const size_t BUFFER_SIZE = 16; // Small buffer size to test boundaries
  const size_t NUM_BUFFERS = 4;  // Multiple buffers to test spanning
  handler.initBuffers(BUFFER_SIZE, NUM_BUFFERS);

  // Create test data smaller than total buffer capacity
  const std::string test_data =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMN"; // 40 bytes

  // Create socketpair for testing
  int sockfd[2];
  ASSERT_NE(socketpair(AF_UNIX, SOCK_STREAM, 0, sockfd), -1)
      << "Failed to create socket pair: " << strerror(errno);

  // Set non-blocking mode for both sockets
  for (int i = 0; i < 2; i++) {
    int flags = fcntl(sockfd[i], F_GETFL, 0);
    ASSERT_NE(flags, -1);
    ASSERT_NE(fcntl(sockfd[i], F_SETFL, flags | O_NONBLOCK), -1);
  }

  // Add all data at once to the handler
  handler.addToBuffer(test_data.data(), test_data.size());

  // Write the data
  ssize_t total_written = 0;
  int write_attempts = 0;
  const int MAX_ATTEMPTS = 10;

  while (total_written < static_cast<ssize_t>(test_data.size()) &&
         write_attempts < MAX_ATTEMPTS) {
    ssize_t written = handler.writeBuffers(sockfd[0]);
    if (written == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }
      ADD_FAILURE() << "Write error: " << strerror(errno);
      break;
    }
    if (written == 0) {
      break; // No more data to write
    }
    total_written += written;
    write_attempts++;
  }

  ASSERT_EQ(total_written, static_cast<ssize_t>(test_data.size()))
      << "Failed to write complete data. Expected: " << test_data.size()
      << " Actual: " << total_written;

  // Read back using new handler
  ZeroCopyHandler read_handler;
  read_handler.initBuffers(BUFFER_SIZE, NUM_BUFFERS);

  std::vector<uint8_t> received_data;
  int read_attempts = 0;

  while (received_data.size() < test_data.size() &&
         read_attempts < MAX_ATTEMPTS) {
    ssize_t bytes_read = read_handler.readToBuffers(sockfd[1]);
    if (bytes_read == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }
      ADD_FAILURE() << "Read error: " << strerror(errno);
      break;
    }
    if (bytes_read == 0) {
      break; // EOF
    }

    std::cout << "Read " << bytes_read << " bytes in attempt " << read_attempts
              << std::endl;

    auto chunk = read_handler.getReadData();
    std::cout << "Chunk size: " << chunk.size() << std::endl;

    received_data.insert(received_data.end(), chunk.begin(), chunk.end());
    read_handler.clear();
    read_attempts++;
  }

  close(sockfd[0]);
  close(sockfd[1]);

  std::cout << "Original data size: " << test_data.size() << std::endl;
  std::cout << "Received data size: " << received_data.size() << std::endl;

  ASSERT_EQ(received_data.size(), test_data.size())
      << "Data size mismatch. Expected: " << test_data.size()
      << " Actual: " << received_data.size();

  std::string received_str(received_data.begin(), received_data.end());
  EXPECT_EQ(received_str, test_data) << "Data content mismatch";

  if (received_str != test_data) {
    std::cout << "Expected: " << test_data << std::endl;
    std::cout << "Received: " << received_str << std::endl;
  }
}
