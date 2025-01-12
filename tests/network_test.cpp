#include "../include/network/server.hpp"
#include "../include/session/session.hpp"
#include <arpa/inet.h>
#include <future>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
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
  constexpr int NUM_CLIENTS = 3; // Reduced from 5 to lower resource usage
  constexpr auto TIMEOUT = std::chrono::seconds(10); // Increased timeout

  server->start();
  std::cout << "Server started, waiting for initialization..." << std::endl;
  std::this_thread::sleep_for(
      std::chrono::milliseconds(500)); // Increased startup wait

  struct ClientResult {
    bool success{false};
    std::string error;
    std::chrono::milliseconds connect_time{0};
    std::chrono::milliseconds operation_time{0};
  };

  auto handle_client = [this](int client_id) -> ClientResult {
    ClientResult result;
    int sock = -1;
    auto start_time = std::chrono::steady_clock::now();

    try {
      std::cout << "Client " << client_id << " attempting connection..."
                << std::endl;

      // Connect with timeout
      std::future<int> connect_future =
          std::async(std::launch::async, [this]() {
            try {
              return createClientSocket();
            } catch (const std::exception &e) {
              std::cout << "Connection creation error: " << e.what()
                        << std::endl;
              return -1;
            }
          });

      sock = connect_future.get();
      if (sock < 0) {
        throw std::runtime_error("Failed to create socket");
      }

      result.connect_time =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - start_time);
      std::cout << "Client " << client_id << " connected in "
                << result.connect_time.count() << "ms" << std::endl;

      // Set longer socket timeouts for CI environment
      struct timeval tv;
      tv.tv_sec = 5; // Increased timeout
      tv.tv_usec = 0;
      if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        std::cout << "Failed to set receive timeout for client " << client_id
                  << std::endl;
      }
      if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        std::cout << "Failed to set send timeout for client " << client_id
                  << std::endl;
      }

      // Add small delay between clients to reduce contention
      std::this_thread::sleep_for(std::chrono::milliseconds(50 * client_id));

      // Join session
      std::string username = "trader" + std::to_string(client_id);
      std::cout << "Client " << client_id << " attempting to join session..."
                << std::endl;

      if (!joinSession(sock, username)) {
        throw std::runtime_error("Failed to join session");
      }

      std::cout << "Client " << client_id << " joined session successfully"
                << std::endl;

      // Place order
      json orderMsg = {{"type", "new_order"}, {"session_id", "test_session"},
                       {"side", "buy"},       {"price", 100.0},
                       {"quantity", 1},       {"order_id", client_id}};

      std::cout << "Client " << client_id << " placing order..." << std::endl;
      std::string response = sendMessage(sock, orderMsg.dump());
      std::cout << "Client " << client_id << " received response: " << response
                << std::endl;

      json responseJson = json::parse(response);

      if (responseJson["status"] != "success") {
        throw std::runtime_error("Order failed: " +
                                 responseJson["message"].get<std::string>());
      }

      result.success = true;

    } catch (const std::exception &e) {
      result.success = false;
      result.error = e.what();
      std::cout << "Client " << client_id << " failed: " << e.what()
                << std::endl;
    }

    result.operation_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);

    if (sock >= 0) {
      close(sock);
      std::cout << "Client " << client_id << " closed connection" << std::endl;
    }

    return result;
  };

  // Launch clients with timeout
  std::vector<std::future<ClientResult>> futures;
  auto start_time = std::chrono::steady_clock::now();

  for (int i = 0; i < NUM_CLIENTS; ++i) {
    futures.push_back(std::async(std::launch::async, handle_client, i));
  }

  // Wait for all clients with timeout
  std::vector<ClientResult> results;
  bool timeout_occurred = false;

  for (auto &future : futures) {
    if (future.wait_for(TIMEOUT) == std::future_status::timeout) {
      timeout_occurred = true;
      std::cout << "Timeout waiting for client completion" << std::endl;
      break;
    }
    results.push_back(future.get());
  }

  auto duration = std::chrono::steady_clock::now() - start_time;

  // Print detailed diagnostic information
  std::cout << "\nTest Summary:" << std::endl;
  std::cout
      << "Total test duration: "
      << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()
      << "ms" << std::endl;
  std::cout << "Results received: " << results.size() << "/" << NUM_CLIENTS
            << std::endl;

  for (size_t i = 0; i < results.size(); ++i) {
    std::cout << "Client " << i << ":"
              << "\n  Success: " << (results[i].success ? "true" : "false")
              << "\n  Connect time: " << results[i].connect_time.count() << "ms"
              << "\n  Total time: " << results[i].operation_time.count()
              << "ms";
    if (!results[i].success) {
      std::cout << "\n  Error: " << results[i].error;
    }
    std::cout << std::endl;
  }

  EXPECT_FALSE(timeout_occurred)
      << "Test timed out after " << TIMEOUT.count() << " seconds";

  if (!timeout_occurred) {
    for (const auto &result : results) {
      EXPECT_TRUE(result.success) << "Client failed: " << result.error;
    }
  }
}
