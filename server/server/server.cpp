#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <map>
#include <queue>
#include<condition_variable>
#include <winsock2.h>
#include "NetworkUtils.h"
#pragma comment(lib, "ws2_32.lib")


using namespace std;
namespace fs = filesystem;
using namespace fs;

mutex consoleMutex, fileSendMutex;
vector<SOCKET> clients;
map<int, vector<SOCKET>> roomsClients;

condition_variable allFilesReceivedCondition;
int expectedAcknowledgements = 0;
int receivedAcknowledgements = 0;

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

    mutex messageQueueMutex, roomManagementMutex;
    condition_variable messageAvailableCondition;
    queue<string> messageQueue;

    thread broadcastThread;
    bool broadcasting = true;

    void addMessageToQueue(const string& message) {
        lock_guard<mutex> lock(messageQueueMutex);
        messageQueue.push(message);
        messageAvailableCondition.notify_one();
    }

    void broadcastMessage(const string& message, SOCKET& senderSocket) {
        lock_guard<mutex> lock(consoleMutex);
        for (SOCKET client : roomsClients[roomId]) {
            if (client != senderSocket) {
                NetworkUtils::sendMessage(client, message);
            }
        }
    }

    void broadcastFileName(const string& filename, SOCKET& senderSocket) {
        string filePath = baseDirectory + filename;
        {
            lock_guard<mutex> lock(fileSendMutex);
            expectedAcknowledgements = roomsClients[roomId].size() - 1;
            receivedAcknowledgements = 0;
            cout << expectedAcknowledgements << endl;
        }

        //for (SOCKET client : roomsClients[roomId]) {
        //    if (client != senderSocket) {
        //        string notification = clientName + ": sending the file " + filename + ". Accept? [y/n]";
        //        NetworkUtils::sendMessage(client, notification);
        //        //NetworkUtils::sendMessage(client, message);
        //        //NetworkUtils::sendFile(filePath, client);
        //    }
        //}

        string notification = clientName + ": sending the file " + filename + ". Accept? [y/n]";
        addMessageToQueue(notification);

        unique_lock<mutex> lock(fileSendMutex);
        allFilesReceivedCondition.wait(lock, [this]() {
            return expectedAcknowledgements == receivedAcknowledgements;
            });

        cout << "Everyone received the file." << endl;
        NetworkUtils::sendMessage(senderSocket, "Everyone received the file.");
        remove(filePath);
    }

    void processMessages() {
        while (broadcasting) {
            unique_lock<mutex> lock(messageQueueMutex);
            messageAvailableCondition.wait(lock, [this] {
                return !messageQueue.empty() || !broadcasting; });

            while (!messageQueue.empty()) {
                string message = messageQueue.front();
                messageQueue.pop();
                broadcastMessage(message, clientSocket);
            }
        }
    }

    void quitRoom() {
        lock_guard<mutex> lock(roomManagementMutex);
        auto& clients = roomsClients[roomId];

        auto it = find(clients.begin(), clients.end(), clientSocket);
        if (it != clients.end()) {
            cout << clientName << " left ROOM " + to_string(roomId) << endl;
            clients.erase(it);
            string leaveMessage = clientName + " left the room";
            addMessageToQueue(leaveMessage);

            string confirmation = "You left ROOM " + to_string(roomId);
            NetworkUtils::sendMessage(clientSocket, confirmation);
        }
    }

    void joinRoom(const string& message) {
        string roomIdStr = message.substr(3);
        int newRoomId = stoi(roomIdStr);

        roomId = newRoomId;

        lock_guard<mutex> lock(roomManagementMutex);
        roomsClients[roomId].push_back(clientSocket);
        string connectionMessage = clientName + " joined ROOM " + to_string(roomId);
        cout << connectionMessage << endl;
        addMessageToQueue(connectionMessage);

        string confirmation = "You joined ROOM " + roomIdStr;
        NetworkUtils::sendMessage(clientSocket, confirmation);
    }

    void sendFile(const string& message) {
        string filename = message.substr(3);
        if (!filename.empty()) {
            string filePath = baseDirectory + filename;
            cout << NetworkUtils::receiveFile(filePath, clientSocket) << endl;
            broadcastFileName(filename, clientSocket);
        }
    }

    void handleDisconnect() {
        lock_guard<mutex> lock(consoleMutex);
        quitRoom();
        cout << clientName << " disconnected.\n";

        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
    }

    void ackReceive() {
        lock_guard<mutex> lock(fileSendMutex);
        receivedAcknowledgements++;
        cout << "received" << receivedAcknowledgements << endl;
        if (receivedAcknowledgements == expectedAcknowledgements) {
            allFilesReceivedCondition.notify_one();
        }
    }

public:

    CommandHandler() : clientSocket(INVALID_SOCKET) {}

    CommandHandler(SOCKET& client) : clientSocket(client) {

        clientName = NetworkUtils::receiveMessage(clientSocket);
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
        while (broadcasting) {
            string message = NetworkUtils::receiveMessage(clientSocket);
            if (message == "") {
                handleDisconnect();
                return;
            }

            else if (message == ".q") {
                quitRoom();
            }

            else if (message._Starts_with(".j ")) {
                joinRoom(message);
            }

            else if (message._Starts_with(".f")) {
                sendFile(message);
            }

            else if (message == ".a") {
                ackReceive();
            }

            else if (message._Starts_with(".y ")) {
                string filename = message.substr(3);
                string path = baseDirectory + "/" + filename;

                if (fs::exists(path)) {
                    cout << "Sending file to client: " << filename << endl;
                    NetworkUtils::sendMessage(clientSocket, "Receiving file from the server");
                    string response = NetworkUtils::sendFile(path, clientSocket);
                    cout << response << endl;
                }
                else {
                    cerr << "File does not exist: " << filename << endl;
                }
            }

            else if (message == ".n ") {
                cout << clientName << " rejected the file." << endl;
                lock_guard<mutex> lock(fileSendMutex);
                expectedAcknowledgements--;
                cout << "expected" << expectedAcknowledgements << endl;
                if (receivedAcknowledgements == expectedAcknowledgements) {
                    allFilesReceivedCondition.notify_one();
                }
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