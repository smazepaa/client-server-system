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

public:

    CommandHandler() : clientSocket(INVALID_SOCKET) {}

    CommandHandler(SOCKET& client) : clientSocket(client) {}

    bool isReady() {
        return this->clientSocket != INVALID_SOCKET;
    }

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

    void handleClient() {
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
            cout << "Waiting for a new client..." << endl;
            SOCKET clientSocket = connManager.acceptClient();
            if (clientSocket != INVALID_SOCKET) {
                lock_guard<mutex> lock(consoleMutex);
                cout << "Client " << clientSocket << " connected.\n";
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