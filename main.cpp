#include <iostream>

#include "server.hpp"

int main() {
    Server server(8080);
    std::cout << "Server started on port 8080" << std::endl;
    server.start();
    return 0;
}