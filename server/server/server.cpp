#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <WinSock2.h>
#include <Ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;
namespace fs = std::filesystem;
using namespace fs;

class Server {

    SOCKET serverSocket;
    SOCKET clientSocket;
    sockaddr_in serverAddr;
    int port = 12345;
    char buffer[1024]; // for storing the message from client
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
        }

        else if (command == "GET") {

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
            cout << "Invalid command" << endl;
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
            fileList = "Server directory is empty.\n";
        }
        else {
            listDirectory(serverDirectory, fileList);
        }

        sendResponse(fileList.c_str());
        cout << "Request completed" << endl;
    }

    void fileInfo(const string& filename) {
        string filePath = serverDirectory + "/" + filename;

        if (!exists(filePath)) {
            cout << "File does not exist: " << filePath << endl;
            sendResponse("File does not exist \n");
            return;
        }

        auto fsize = file_size(filePath);
        auto ftime = last_write_time(filePath);
        auto ftype = is_directory(filePath) ? "File Folder" : "File";

        // Convert file_time_type to time_t
        auto sctp = chrono::time_point_cast<chrono::system_clock::duration>(
            ftime - file_time_type::clock::now() + chrono::system_clock::now());
        time_t cftime = chrono::system_clock::to_time_t(sctp);

        // Convert time_t to tm for formatting
        tm* timeinfo = localtime(&cftime);

        // Format the time to a string
        char timeStr[80];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);

        string response = "Name: " + filename + "\nSize: " + to_string(fsize) + " bytes\nType: "
            + ftype + "\nLast Modified: " + timeStr;

        sendResponse(response.c_str());
    }

    void communicateWithClient() {

        bool received = receiveMessage();
        if (received) {
            cout << "Received data: " << buffer << endl;

            const char* response = "Hello, client! This is the server.";
            sendResponse(response);
        }
    }

    void deleteFile(const string& filename) {
        string filePath = serverDirectory + "/" + filename;

        if (!exists(filePath)) {
            cout << "File does not exist: " << filePath << endl;
            sendResponse("File does not exist.\n");
            return;
        }

        if (remove(filePath)) {
            cout << "File deleted successfully: " << filePath << endl;
            sendResponse("File deleted successfully.\n");
        }

        else {
            cout << "Failed to delete file: " << filePath << endl;
            sendResponse("Failed to delete file.\n");
        }
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

    void recieveCommands() {
        bool received = receiveMessage();
        if (received) {

            vector<string> params;
            istringstream iss(buffer);
            string word;
            while (iss >> word) {
                params.push_back(word);
            }

            if (params.empty()) {
                cout << "No command received." << endl;
                return;
            }

            string command = params[0];
            string filename = (params.size() > 1) ? params[1] : "";

            handleCommands(command, filename);
        }
    }

    void receiveFile(const string& filename) {

        string outputFilePath = serverDirectory + "/" + filename;

        if (exists(outputFilePath)) {
            cout << "File already exists: " << outputFilePath << endl;
            const char* response = "File already exists";
            sendResponse(response);
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
        string fileData;

        while (true) {
            memset(buffer, 0, bufferSize);
            int bytesReceived = recv(clientSocket, buffer, bufferSize, 0);

            if (bytesReceived <= 0) {
                cout << "Failed to receive data or connection closed by client." << endl;
                const char* response = "File transfer failed";
                sendResponse(response);
                break;
            }

            fileData.append(buffer, bytesReceived);

            // Check for EOF marker
            size_t eofPos = fileData.find(eofMarker);
            if (eofPos != string::npos) {
                // Write data to file excluding EOF marker
                outputFile.write(fileData.c_str(), eofPos);
                break;
            }

            // Write to file if buffer is full and EOF marker not found
            if (fileData.size() >= bufferSize) {
                outputFile.write(fileData.c_str(), fileData.size() - eofMarker.size());
                fileData.erase(0, fileData.size() - eofMarker.size());
            }
        }

        outputFile.close();
        cout << "File transfer completed and saved to: " << outputFilePath << endl;
        const char* response = "File transfer completed";
        sendResponse(response);
    }

};

int main() {
    
    Server server;
    while (true) {
        server.recieveCommands();
    }

    return 0;
}
