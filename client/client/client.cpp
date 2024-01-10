#include <iostream>
#include <WinSock2.h>
#include <Ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

class Client {
private:
    SOCKET clientSocket;
    sockaddr_in serverAddr;
    int port = 12345;
    PCWSTR serverIp = L"127.0.0.1";

    void clientConfig() {
        clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (clientSocket == INVALID_SOCKET) {
            cerr << "Error creating socket: " << WSAGetLastError() << endl;
            throw runtime_error("Error creating socket");
        }

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        InetPton(AF_INET, serverIp, &serverAddr.sin_addr);
    }

    void connectToServer() {
        
        clientConfig();

        if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
            cerr << "Connect failed with error: " << WSAGetLastError() << endl;
            throw runtime_error("Connect failed");
        }
    }

    void winsockInit() {
        // Initialize Winsock
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            cerr << "WSAStartup failed" << endl;
            throw std::runtime_error("WSAStartup failed");
        }
    }

public:
    Client(){
        winsockInit();
        connectToServer();
    }

    ~Client() {
        closesocket(clientSocket);
        WSACleanup();
    }

    void sendMessage(const char* message) {
        send(clientSocket, message, (int)strlen(message), 0);
    }

    string receiveMessage() {
        char buffer[1024];
        memset(buffer, 0, 1024);
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived > 0) {
            return string(buffer);
        }
        else {
            return "";
        }
    }
};

int main() {

    Client client;
    client.sendMessage("Hello, server! How are you?");
    string response = client.receiveMessage();
    cout << "Received from server: " << response << endl;

    return 0;
}
