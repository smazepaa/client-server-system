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
    string serverDirectory = "C:/Users/sofma/server-dir";

    // methods for setting up the connection
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

    void cleanup() {
        closesocket(serverSocket);
        WSACleanup();
    }

    // commands methods
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
        char buffer[1024]; // for storing the message from client
        memset(buffer, 0, 1024);
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived > 0) {
            return string(buffer);
        }
        return "";
    }

    void handleCommands(const string& command, const string& filename) {
        if (command == "LIST") {
            listFiles();
        }

        else if (command == "GET") {
            sendFile(filename);
            cout << receiveMessage() << endl;
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

    void deleteFile(const string& filename) {
        string filePath = serverDirectory + "/" + filename;
        string fileType = is_directory(filePath) ? "Folder" : "File";

        if (!exists(filePath)) {
            cout << fileType << " does not exist: " << filePath << endl;
            sendResponse(fileType + " does not exist on the server.\n");
            return;
        }

        if (remove(filePath)) {
            cout << fileType << " was successfully deleted." << endl;
            sendResponse(fileType + " was successfully deleted.\n");
        }

        else {
            cout << "Failed to delete: " << filePath << endl;
            sendResponse("Failed to delete " + filename);
        }
    }

    void sendFile(const string& filename) {
        string filePath = serverDirectory + "/" + filename;

        // Check if the file exists
        if (!exists(filePath)) {
            cout << "File does not exist: " << filePath << endl;
            sendResponse("No such file on the server\n");
            return;
        }

        ifstream file(filePath, ios::binary);
        if (!file.is_open()) {
            cout << "Failed to open file" << endl;
            sendResponse("Failed to open file\n");
            return;
        }

        const size_t bufferSize = 1024;
        char buffer[bufferSize];

        while (file.read(buffer, bufferSize) || file.gcount()) {
            send(clientSocket, buffer, file.gcount(), 0); // Send content by chunks
        }

        file.close();

        // Send EOF marker directly
        const char* eofMarker = "<EOF>";
        send(clientSocket, eofMarker, strlen(eofMarker), 0); // Send EOF marker
        cout << receiveMessage() << endl;
        sendResponse("File transfer completed\n");
    }

    void receiveFile(const string& filename) {

        string outputFilePath = serverDirectory + "/" + filename;

        if (exists(outputFilePath)) {
            string response = "File already exists: " + outputFilePath;
            cout << response << endl;
            sendResponse(response);
            return;
        }

        ofstream outputFile(outputFilePath, ios::binary);
        if (!outputFile.is_open()) {
            string response = "Failed to open file for writing: " + outputFilePath;
            cout << response << endl;
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
                cout << "Failed to receive data or connection closed by client." << endl;
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
        cout << "File transfer completed and saved to: " << outputFilePath << endl;
        const char* response = "File transfer completed";
        sendResponse(response);
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
        string received = receiveMessage();
        if (received != "") {

            vector<string> params;
            istringstream iss(received);
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

};

int main() {
    
    Server server;
    while (true) {
        server.recieveCommands();
    }

    return 0;
}
