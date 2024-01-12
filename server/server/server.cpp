#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <filesystem>
#include <WinSock2.h>
#include <Ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;
namespace fs = std::filesystem;

class Server {

    SOCKET serverSocket;
    SOCKET clientSocket;
    sockaddr_in serverAddr;
    int port = 12345;
    char buffer[1024]; // for storing the message from client
    string filename;
    string serverDirectory = "C:/Users/sofma/server-dir";

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

    void handleCommands(const string& command, const string& filename) {
        if (command == "LIST") {
            listFiles();
            cout << "creating a list" << endl;
        }

        else if (command == "GET") {

        }

        else if (command == "PUT") {

        }

        else if (command == "INFO") {

        }

        else if (command == "DELETE") {

        }

        else {
            cout << "invalid command" << endl;
        }
    }

    void listFiles(){
        string fileList = "Files in directory:\n";

        for (const auto& entry : fs::directory_iterator(serverDirectory)) {
            fileList += entry.path().filename().string() + "\n";
        }

        // Send the file list to the client
        sendResponse(fileList.c_str());
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

    void recieveCommands() {

        bool received = receiveMessage();
        if (received) {
            cout << "Received data: " << buffer << endl;

            vector<string> params;
            istringstream iss(buffer);
            string word;
            while (iss >> word) {
                params.push_back(word);
            }

            string command = move(params[0]);

            if (command != "LIST") {
                filename = move(params[1]);
            }

            handleCommands(command, filename);
        }
    }
};

int main() {
    
    Server server;
    while (true) {
        server.recieveCommands();
    }

    return 0;
}
