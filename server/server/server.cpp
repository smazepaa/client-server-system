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

void broadcastMessage(const string& message, SOCKET senderSocket) {
	lock_guard<mutex> lock(consoleMutex);
	string fullMessage = "Client " + to_string(senderSocket) + ": " + message;
	cout << fullMessage << endl;
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
			lock_guard<mutex> lock(consoleMutex);
			cout << "Client " << clientSocket << " disconnected.\n";
			break;
		}
		buffer[bytesReceived] = '\0';
		string message(buffer);
		broadcastMessage(message, clientSocket);
	}
	closesocket(clientSocket);
}

int main() {
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		cerr << "WSAStartup failed.\n";
		return 1;
	}
	SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket == INVALID_SOCKET) {
		cerr << "Socket creation failed.\n";
		WSACleanup();
		return 1;
	}
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons(12345);
	if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) ==
		SOCKET_ERROR) {
		cerr << "Bind failed.\n";
		closesocket(serverSocket);
		WSACleanup();
		return 1;
	}
	if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
		cerr << "Listen failed.\n";
		closesocket(serverSocket);
		WSACleanup();
		return 1;
	}
	cout << "Server is listening on port 12345...\n";
	while (true) {
		SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
		if (clientSocket == INVALID_SOCKET) {
			cerr << "Accept failed.\n";
			closesocket(serverSocket);
			WSACleanup();
			return 1;
		}
		lock_guard<mutex> lock(consoleMutex);
		cout << "Client " << clientSocket << " connected.\n";
		thread clientThread(handleClient, clientSocket);
		clientThread.detach(); // Detach the thread to allow handling multiple clients concurrently
	}
	closesocket(serverSocket);
	WSACleanup();
	return 0;
}