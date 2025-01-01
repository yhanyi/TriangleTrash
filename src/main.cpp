#include "../include/network/server.hpp"
#include <chrono>
#include <iostream>
#include <thread>

int main() {
  std::cout << "Starting △ TriangleTrash △ Server...\n";

  try {
    network::NetworkServer server(8080);

    server.start();

    std::cout << "Server running on port 8080\n";
    std::cout << "Default session created. Users can connect using 'default' "
                 "as session_id\n";
    std::cout << "Press Ctrl+C to exit.\n";

    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
