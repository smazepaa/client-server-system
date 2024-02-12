#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <map>
#include <queue>
#include<condition_variable>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#include "NetworkUtils.h"

using namespace std;
namespace fs = filesystem;
using namespace fs;

mutex consoleMutex;
vector<SOCKET> clients;
map<int, vector<SOCKET>> roomsClients;

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
    string baseDirectory = "C:/Users/sofma/server-dir/";
    string clientName;
    int roomId;

    bool fileAccepted = false;
    string fileToSend;

    mutex messageQueueMutex;
    condition_variable messageAvailableCondition;
    queue<string> messageQueue;

    thread broadcastThread;
    bool broadcasting = true;

    void addMessageToQueue(const string& message) {
        lock_guard<mutex> lock(messageQueueMutex);
        messageQueue.push(message);
        messageAvailableCondition.notify_one();
    }

    void broadcastMessage(const string& message, SOCKET senderSocket) {
        lock_guard<mutex> lock(consoleMutex);
        for (SOCKET client : roomsClients[roomId]) {
            if (client != senderSocket) {
                NetworkUtils::sendMessage(client, message);
            }
        }
    }

    /*void broadcastFile(const string& filename, SOCKET& senderSocket) {
        string filePath = baseDirectory + filename;
        string message = ".i " + filename;
        for (SOCKET client : roomsClients[roomId]) {
            if (client != senderSocket) {
                NetworkUtils::sendMessage(client, filename);
                NetworkUtils::sendFile(filePath, client);
            }
        }
    }*/

    void broadcastFile(const string& filename, SOCKET& senderSocket, const string& message) {
        string filePath = baseDirectory + filename; // Ensure the file path is correct
        for (SOCKET client : roomsClients[roomId]) {
            if (client != senderSocket) {
                NetworkUtils::sendMessage(client, message);
                //string message = NetworkUtils::receiveMessage(client);
                /*if (message == ".y") {
                    NetworkUtils::sendFile(filePath, client);
                }
                else {
                    cout << "file declined" << endl;
                }*/
            }
        }
    }


    void processMessages() {
        while (broadcasting) {
            unique_lock<mutex> lock(messageQueueMutex);
            messageAvailableCondition.wait(lock, [this] {
                return !messageQueue.empty() || !broadcasting;
                });
            while (!messageQueue.empty()) {
                string message = messageQueue.front();
                messageQueue.pop();
                broadcastMessage(message, clientSocket);
            }
        }
    }

    void quitRoom() {
        lock_guard<mutex> lock(consoleMutex);

        auto& clients = roomsClients[roomId];
        auto it = find(clients.begin(), clients.end(), clientSocket);
        if (it != clients.end()) {
            clients.erase(it);
            
            //NetworkUtils::sendMessage(clientSocket, "You left the room");
            cout << clientName << " left ROOM " + to_string(roomId) << endl;
            string leaveMessage = clientName + " left the room";
            addMessageToQueue(leaveMessage);
        }
    }

public:

    CommandHandler() : clientSocket(INVALID_SOCKET) {}

    CommandHandler(SOCKET& client) : clientSocket(client) {
        string clientInfo = NetworkUtils::receiveMessage(clientSocket);
        size_t delimiterPos = clientInfo.find(';');
        clientName = clientInfo.substr(0, delimiterPos);
        roomId = stoi(clientInfo.substr(delimiterPos + 1));

        cout << "Accepted connection from " << clientName << endl;

        broadcastThread = thread(&CommandHandler::processMessages, this);
    }

    ~CommandHandler() {
        broadcasting = false;
        messageAvailableCondition.notify_all();
        if (broadcastThread.joinable()) {
            broadcastThread.join();
        }
        closesocket(clientSocket);
    }

    bool isReady() {
        return this->clientSocket != INVALID_SOCKET;
    }

    void handleClient() {
        roomsClients[roomId].push_back(clientSocket);
        string connectionMessage = clientName + " joined ROOM " + to_string(roomId);
        cout << connectionMessage << endl;
        addMessageToQueue(connectionMessage);

        while (broadcasting) {
            string message = NetworkUtils::receiveMessage(clientSocket);
            if (message == "") {
                lock_guard<mutex> lock(consoleMutex);
                cout << clientName << " disconnected.\n";
                string message = clientName + " left the room";
                addMessageToQueue(message);
                auto& clients = roomsClients[roomId];
                auto it = find(clients.begin(), clients.end(), clientSocket);
                if (it != clients.end()) {
                    clients.erase(it);
                }

                closesocket(clientSocket);
                clientSocket = INVALID_SOCKET;

                return;
            }

            else if (message == ".q") {
                quitRoom();
            }

            else if (message._Starts_with(".j ")) {
                string roomIdStr = message.substr(3);
                roomId = stoi(roomIdStr);
                // Join new room

                lock_guard<mutex> lock(consoleMutex);
                roomsClients[roomId].push_back(clientSocket);
                string connectionMessage = clientName + " joined ROOM " + to_string(roomId);
                cout << connectionMessage << endl;
                addMessageToQueue(connectionMessage);

                string confirmation = "Joined room " + roomIdStr;
                NetworkUtils::sendMessage(clientSocket, confirmation);
            }

            else if (message._Starts_with(".f")) {
                fileToSend = message.substr(3);
                if (!fileToSend.empty()) {
                    string filePath = baseDirectory + fileToSend;
                    cout << NetworkUtils::receiveFile(filePath, clientSocket) << endl;
                    broadcastFile(fileToSend, clientSocket, message); // Broadcast the file
                }
            }

            else if (message == ".y") {
                cout << "file accepted by " << clientName << endl;
                string filePath = baseDirectory + fileToSend;

                NetworkUtils::sendFile(filePath, clientSocket);
            }

            else {
                string fullMessage = clientName + ": " + message;
                addMessageToQueue(fullMessage);
            }
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
            SOCKET clientSocket = connManager.acceptClient();
            if (clientSocket != INVALID_SOCKET) {
                lock_guard<mutex> lock(consoleMutex);
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