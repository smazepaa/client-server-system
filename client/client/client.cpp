#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include "NetworkUtils.h"

#pragma comment(lib, "ws2_32.lib")

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

    ConnectionManager(){
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

    void getFile(const string& line, const string& filename) {
        NetworkUtils::sendMessage(clientSocket, line);
        string resp = NetworkUtils::receiveMessage(clientSocket);

        if (resp.find("File does not exist") == string::npos) {

            // Only proceed to receive file if the file exists
            string outputPath = clientDirectory + "/" + filename;
            cout << NetworkUtils::receiveFile(outputPath, clientSocket) << endl;
            cout << NetworkUtils::receiveMessage(clientSocket) << endl;
            
        }
        else {
            cout << resp << endl << endl;
        }
    }

public:

    CommandHandler(SOCKET socket) : clientSocket(socket){}

    ~CommandHandler(){
        closesocket(clientSocket);
        WSACleanup();
    }

    void inputCommand() {
        vector<string> inputParams;

        string line;
        getline(cin, line);

        if (line != "") {

            istringstream iss(line);
            string word;
            while (iss >> word) {
                inputParams.push_back(word);
            }

            string command = inputParams[0];
            string filename;
            if (inputParams.size() == 2) {
                filename = inputParams[1];
            }

            if (command == "PUT") {

                string filePath = clientDirectory + "/" + filename;

                if (!exists(filePath)) {
                    cout << "File does not exist: " << filePath << endl << endl;
                }
                else {
                    NetworkUtils::sendMessage(clientSocket, line);
                    cout << NetworkUtils::sendFile(filePath, clientSocket) << endl;
                    cout << NetworkUtils::receiveMessage(clientSocket) << endl << endl;
                }
            }

            else if (command == "LIST" || command == "INFO" || command == "DELETE") {
                NetworkUtils::sendMessage(clientSocket, line);
                cout << NetworkUtils::receiveMessage(clientSocket) << endl << endl;
            }

            else if (command == "GET") {
                getFile(line, filename);
            }

            else {
                cout << "Invalid command" << endl << endl;
            }
        }
        else {
            cout << "Empty input" << endl << endl;
        }
    }

    bool isReady() const {
        return clientSocket != INVALID_SOCKET;
    }

    void sendClientName() {
        string name;
        while (name == "") {
            cout << "Enter client name: ";
            getline(cin, name);
            if (name != "") {
                cout << "Client name accepted: " << name << endl << endl;
                NetworkUtils::sendMessage(clientSocket, name);

                clientDirectory = baseDirectory + name;
                if (!fs::exists(clientDirectory)) {
                    create_directory(clientDirectory);
                }

                cout << "Enter commands: " << endl;
            }
            else {
                cout << "Cannot proceed with empty client name" << endl;
            }
        }

    }
};

class Client {
    ConnectionManager connManager;
    CommandHandler cmdHandler;

public:
    Client() : connManager(),
        cmdHandler(move(connManager.getClientSocket())) {
        if (connManager.isReady()) {
            connManager.connectToServer();
            cmdHandler.sendClientName();
        }
        else {
            throw runtime_error("Client is not ready for connections");
        }
    }


    void processCommands() {
        if (cmdHandler.isReady()) {
            cmdHandler.inputCommand();
        }
    }
};

int main() {
    try {
        Client client;
        while (true) {
            client.processCommands();
        }
    }
    catch (const runtime_error& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
    return 0;
}