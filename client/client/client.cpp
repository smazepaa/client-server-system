#include <iostream>
#include <thread>
#include <string>
#include <winsock2.h>
#include <Ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

class ConnectionManager {

    SOCKET clientSocket;
    sockaddr_in serverAddr;
    int port = 12345;
    PCWSTR serverIp = L"127.0.0.1";

    void clientConfig() {
        clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (clientSocket == INVALID_SOCKET) {
            cerr << "Error creating socket: " << WSAGetLastError() << endl;
            WSACleanup();
            return;
        }

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        InetPton(AF_INET, serverIp, &serverAddr.sin_addr);
    }

    bool winsockInit() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            cerr << "WSAStartup failed" << endl;
            return false;
        }
        return true;
    }

    void cleanup() {
        closesocket(clientSocket);
        WSACleanup();
    }

public:

    ConnectionManager() {
        if (!winsockInit()) {
            throw runtime_error("Winsock initialization failed");
        }
        else {
            clientConfig();
        }
    }

    ~ConnectionManager() {
        cleanup();
    }

    SOCKET getClientSocket() const {
        return this->clientSocket;
    }

    bool isReady() const {
        return clientSocket != INVALID_SOCKET;
    }

    void connectToServer() {
        if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
            cerr << "Connect failed with error: " << WSAGetLastError() << endl;
            return;
        }
        cout << "Client connected to server" << endl << endl;
    }
};

class CommandHandler {

    SOCKET clientSocket;
    string baseDirectory = "C:/Users/sofma/client-dir/";
    string clientDirectory;

public:

    CommandHandler(SOCKET socket) : clientSocket(socket) {}

    ~CommandHandler() {
        closesocket(clientSocket);
        WSACleanup();
    }

    bool isReady() const {
        return clientSocket != INVALID_SOCKET;
    }
};

class Client {
    ConnectionManager connManager;
    CommandHandler cmdHandler;
    SOCKET clientSocket;

public:
    Client() : connManager(),
        cmdHandler(move(connManager.getClientSocket())) {
        clientSocket = connManager.getClientSocket();
        if (connManager.isReady()) {
            connManager.connectToServer();
        }
        else {
            throw runtime_error("Client is not ready for connections");
        }
    }

    void receiveMessages() {
        char buffer[4096];
        while (true) {
            int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytesReceived <= 0) {
                cerr << "Server disconnected.\n";
                break;
            }
            buffer[bytesReceived] = '\0';
            cout << buffer << endl;
        }
    }

    void processInput() {
        if (cmdHandler.isReady()) {
            thread receiveThread([this]() { this->receiveMessages(); });
            string message;
            while (true) {
                getline(std::cin, message);
                send(clientSocket, message.c_str(), message.size() + 1, 0);
            }
        }
    }
};


int main() {
    try {
        Client client;
        while (true) {
            client.processInput();
        }
    }
    catch (const runtime_error& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
    return 0;
}