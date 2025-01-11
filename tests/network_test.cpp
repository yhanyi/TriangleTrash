#include "../include/network/server.hpp"
#include "../include/session/session.hpp"
#include <arpa/inet.h>
#include <future>
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
    server = std::make_unique<network::NetworkServer>(test_port);
    server->createSession("test_session");
  }

  void TearDown() override {
    if (server) {
      server->stop();
    }
  }

  // Helper function to create a client socket with timeout
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
      throw std::runtime_error("Failed to connect");
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

  // Helper to join session
  bool joinSession(int sock, const std::string &username,
                   const std::string &session_id = "test_session") {
    json joinMsg = {
        {"type", "join"}, {"username", username}, {"session_id", session_id}};

    std::string response = sendMessage(sock, joinMsg.dump());
    json responseJson = json::parse(response);
    return responseJson["status"] == "success";
  }

  std::unique_ptr<network::NetworkServer> server;
  const uint16_t test_port = 8081;
};

// Test server start/stop
TEST_F(NetworkTest, ServerStartStop) {
  EXPECT_NO_THROW(server->start());
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_NO_THROW(server->stop());
}

// Test client connection and session join
TEST_F(NetworkTest, ClientCanJoinSession) {
  server->start();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  int clientSocket = createClientSocket();
  EXPECT_TRUE(joinSession(clientSocket, "trader1"));
  close(clientSocket);
}

// Test duplicate username handling
TEST_F(NetworkTest, RejectsDuplicateUsername) {
  server->start();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  int socket1 = createClientSocket();
  int socket2 = createClientSocket();

  EXPECT_TRUE(joinSession(socket1, "trader1"));
  EXPECT_FALSE(joinSession(socket2, "trader1"));

  close(socket1);
  close(socket2);
}

// Test order submission
TEST_F(NetworkTest, CanSubmitOrder) {
  server->start();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  int clientSocket = createClientSocket();
  EXPECT_TRUE(joinSession(clientSocket, "trader1"));

  json orderMsg = {{"type", "new_order"}, {"session_id", "test_session"},
                   {"side", "buy"},       {"price", 100.0},
                   {"quantity", 10},      {"order_id", 12345}};

  std::string response = sendMessage(clientSocket, orderMsg.dump());
  json responseJson = json::parse(response);
  EXPECT_EQ(responseJson["status"], "success");

  close(clientSocket);
}

// Test order matching between users
TEST_F(NetworkTest, MatchesOrdersBetweenUsers) {
  server->start();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  int socket1 = createClientSocket();
  int socket2 = createClientSocket();
  std::string response;

  // Join session
  EXPECT_TRUE(joinSession(socket1, "seller"));
  EXPECT_TRUE(joinSession(socket2, "buyer"));

  // Get the session and initialize seller's position
  auto *session = server->getSession("test_session");
  ASSERT_NE(session, nullptr);
  auto seller = session->getUser("seller");
  ASSERT_NE(seller, nullptr);
  seller->addPosition("STOCK", 20); // Give seller initial position

  // Now test the order matching

  // First, have the buyer place a buy order
  json buyOrder = {{"type", "new_order"}, {"session_id", "test_session"},
                   {"side", "buy"},       {"price", 90.0},
                   {"quantity", 10},      {"order_id", 0}};
  response = sendMessage(socket2, buyOrder.dump());
  EXPECT_EQ(json::parse(response)["status"], "success");

  // Seller places matching sell order
  json sellOrder = {{"type", "new_order"}, {"session_id", "test_session"},
                    {"side", "sell"},      {"price", 90.0},
                    {"quantity", 10},      {"order_id", 1}};
  response = sendMessage(socket1, sellOrder.dump());
  EXPECT_EQ(json::parse(response)["status"], "success");

  // Close sockets
  close(socket1);
  close(socket2);
}

// Test insufficient funds handling
TEST_F(NetworkTest, HandlesInsufficientFunds) {
  server->start();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  int clientSocket = createClientSocket();
  EXPECT_TRUE(joinSession(clientSocket, "trader1"));

  json orderMsg = {{"type", "new_order"},
                   {"session_id", "test_session"},
                   {"side", "buy"},
                   {"price", 20000.0}, // Price * quantity > initial balance
                   {"quantity", 1000},
                   {"order_id", 12345}};

  std::string response = sendMessage(clientSocket, orderMsg.dump());
  json responseJson = json::parse(response);
  EXPECT_EQ(responseJson["status"], "error");
  EXPECT_TRUE(responseJson["message"].get<std::string>().find(
                  "Insufficient funds") != std::string::npos);

  close(clientSocket);
}

// Test multiple clients
TEST_F(NetworkTest, HandlesMultipleClients) {
  server->start();

  constexpr int NUM_CLIENTS = 5;
  std::vector<std::future<bool>> client_futures;
  std::vector<int> sockets;
  sockets.reserve(NUM_CLIENTS);

  // Create clients concurrently
  auto handle_client = [this](int client_id) -> bool {
    try {
      int sock = createClientSocket();

      // Join session
      if (!joinSession(sock, "trader" + std::to_string(client_id))) {
        close(sock);
        return false;
      }

      // Place order
      json orderMsg = {{"type", "new_order"}, {"session_id", "test_session"},
                       {"side", "buy"},       {"price", 100.0},
                       {"quantity", 1},       {"order_id", client_id}};

      std::string response = sendMessage(sock, orderMsg.dump());
      json responseJson = json::parse(response);
      bool success = responseJson["status"] == "success";

      close(sock);
      return success;
    } catch (const std::exception &e) {
      return false;
    }
  };

  // Launch all clients concurrently
  for (int i = 0; i < NUM_CLIENTS; ++i) {
    client_futures.emplace_back(
        std::async(std::launch::async, handle_client, i));
  }

  // Wait for all clients and verify results
  for (auto &future : client_futures) {
    EXPECT_TRUE(future.get());
  }
}
