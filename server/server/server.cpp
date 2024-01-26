#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <mutex>
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include "NetworkUtils.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;
namespace fs = filesystem;
using namespace fs;
mutex m;

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
    string serverDirectory;
    string baseDirectory = "C:/Users/sofma/server-dir/";
    string clientName;

    void handleCommands(const string& command, const string& filename) {

        string path = serverDirectory + "/" + filename;

        if (command == "LIST") {
            listFiles();
        }

        else if (command == "GET") {
            string response = NetworkUtils::sendFile(path, clientSocket);
            m.lock();
            cout << clientName << " <- " << response << endl;
            m.unlock();
            NetworkUtils::sendMessage(clientSocket, response);

        }

        else if (command == "PUT") {
            string response = NetworkUtils::receiveFile(path, clientSocket);
            m.lock();
            cout << clientName << " <- " << response << endl;
            m.unlock();
            NetworkUtils::sendMessage(clientSocket, response);
        }

        else if (command == "INFO") {
            fileInfo(filename);
        }

        else if (command == "DELETE") {
            deleteFile(filename);
        }
    }

    void listDirectory(const path& path, string& fileList, string indent = "") {
        for (const auto& entry : directory_iterator(path)) {
            if (is_directory(entry.status())) {
                fileList += indent + "[Folder] " + entry.path().filename().string() + "\n";
                // Recursively list the contents of the subdirectory
                listDirectory(entry.path(), fileList, indent + "  ");
            }
            else {
                fileList += indent + "[File] " + entry.path().filename().string() + "\n";
            }
        }
    }

    void listFiles() {
        m.lock();
        string fileList = "Files in directory: " + serverDirectory + "\n";

        if (fs::is_empty(serverDirectory)) {
            fileList = serverDirectory + " is empty.\n";
        }
        else {
            listDirectory(serverDirectory, fileList);
        }

        NetworkUtils::sendMessage(clientSocket, fileList.c_str());
        cout << clientName << " <- " << "List sent to the client" << endl;
        m.unlock();
    }

    void fileInfo(const string& filename) {
        m.lock();
        string filePath = serverDirectory + "/" + filename;

        if (!exists(filePath)) {
            cout << clientName << " <- " << "File does not exist: " << filePath << endl;
            NetworkUtils::sendMessage(clientSocket, "File does not exist \n");
            return;
        }

        auto ftype = is_directory(filePath) ? "File Folder" : "File";
        size_t totalSize = 0;

        if (ftype == "File Folder") {
            for (const auto& entry : recursive_directory_iterator(filePath)) {
                if (!is_directory(entry)) {
                    totalSize += file_size(entry.path());
                }
            }
        }
        else {
            totalSize = file_size(filePath);
        }

        m.unlock();
        // Convert size to human-readable format
        string readableSize;
        if (totalSize < 1024) {
            readableSize = to_string(totalSize) + " bytes";
        }
        else if (totalSize < 1024 * 1024) {
            readableSize = to_string(totalSize / 1024) + " KB";
        }
        else if (totalSize < 1024 * 1024 * 1024) {
            readableSize = to_string(totalSize / (1024 * 1024)) + " MB";
        }
        else {
            readableSize = to_string(totalSize / (1024 * 1024 * 1024)) + " GB";
        }

        auto ftime = last_write_time(filePath);
        auto sctp = chrono::time_point_cast<chrono::system_clock::duration>(
            ftime - file_time_type::clock::now() + chrono::system_clock::now());
        time_t cftime = chrono::system_clock::to_time_t(sctp);
        tm* timeinfo = localtime(&cftime);
        char timeStr[80];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);

        string response = "Name: " + filename + "\nSize: " + readableSize + "\nType: "
            + ftype + "\nLast Modified: " + timeStr;

        NetworkUtils::sendMessage(clientSocket, response);
        m.lock();
        cout << clientName << " <- " << "Information sent to the client." << endl;
        m.unlock();
    }

    void deleteFile(const string& filename) {
        m.lock();
        string filePath = serverDirectory + "/" + filename;
        string fileType = is_directory(filePath) ? "Folder" : "File";

        if (!exists(filePath)) {
            cout << clientName << " <- " << fileType << " does not exist: " << filePath << endl;
            NetworkUtils::sendMessage(clientSocket, fileType + " does not exist on the server.\n");
            return;
        }

        if (remove(filePath)) {
            cout << clientName << " <- " << fileType << " was successfully deleted." << endl;
            NetworkUtils::sendMessage(clientSocket, fileType + " was successfully deleted.\n");
        }

        else {
            cout << clientName << " <- " << "Failed to delete: " << filePath << endl;
            NetworkUtils::sendMessage(clientSocket, "Failed to delete " + filename);
        }
        m.unlock();
    }


public:

    CommandHandler() : clientSocket(INVALID_SOCKET){}

    CommandHandler(SOCKET& client) : clientSocket(client){
        clientName = NetworkUtils::receiveMessage(clientSocket);
        serverDirectory = baseDirectory + clientName;

        cout << "Accepted connection from " << clientName << endl;
        cout << "Working directory: " << serverDirectory << endl;

        if (!fs::exists(serverDirectory)) {
            create_directory(serverDirectory);
        }
    }

    void receiveCommands() {
        string received = NetworkUtils::receiveMessage(clientSocket);
        if (received.empty()) {
            m.lock();
            cout << clientName << " disconnected." << endl;
            m.unlock();
            closesocket(clientSocket);
            clientSocket = INVALID_SOCKET; // Update socket status
            return; // Exit the method
        }

        m.lock();
        cout << clientName << " -> " << received << endl;
        m.unlock();

        vector<string> params;
        istringstream iss(received);
        string word;
        while (iss >> word) {
            params.push_back(word);
        }

        string command = params[0];
        string filename = (params.size() > 1) ? params[1] : "";

        handleCommands(command, filename);
    }

    bool isReady() {
        return this->clientSocket != INVALID_SOCKET;
    }
};

class Server {
    ConnectionManager connManager;
    vector<thread> clientThreads;
    CommandHandler cmdHandler;

    void handleClient(SOCKET clientSocket) {

        CommandHandler cmdHandler(clientSocket);
        while (cmdHandler.isReady()) {
            cmdHandler.receiveCommands();
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
            cout << "Waiting for a new client..." << endl;
            SOCKET clientSocket = connManager.acceptClient();
            if (clientSocket != INVALID_SOCKET) {
                // Use a lambda to call the member function
                clientThreads.emplace_back([this, clientSocket]() {
                    this->handleClient(clientSocket);
                });
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

