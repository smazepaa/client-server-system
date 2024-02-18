#include <iostream>
#include <thread>
#include <string>
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <condition_variable>
#include <mutex>
#pragma comment(lib, "ws2_32.lib")
#include "NetworkUtils.h"

using namespace std;
namespace fs = std::filesystem;
using namespace fs;

atomic<bool> bFileReceivingState = false;
// condition_variable waitForReceival;

mutex cvMutex; // Mutex for condition variable synchronization
condition_variable cv; // Condition variable for file reception state
bool allReceived = false;

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
    
    bool inRoom = false;

    string name, roomId;

    void quitRoom() {
        string confirmation;
        cout << "Do you want to leave the room? [y/n] ";
        getline(cin, confirmation);
        cout << "\033[A\033[2K";

        if (confirmation == "y") {
            NetworkUtils::sendMessage(clientSocket, ".q");
            inRoom = false;
        }
        else if (confirmation == "n") {
            cout << "Remaining in the same room." << endl;
        }
        else {
            cout << "Invalid response." << endl;
        }
    }

    void joinRoom(const string& newRoomId) {
        if (newRoomId != "") {
            NetworkUtils::sendMessage(clientSocket, ".j " + newRoomId);
            inRoom = true;
        }
        else {
            cout << "Cannot proceed without the room id." << endl;
        }
    }

    void sendFile(const string& message, const string& filename) {
        if (!filename.empty()) {
            cout << "You: sending the file " << filename << endl;
            NetworkUtils::sendMessage(clientSocket, message);
            string filePath = clientDirectory + "/" + filename;
            NetworkUtils::sendFile(filePath, clientSocket);
        }
        else {
            cout << "Cannot proceed without the filename." << endl;
        }
    }

    void sendMessage(stringstream& ss) {
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
        while (name == "") {
            cout << "Enter client name: ";
            getline(cin, name);
            if (name != "") {
                cout << endl;

                NetworkUtils::sendMessage(clientSocket, name);
             
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
            sendMessage(ss);
        }

        else if (command == ".f") {
            string filename;
            getline(ss, filename);
            sendFile(message, filename);
        }

        else if (command == ".q") {
            quitRoom();
        }

        else if (command == ".j") {
            string newRoomId;
            getline(ss, newRoomId);

            if (inRoom) {
                quitRoom();
            }
            joinRoom(newRoomId);
        }

        else {
            cout << endl;
            cout << "Unknown command or message not prefixed correctly. Please use\n" <<
                "-> .m <message> to send a message\n" << "-> .f <filename> to send a file\n" <<
                "-> .q to leave the room\n" << "-> .j <id> to join a room" << endl;
            cout << endl;
        }
    }

    void receiveFile(const string& filename) {
        string filePath = clientDirectory + "/" + filename;
        string status = NetworkUtils::receiveFile(filePath, clientSocket);
        cout << status << endl;
        NetworkUtils::sendMessage(clientSocket, ".a");
    }
};

class Client {
    ConnectionManager connManager;
    CommandHandler cmdHandler;
    SOCKET clientSocket;

    thread receiveThread;

    bool expectingFile = false;
    string expectedFileName;


    string extractFileNameFromNotice(const string& message) {
        size_t startPos = message.find(": sending the file ") + strlen(": sending the file ");
        size_t endPos = message.find(". Accept? [y/n]");
        if (startPos != string::npos && endPos != string::npos) {
            return message.substr(startPos, endPos - startPos);
        }
        return "";
    }

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
            if (bFileReceivingState) {
                //NetworkUtils::sendMessage(clientSocket, ".y " + expectedFileName);
                string path = cmdHandler.clientDirectory + "/" + expectedFileName;
                bFileReceivingState = false;
                cout << NetworkUtils::receiveFile(path, clientSocket) << endl;
                NetworkUtils::sendMessage(clientSocket, ".a");
                expectingFile = false;
            }
            else {
                //string message = NetworkUtils::receiveMessage(clientSocket);
                if (message == "") {
                    cerr << "Server disconnected.\n";
                    break;
                }

                else if (message.ends_with(". Accept? [y/n]")) {
                    expectedFileName = extractFileNameFromNotice(message);
                    cout << message << endl;
                    expectingFile = true;
                }

                else if (message == "File received by clients, who accepted") {
                    cout << message << endl;
                    unique_lock<std::mutex> lk(cvMutex);
                    allReceived = true;
                    lk.unlock();
                    cv.notify_one();
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
                    if (message == "y") {
                        NetworkUtils::sendMessage(clientSocket, ".y " + expectedFileName);
                        bFileReceivingState = true;
                    }
                    else if (message == "n") {
                        NetworkUtils::sendMessage(clientSocket, ".n");
                    }
                    expectingFile = false;
                }

                else if (message.starts_with(".f ")) {
                    cmdHandler.handleCommands(message);

                    unique_lock<mutex> lk(cvMutex);
                    cv.wait(lk, [this]() {
                        if (!allReceived) {
                            cout << "Wait for users to receive the file to proceed" << endl;
                        }
                        return allReceived;
                    });
                    allReceived = false;
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