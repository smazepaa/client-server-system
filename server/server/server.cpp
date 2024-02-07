#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <map>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#include "NetworkUtils.h"

using namespace std;
namespace fs = filesystem;
using namespace fs;

mutex consoleMutex;
vector<SOCKET> clients;
map<int, vector<SOCKET>> roomsClients; // Use an int for room ID, adjust as needed
mutex roomsMutex; // Protect access to roomsClients


class ConnectionManager {

    SOCKET serverSocket;
    sockaddr_in serverAddr;
    int port = 12345;

    bool winsockInit() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            cerr << "WSAStartup failed" << endl;
            return false;
        }
        return true;
    }

    void serverConfig() {
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket == INVALID_SOCKET)
        {
            cerr << "Error creating socket: " << WSAGetLastError() << endl;
            WSACleanup();
            return;
        }
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);

        bindSocket();
        listenForConnections();
    }

    void bindSocket() {
        if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
            cerr << "Bind failed with error: " << WSAGetLastError() << endl;
            cleanup();
            return;
        }
    }

    void listenForConnections() {
        if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
        {
            cerr << "Listen failed with error: " << WSAGetLastError() << endl;
            closesocket(serverSocket);
            WSACleanup();
            return;
        }
        cout << "Server listening on port " << port << endl << endl;
    }

    void cleanup() {
        closesocket(serverSocket);
        WSACleanup();
    }

public:
    ConnectionManager() {
        if (!winsockInit()) {
            throw runtime_error("Winsock initialization failed");
        }
        else {
            serverConfig();
        }
    }

    ~ConnectionManager() {
        cleanup();
    }

    bool isReady() const {
        return serverSocket != INVALID_SOCKET;
    }

    SOCKET acceptClient() {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            cerr << "Accept failed with error: " << WSAGetLastError() << endl;
            return INVALID_SOCKET;
        }
        return clientSocket;
    }

};

class CommandHandler {

    SOCKET clientSocket;
    string serverDirectory;
    string baseDirectory = "C:/Users/sofma/server-dir/";
    string clientName;
    int roomId; // Add room ID member

public:

    CommandHandler() : clientSocket(INVALID_SOCKET) {}

    CommandHandler(SOCKET& client) : clientSocket(client) {
        string clientInfo = NetworkUtils::receiveMessage(clientSocket);
        size_t delimiterPos = clientInfo.find(';');
        clientName = clientInfo.substr(0, delimiterPos);
        roomId = stoi(clientInfo.substr(delimiterPos + 1)); // Assuming room ID is always a valid int

        serverDirectory = baseDirectory + clientName;

        cout << "Accepted connection from " << clientName << endl;
        cout << "Working directory: " << serverDirectory << endl;

        if (!fs::exists(serverDirectory)) {
            create_directory(serverDirectory);
        }
    }

    bool isReady() {
        return this->clientSocket != INVALID_SOCKET;
    }

    void broadcastMessage(const string& message, SOCKET senderSocket) {
        lock_guard<mutex> lock(consoleMutex);
        // Broadcast message only to clients in the same room
        for (SOCKET client : roomsClients[roomId]) {
            if (client != senderSocket) {
                NetworkUtils::sendMessage(client, message);
            }
        }
    }

    void handleClient() {
        //clients.push_back(clientSocket);
        roomsClients[roomId].push_back(clientSocket);
        string connectionMessage = clientName + " joined ROOM " + to_string(roomId);
        cout << connectionMessage << endl;
        broadcastMessage(connectionMessage, clientSocket);
        
        while (true) {
            string message = NetworkUtils::receiveMessage(clientSocket);
            if (message == "") {
                lock_guard<mutex> lock(consoleMutex);
                cout << clientName << " disconnected.\n";

                auto& clients = roomsClients[roomId];
                auto it = std::find(clients.begin(), clients.end(), clientSocket);
                if (it != clients.end()) {
                    clients.erase(it);
                }

                closesocket(clientSocket);
                clientSocket = INVALID_SOCKET;
                return;
            }
            string fullMessage = clientName + ": " + message;
            broadcastMessage(fullMessage, clientSocket);
        }
        closesocket(clientSocket);
    }
};


class Server {
    ConnectionManager connManager;
    vector<thread> clientThreads;
    CommandHandler cmdHandler;

    void handleClient(SOCKET clientSocket) {

        CommandHandler cmdHandler(clientSocket);
        while (cmdHandler.isReady()) {
            cmdHandler.handleClient();
        }
    }

public:

    ~Server() {
        for (auto& t : clientThreads) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    void start() {
        while (true) {
            SOCKET clientSocket = connManager.acceptClient();
            if (clientSocket != INVALID_SOCKET) {
                lock_guard<mutex> lock(consoleMutex);
                thread clientThread([this, clientSocket]() {
                    this->handleClient(clientSocket);});
                clientThread.detach();
            }
            else {
                cerr << "Failed to accept a client." << endl;
            }
        }
    }
};

int main() {
    try {
        Server server;
        server.start();
    }
    catch (const runtime_error& e) {
        cerr << "Server Error: " << e.what() << endl;
        return 1;
    }
    return 0;
}