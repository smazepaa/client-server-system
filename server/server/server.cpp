#include <iostream>
#include <WinSock2.h>
#include <Ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

class Server {

    SOCKET serverSocket;
    SOCKET clientSocket;
    sockaddr_in serverAddr;
    int port = 12345;
    char buffer[1024]; // for storing the message from client

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
    }

    void winsockInit() {
        // Initialize Winsock
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            cerr << "WSAStartup failed" << endl;
            return;
        }
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
        cout << "Server listening on port " << port << endl;
    }

    void acceptClient() {
        clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            cerr << "Accept failed with error: " << WSAGetLastError() << endl;
            closesocket(serverSocket);
            WSACleanup();
            return;
        }
    }

    void sendResponse(const char* response) {
        send(clientSocket, response, (int)strlen(response), 0);
    }

    bool receiveMessage() {
        memset(buffer, 0, 1024);
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        return (bytesReceived > 0);
    }

    void cleanup() {
        closesocket(serverSocket);
        WSACleanup();
    }

public:
    Server(){
        winsockInit();
        serverConfig();
        bindSocket();
        listenForConnections();
        acceptClient();
    }

    ~Server() {
        cleanup();
    }

    void communicateWithClient() {
        
        bool received = receiveMessage();
        if (received) {
            cout << "Received data: " << buffer << endl;

            const char* response = "Hello, client! This is the server.";
            sendResponse(response);
        }
    }
};

int main() {
    
    Server server;
    server.communicateWithClient();

    return 0;
}
