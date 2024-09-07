#include "server.hpp"

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

Server::Server(int port) : port(port) {}

void Server::start() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Error creating socket" << std::endl;
        return;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "Error binding socket" << std::endl;
        close(serverSocket);
        return;
    }

    if (listen(serverSocket, 5) == -1) {
        std::cerr << "Error listening on socket" << std::endl;
        close(serverSocket);
        return;
    }

    std::cout << "Server listening on port " << port << std::endl;

    while (true) {
        sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket == -1) {
            std::cerr << "Error accepting client connection" << std::endl;
            continue;
        }

        std::thread clientThread(&Server::handleClient, this, clientSocket);
        clientThread.detach();
    }
}

void Server::createRoom(const std::string& roomCode) {
    rooms.emplace(roomCode, Game(roomCode));
}

Game* Server::getRoom(const std::string& roomCode) {
    auto it = rooms.find(roomCode);
    return (it != rooms.end()) ? &(it->second) : nullptr;
}

void Server::handleClient(int clientSocket) {
    char buffer[1024];
    std::string currentRoom;
    std::string playerName;

    // Send welcome message
    const char* welcomeMsg = "Welcome to TriangleTrash! Enter your name: ";
    send(clientSocket, welcomeMsg, strlen(welcomeMsg), 0);

    // Receive player name
    int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';
        playerName = buffer;
        playerName.erase(playerName.find_last_not_of(" \n\r\t") + 1);
    }

    while (true) {
        const char* roomPrompt = "Enter room code to join or create (or 'quit' to exit): ";
        send(clientSocket, roomPrompt, strlen(roomPrompt), 0);

        bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) break;

        buffer[bytesRead] = '\0';
        std::string input(buffer);
        input.erase(input.find_last_not_of(" \n\r\t") + 1);

        if (input == "quit") break;

        Game* game = getRoom(input);
        if (!game) {
            createRoom(input);
            game = getRoom(input);
        }

        currentRoom = input;
        game->addPlayer(new Player(playerName, 10000));  // Add player with initial balance

        // Add client to the room's client list
        roomClients[currentRoom].push_back(clientSocket);

        // Set socket to non-blocking mode
        int flags = fcntl(clientSocket, F_GETFL, 0);
        fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK);

        std::string inputBuffer;
        while (true) {
            std::string gameState = game->getGameState();
            std::string fullUpdate = "\033[2J\033[H" + gameState + "\nCOMMAND: ";
            send(clientSocket, fullUpdate.c_str(), fullUpdate.length(), 0);

            char buffer[1024];
            int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);

            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                std::string input(buffer);
                input.erase(input.find_last_not_of(" \n\r\t") + 1);

                if (input == "help") {
                    std::string helpMessage =
                        "Available commands:\n"
                        "  bid <quantity> @<price> - Place a bid order\n"
                        "  ask <quantity> @<price> - Place an ask order\n"
                        "  buy <quantity> - Place a market buy order\n"
                        "  sell <quantity> - Place a market sell order\n"
                        "  write_call <quantity> <strike_price> <days_to_expire> - Write a call option\n"
                        "  write_put <quantity> <strike_price> <days_to_expire> - Write a put option\n"
                        "  buy_option <option_index> - Buy an available option\n"
                        "  exercise_option <option_index> - Exercise an option you own\n"
                        "  back - Leave the current room\n"
                        "  help - Display this help message\n";
                    send(clientSocket, helpMessage.c_str(), helpMessage.length(), 0);
                    continue;
                }

                if (input == "back") {
                    // Remove client from the room's client list
                    auto& clients = roomClients[currentRoom];
                    clients.erase(std::remove(clients.begin(), clients.end(), clientSocket), clients.end());
                    break;
                }

                game->processOrder(playerName, input);
                inputBuffer.clear();
            } else if (bytesRead == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                // No data available, continue with the loop
            } else {
                // Error or disconnect
                break;
            }

            // Short sleep to prevent CPU overuse
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    close(clientSocket);
}