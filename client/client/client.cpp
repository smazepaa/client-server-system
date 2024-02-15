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

atomic<bool> bFileReceivingState = false;

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

public:

    string clientDirectory;

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

                clientDirectory = baseDirectory + name;
                if (!fs::exists(clientDirectory)) {
                    create_directory(clientDirectory);
                }

                // cout << "Enter messages: " << endl;
            }
            else {
                cout << "Cannot proceed with empty client name" << endl;
            }
        }
    }

    void handleCommands(const string& message) {
        stringstream ss(message);
        string command;
        getline(ss, command, ' ');

        if (command == ".m") {
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
        else if (command == ".q") {
            string confirmation;
            cout << "Do you want to leave the room? [y/n] ";
            getline(cin, confirmation);
            if (confirmation == "y") {
                NetworkUtils::sendMessage(clientSocket, ".q");
                // cout << NetworkUtils::receiveMessage(clientSocket) << endl;
                cout << "you left the room" << endl;

                string newRoomId;
                cout << "Enter new room ID to join or type '.q' to quit: ";
                getline(cin, newRoomId);
                if (newRoomId != ".q") {
                    NetworkUtils::sendMessage(clientSocket, ".j " + newRoomId);
                    cout << NetworkUtils::receiveMessage(clientSocket) << endl;
                }
            }

        }
        else if (command == ".f") {
            string filename;
            getline(ss, filename);
            string filepath = clientDirectory + "/" + filename;

            NetworkUtils::sendMessage(clientSocket, ".f " + filename);
            string response = NetworkUtils::sendFile(filepath, clientSocket);
            cout << response << endl;
        }

        else {
            cout << "Unknown command or message not prefixed correctly. Please use .m to send a message." << endl;
        }
    }
};

class Client {
    ConnectionManager connManager;
    CommandHandler cmdHandler;
    SOCKET clientSocket;

    string extractFileNameFromNotice(const string& message) {
        size_t startPos = message.find(": ") + 2;
        size_t endPos = message.find(". Accept? [Y/N]");
        if (startPos != string::npos && endPos != string::npos) {
            return message.substr(startPos, endPos - startPos);
        }
        return "";
    }


public:

    bool expectingFile = false;
    string expectedFileName;

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

    void receiveMessages() {
        while (true) {
            string message = NetworkUtils::receiveMessage(clientSocket);
            if (bFileReceivingState) {
                NetworkUtils::sendMessage(clientSocket, ".a " + expectedFileName);
                string path = cmdHandler.clientDirectory + "/" + expectedFileName;
                cout << path << endl;
                bFileReceivingState = false;
                cout << NetworkUtils::receiveFile(path, clientSocket) << endl;
            }
            else {
                if (message == "") {
                    cerr << "Server disconnected.\n";
                    break;
                }

                if (message._Starts_with("Incoming file: ")) {
                    expectedFileName = extractFileNameFromNotice(message);
                    cout << message << endl;
                    expectingFile = true;
                }
                else if (!expectingFile) {
                    cout << message << endl;
                }
            }
            
        }
    }

    void processInput() {
        if (cmdHandler.isReady()) {
            thread receiveThread(&Client::receiveMessages, this);
            receiveThread.detach();

            string message;
            while (true) {
                getline(cin, message);

                if (expectingFile) {
                    if (message == "y" || message == "Y") {
                        NetworkUtils::sendMessage(clientSocket, ".a " + expectedFileName);
                        string path = cmdHandler.clientDirectory + "/" + expectedFileName;
                        cout << path << endl;
                        bFileReceivingState = true;
                        // cout << NetworkUtils::receiveFile(path, clientSocket) << endl;
                    }
                    else if (message == "n" || message == "N") {
                        NetworkUtils::sendMessage(clientSocket, ".r ");
                    }
                    expectingFile = false; // Reset the flag after handling the response
                }
                else {
                    cmdHandler.handleCommands(message);
                }
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