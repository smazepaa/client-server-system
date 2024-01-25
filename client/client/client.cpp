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

    void sendFile(const string& filename) {

        string filePath = clientDirectory + "/" + filename;

        ifstream file(filePath, ios::binary);
        if (!file.is_open()) {
            cout << "Failed to open file" << endl;
            return;
        }

        const size_t bufferSize = 1024;
        char buffer[bufferSize];

        while (file.read(buffer, bufferSize) || file.gcount()) {
            send(clientSocket, buffer, file.gcount(), 0); // send content by chunks
        }

        file.close();

        const char* eofMarker = "<EOF>";
        NetworkUtils::sendMessage(clientSocket, eofMarker); // send end-of-file marker
    }

    void receiveFile(const string& filename) {
        string outputFilePath = clientDirectory + "/" + filename;

        if (exists(outputFilePath)) {
            string response = "File already exists: " + outputFilePath;
            cout << response << endl;
            NetworkUtils::sendMessage(clientSocket, response);
            return;
        }

        ofstream outputFile(outputFilePath, ios::binary);
        if (!outputFile.is_open()) {
            cout << "Failed to open file for writing: " << outputFilePath << endl;
            return;
        }

        const size_t bufferSize = 1024;
        char buffer[bufferSize];
        string eofMarker = "<EOF>";
        bool eofFound = false;

        while (!eofFound) {
            memset(buffer, 0, bufferSize);
            int bytesReceived = recv(clientSocket, buffer, bufferSize, 0);

            if (bytesReceived <= 0) {
                const char* response = "File transfer failed";
                cout << response << endl;
                NetworkUtils::sendMessage(clientSocket, response);
                return;
            }

            // Check for EOF marker and write only up to the marker
            for (int i = 0; i < bytesReceived; ++i) {
                if (string(&buffer[i], &buffer[min(i + eofMarker.length(), static_cast<size_t>(bytesReceived))]) == eofMarker) {
                    // Write data up to the EOF marker
                    outputFile.write(buffer, i);
                    eofFound = true;
                    break;
                }
            }

            if (!eofFound) {
                // Write the entire buffer to file as EOF marker not found
                outputFile.write(buffer, bytesReceived);
            }
        }

        outputFile.close();
        const char* response = "File transfer completed";
        NetworkUtils::sendMessage(clientSocket, response);
        cout << response << endl << endl;
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
                    sendFile(filename);
                    cout << NetworkUtils::receiveMessage(clientSocket) << endl;
                }
            }

            else if (command == "LIST" || command == "INFO" || command == "DELETE") {
                NetworkUtils::sendMessage(clientSocket, line);
                cout << NetworkUtils::receiveMessage(clientSocket) << endl << endl;
            }

            else if (command == "GET") {
                NetworkUtils::sendMessage(clientSocket, line);
                string resp = NetworkUtils::receiveMessage(clientSocket);
                if (resp.find("File does not exist") == string::npos) {
                    // Only proceed to receive file if the file exists
                    receiveFile(filename);
                }
                else {
                    cout << resp << endl << endl;
                }
            }
            else {
                cout << "Invalid command" << endl;
            }
        }
        else {
            cout << "Empty input\n" << endl;
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