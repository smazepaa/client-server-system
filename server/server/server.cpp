#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

mutex consoleMutex;
vector<SOCKET> clients;

void broadcastMessage(const std::string& message, SOCKET senderSocket) {
	std::lock_guard<std::mutex> lock(consoleMutex);
	string fullMessage = "Client " + to_string(senderSocket) + ": " + message;
	std::cout << fullMessage << std::endl;
	for (SOCKET client : clients) {
		if (client != senderSocket) {
			send(client, fullMessage.c_str(), fullMessage.size() + 1, 0);
		}
	}
}

void handleClient(SOCKET clientSocket) {
	clients.push_back(clientSocket);
	char buffer[4096];
	while (true) {
		int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
		if (bytesReceived <= 0) {
			std::lock_guard<std::mutex> lock(consoleMutex);
			std::cout << "Client " << clientSocket << " disconnected.\n";
			break;
		}
		buffer[bytesReceived] = '\0';
		std::string message(buffer);
		broadcastMessage(message, clientSocket);
	}
	closesocket(clientSocket);
}

int main() {
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cerr << "WSAStartup failed.\n";
		return 1;
	}
	SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket == INVALID_SOCKET) {
		std::cerr << "Socket creation failed.\n";
		WSACleanup();
		return 1;
	}
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons(8080);
	if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) ==
		SOCKET_ERROR) {
		std::cerr << "Bind failed.\n";
		closesocket(serverSocket);
		WSACleanup();
		return 1;
	}
	if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
		std::cerr << "Listen failed.\n";
		closesocket(serverSocket);
		WSACleanup();
		return 1;
	}
	std::cout << "Server is listening on port 8080...\n";
	while (true) {
		SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
		if (clientSocket == INVALID_SOCKET) {
			std::cerr << "Accept failed.\n";
			closesocket(serverSocket);
			WSACleanup();
			return 1;
		}
		std::lock_guard<std::mutex> lock(consoleMutex);
		std::cout << "Client " << clientSocket << " connected.\n";
		std::thread clientThread(handleClient, clientSocket);
		clientThread.detach(); // Detach the thread to allow handling multiple clients concurrently
	}
	closesocket(serverSocket);
	WSACleanup();
	return 0;
}