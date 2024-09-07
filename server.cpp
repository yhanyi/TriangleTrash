#include "server.hpp"

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <thread>

#include "game.hpp"
#include "player.hpp"
#include "server.hpp"

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

    const char* welcomeMsg = "Welcome to TriangleTrash! Enter your name: ";
    send(clientSocket, welcomeMsg, strlen(welcomeMsg), 0);

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
        game->addPlayer(new Player(playerName, 10000));

        while (true) {
            std::string gameState = game->getGameState();
            send(clientSocket, gameState.c_str(), gameState.length(), 0);

            const char* orderPrompt = "\nEnter order (e.g., 'bid 5 @500' or 'ask 3 @600') or 'back' to change room: ";
            send(clientSocket, orderPrompt, strlen(orderPrompt), 0);

            bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytesRead <= 0) break;

            buffer[bytesRead] = '\0';
            input = buffer;
            input.erase(input.find_last_not_of(" \n\r\t") + 1);

            if (input == "back") break;

            game->processOrder(playerName, input);
        }
    }

    close(clientSocket);
}