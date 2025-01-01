#include "../include/network/server.hpp"
#include "../include/orderbook/orderbook.hpp"
#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

using json = nlohmann::json;

class NetworkTest : public ::testing::Test {
protected:
  void SetUp() override {
    server = std::make_unique<network::NetworkServer>(test_port, book);
  }

  void TearDown() override {
    if (server) {
      server->stop();
    }
  }

  // Helper function to create a client socket
  int createClientSocket() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
      throw std::runtime_error("Failed to create client socket");
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(test_port);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
      close(sock);
      throw std::runtime_error("Failed to connect to server");
    }

    return sock;
  }

  // Helper to send message and get response
  std::string sendMessage(int sock, const std::string &message) {
    if (send(sock, message.c_str(), message.length(), 0) < 0) {
      throw std::runtime_error("Failed to send message");
    }

    std::array<char, 4096> buffer{};
    ssize_t bytesRead = recv(sock, buffer.data(), buffer.size() - 1, 0);
    if (bytesRead < 0) {
      throw std::runtime_error("Failed to receive response");
    }

    return std::string(buffer.data(), bytesRead);
  }

  orderbook::OrderBook book;
  std::unique_ptr<network::NetworkServer> server;
  const uint16_t test_port = 8081; // Different from main server port
};

// Test server start/stop
TEST_F(NetworkTest, ServerStartStop) {
  EXPECT_NO_THROW(server->start());
  std::this_thread::sleep_for(
      std::chrono::milliseconds(100)); // Allow server to start
  EXPECT_NO_THROW(server->stop());
}

// Test client connection
TEST_F(NetworkTest, ClientCanConnect) {
  server->start();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  EXPECT_NO_THROW({
    int clientSocket = createClientSocket();
    close(clientSocket);
  });
}

// Test order submission
TEST_F(NetworkTest, CanSubmitOrder) {
  server->start();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  int clientSocket = createClientSocket();

  json orderMsg = {{"type", "new_order"},
                   {"side", "buy"},
                   {"price", 100.0},
                   {"quantity", 10},
                   {"order_id", 12345}};

  std::string response = sendMessage(clientSocket, orderMsg.dump());

  json responseJson = json::parse(response);
  EXPECT_EQ(responseJson["status"], "success");
  EXPECT_EQ(responseJson["order_id"], 12345);

  close(clientSocket);
}

// Test invalid message
TEST_F(NetworkTest, HandlesInvalidMessage) {
  server->start();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  int clientSocket = createClientSocket();

  std::string invalidJson = "not valid json";
  std::string response = sendMessage(clientSocket, invalidJson);

  json responseJson = json::parse(response);
  EXPECT_EQ(responseJson["status"], "error");
  EXPECT_TRUE(responseJson.contains("message"));

  close(clientSocket);
}

// Test multiple clients
TEST_F(NetworkTest, HandlesMultipleClients) {
  server->start();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  constexpr int NUM_CLIENTS = 5;
  std::vector<int> sockets;

  for (int i = 0; i < NUM_CLIENTS; ++i) {
    sockets.push_back(createClientSocket());
  }

  json orderMsg = {{"type", "new_order"},
                   {"side", "buy"},
                   {"price", 100.0},
                   {"quantity", 10},
                   {"order_id", 0}};

  for (int i = 0; i < NUM_CLIENTS; ++i) {
    orderMsg["order_id"] = i;
    std::string response = sendMessage(sockets[i], orderMsg.dump());
    json responseJson = json::parse(response);
    EXPECT_EQ(responseJson["status"], "success");
  }

  for (int socket : sockets) {
    close(socket);
  }
}
