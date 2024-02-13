#include <iostream>
#include <thread>
#include <string>
#include <winsock2.h>
#include <Ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include "NetworkUtils.h"

using namespace std;
namespace fs = std::filesystem;
using namespace fs;

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

    bool inRoom = false;

    void quitRoom() {
        string confirmation;
        cout << "Do you want to leave the room? [y/n] ";
        getline(cin, confirmation);
        cout << "\033[A\033[2K";
        if (confirmation == "y") {
            NetworkUtils::sendMessage(clientSocket, ".q");
            cout << "You left the room" << endl;
            inRoom = false;
        }
        else if (confirmation == "n") {
            cout << "Remaining in the same room." << endl;
        }
        else {
            cout << "Invalid response." << endl;
        }
    }

public:

    CommandHandler(SOCKET socket) : clientSocket(socket) {}

    ~CommandHandler() {
        closesocket(clientSocket);
        WSACleanup();
    }

    bool isReady() const {
        return clientSocket != INVALID_SOCKET;
    }

    void sendClientName() {
        string name, roomId;
        while (name == "") {
            cout << "Enter client name: ";
            getline(cin, name);
            if (name != "") {
                cout << "Enter room ID: ";
                getline(cin, roomId);
                cout << "You joined ROOM " << roomId << endl << endl;
                string clientInfo = name + ";" + roomId;
                NetworkUtils::sendMessage(clientSocket, clientInfo);
                inRoom = true;

                clientDirectory = baseDirectory + name;
                if (!fs::exists(clientDirectory)) {
                    create_directory(clientDirectory);
                }
            }

            else {
                cout << "Cannot proceed without client name" << endl;
            }
        }
    }

    void handleCommands(const string& message) {
        stringstream ss(message);
        string command;
        getline(ss, command, ' ');
        cout << "\033[A\033[2K";
        // \033[A moves the cursor up one line, and \033[2K clears the entire line.

        if (command == ".m") {
            if (inRoom) {
                string messageContent;
                getline(ss, messageContent);
                if (!messageContent.empty()) {
                    cout << "You: " << messageContent << endl;
                    NetworkUtils::sendMessage(clientSocket, messageContent);
                }
                else {
                    cout << "Cannot send an empty message." << endl;
                }
            }
            else {
                cout << "Join a room to send messages." << endl;
            }
            
        }
        else if (command == ".q") {
            quitRoom();
        }

        else if (command == ".j") {
            string newRoomId;
            getline(ss, newRoomId);
            if (newRoomId != "") {
                NetworkUtils::sendMessage(clientSocket, ".j " + newRoomId);
                inRoom = true;
            }
            else {
                cout << "Cannot proceed without the room id." << endl;
            }
        }

        else {
            cout << endl;
            cout << "Unknown command or message not prefixed correctly. Please use\n" <<
                "-> .m <message> to send a message\n" << "->.f <filename> to send a file\n" <<
                "-> .q to leave the room\n" << "-> .j <id> to join a room" << endl;
            cout << endl;
        }
    }
};

class Client {
    ConnectionManager connManager;
    CommandHandler cmdHandler;
    SOCKET clientSocket;

    thread receiveThread;

public:
    Client() : connManager(),
        cmdHandler(move(connManager.getClientSocket())) {
        clientSocket = connManager.getClientSocket();
        if (connManager.isReady()) {
            connManager.connectToServer();
            cmdHandler.sendClientName();
        }
        else {
            throw runtime_error("Client is not ready for connections");
        }
    }

    ~Client() {
        if (receiveThread.joinable()) {
            receiveThread.join();
        }
        closesocket(clientSocket);
    }

    void receiveMessages() {
        while (true) {
            string message = NetworkUtils::receiveMessage(clientSocket);
            if (message == "") {
                cerr << "Server disconnected.\n";
                return;
            }
            cout << message << endl;
        }
    }

    void processInput() {
        if (cmdHandler.isReady()) {
            receiveThread = thread(&Client::receiveMessages, this);
            string message;
            while (true) {
                getline(cin, message);
                cmdHandler.handleCommands(message);
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