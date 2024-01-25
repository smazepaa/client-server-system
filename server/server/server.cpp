#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <WinSock2.h>
#include <Ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;
namespace fs = std::filesystem;
using namespace fs;

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

    void sendResponse(const string& response) {
        string dataToSend = response + "<END>"; // end marker
        const size_t bufferSize = 1024;
        size_t dataLength = dataToSend.length();
        size_t sent = 0;

        while (sent < dataLength) {
            size_t toSend = min(bufferSize, dataLength - sent);
            send(clientSocket, dataToSend.c_str() + sent, toSend, 0);
            sent += toSend;
        }
    }

    string receiveMessage() {
        string totalData;
        char buffer[1024];
        const string endMarker = "<END>";

        while (true) {
            memset(buffer, 0, 1024);
            int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);

            if (bytesReceived > 0) {
                totalData.append(buffer, bytesReceived);
                size_t found = totalData.find(endMarker);
                if (found != string::npos) {
                    totalData.erase(found, endMarker.length());
                    break;
                }
            }
            else {
                return "";
            }
        }
        return totalData;
    }

    void handleCommands(const string& command, const string& filename) {
        if (command == "LIST") {
            listFiles();
        }

        else if (command == "GET") {
            sendFile(filename);
        }

        else if (command == "PUT") {
            receiveFile(filename);
        }

        else if (command == "INFO") {
            fileInfo(filename);
        }

        else if (command == "DELETE") {
            deleteFile(filename);
        }

        else {
            cout << clientName << " -> " << "Invalid command" << endl;
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
        string fileList = "Files in directory: " + serverDirectory + "\n";

        if (fs::is_empty(serverDirectory)) {
            fileList = serverDirectory + " is empty.\n";
        }
        else {
            listDirectory(serverDirectory, fileList);
        }

        sendResponse(fileList.c_str());
        cout << clientName << " <- " << "List sent to the client" << endl;
    }

    void fileInfo(const string& filename) {
        string filePath = serverDirectory + "/" + filename;

        if (!exists(filePath)) {
            cout << clientName << " <- " << "File does not exist: " << filePath << endl;
            sendResponse("File does not exist \n");
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

        sendResponse(response);
        cout << clientName << " <- " << "Information sent to the client." << endl;
    }

    void deleteFile(const string& filename) {
        string filePath = serverDirectory + "/" + filename;
        string fileType = is_directory(filePath) ? "Folder" : "File";

        if (!exists(filePath)) {
            cout << clientName << " <- " << fileType << " does not exist: " << filePath << endl;
            sendResponse(fileType + " does not exist on the server.\n");
            return;
        }

        if (remove(filePath)) {
            cout << clientName << " <- " << fileType << " was successfully deleted." << endl;
            sendResponse(fileType + " was successfully deleted.\n");
        }

        else {
            cout << clientName << " <- " << "Failed to delete: " << filePath << endl;
            sendResponse("Failed to delete " + filename);
        }
    }

    void sendFile(const string& filename) {
        string filePath = serverDirectory + "/" + filename;

        if (!exists(filePath)) {
            string response = "File does not exist: " + filePath;
            cout << clientName << " <- " << response << endl;
            sendResponse(response);
            return;
        }

        ifstream file(filePath, ios::binary);
        if (!file.is_open()) {
            cout << clientName << " <- " << "Failed to open file" << endl;
            sendResponse("Failed to open file\n");
            return;
        }

        sendResponse("file is present");

        const size_t bufferSize = 1024;
        char buffer[bufferSize];

        while (file.read(buffer, bufferSize) || file.gcount()) {
            send(clientSocket, buffer, file.gcount(), 0); // Send content by chunks
        }

        file.close();

        const char* eofMarker = "<EOF>";
        send(clientSocket, eofMarker, strlen(eofMarker), 0); // Send EOF marker
        sendResponse("File transfer completed\n");

        cout << clientName << " <- " << "File transfer completed" << endl;
        cout << clientName << " -> " << receiveMessage() << endl;
    }

    void receiveFile(const string& filename) {

        string outputFilePath = serverDirectory + "/" + filename;

        if (exists(outputFilePath)) {
            string response = "File already exists: " + outputFilePath;
            cout << clientName << " <- " << response << endl;
            sendResponse(response);
            return;
        }

        ofstream outputFile(outputFilePath, ios::binary);
        if (!outputFile.is_open()) {
            string response = "Failed to open file for writing: " + outputFilePath;
            cout << clientName << " <- " << response << endl;
            sendResponse(response);
            return;
        }

        const size_t bufferSize = 1024;
        char buffer[bufferSize];
        string eofMarker = "<EOF>";
        string fileData;

        while (true) {
            memset(buffer, 0, bufferSize);
            int bytesReceived = recv(clientSocket, buffer, bufferSize, 0);

            if (bytesReceived <= 0) {
                cout << clientName << " <- " << "Failed to receive data or connection closed by client." << endl;
                const char* response = "File transfer failed";
                sendResponse(response);
                break;
            }

            fileData.append(buffer, bytesReceived);

            size_t eofPos = fileData.find(eofMarker); // check for the end of file
            if (eofPos != string::npos) {
                outputFile.write(fileData.c_str(), eofPos); // remove end-of-file marker
                break;
            }

            // Write to file if buffer is full and EOF marker not found
            if (fileData.size() >= bufferSize) {
                outputFile.write(fileData.c_str(), fileData.size() - eofMarker.size());
                fileData.erase(0, fileData.size() - eofMarker.size());
            }
        }

        outputFile.close();
        cout << clientName << " <- " << "File transfer completed and saved to: " << outputFilePath << endl;
        const char* response = "File transfer completed";
        sendResponse(response);
    }

public:

    CommandHandler() : clientSocket(INVALID_SOCKET){}

    CommandHandler(SOCKET& client) : clientSocket(client){
        clientName = receiveMessage();
        serverDirectory = baseDirectory + clientName;

        cout << "Accepted connection from " << clientName << endl;
        cout << "Working directory: " << serverDirectory << endl;

        if (!fs::exists(serverDirectory)) {
            create_directory(serverDirectory);
        }
    }

    void receiveCommands() {
        string received = receiveMessage();
        if (received.empty()) {
            cout << clientName << " disconnected." << endl;
            closesocket(clientSocket);
            clientSocket = INVALID_SOCKET; // Update socket status
            return; // Exit the method
        }

        cout << clientName << " -> " << received << endl;

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
            std::cout << "Waiting for a new client..." << std::endl;
            SOCKET clientSocket = connManager.acceptClient();
            if (clientSocket != INVALID_SOCKET) {
                // Use a lambda to call the member function
                clientThreads.emplace_back([this, clientSocket]() {
                    this->handleClient(clientSocket);
                });
            }
            else {
                std::cerr << "Failed to accept a client." << std::endl;
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

