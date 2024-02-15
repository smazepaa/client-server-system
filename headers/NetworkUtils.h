#pragma once

#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <WinSock2.h>
#include <Ws2tcpip.h>

using namespace std;
namespace fs = std::filesystem;
using namespace fs;

class NetworkUtils {

public:
    static void sendMessage(SOCKET& clientSocket, const string& message) {
        int messageLength = static_cast<int>(message.length());
        send(clientSocket, reinterpret_cast<char*>(&messageLength), sizeof(messageLength), 0);
        send(clientSocket, message.c_str(), messageLength, 0);
    }

    static string receiveMessage(SOCKET& clientSocket) {
        int messageLength = 0;
        recv(clientSocket, reinterpret_cast<char*>(&messageLength), sizeof(messageLength), 0);

        if (messageLength <= 0) return "";

        string message(messageLength, '\0');
        recv(clientSocket, &message[0], messageLength, 0);
        return message;
    }

    static string receiveFile(const string& basePath, SOCKET& clientSocket) {
        int fileSize = 0;
        recv(clientSocket, reinterpret_cast<char*>(&fileSize), sizeof(fileSize), 0);

        if (fileSize <= 0) return "Invalid file size received";

        string outputFilePath = basePath;
        size_t delimiterPos = outputFilePath.find('.');
        string filename = outputFilePath.substr(0, delimiterPos);
        string extention = outputFilePath.substr(delimiterPos + 1);

        // Check if file exists and adjust filename
        int fileIndex = 1;
        while (fs::exists(outputFilePath)) {
            outputFilePath = filename + "(" + std::to_string(fileIndex) + ")." + extention;
            fileIndex++;
        }

        ofstream outputFile(outputFilePath, ios::binary);
        if (!outputFile.is_open()) {
            return "Failed to open file for writing: " + outputFilePath;
        }

        int bytesReceived = 0;
        char buffer[1024];
        while (bytesReceived < fileSize) {
            int chunkSize = recv(clientSocket, buffer, min(static_cast<int>(sizeof(buffer)), fileSize - bytesReceived), 0);
            if (chunkSize <= 0) break;
            outputFile.write(buffer, chunkSize);
            bytesReceived += chunkSize;
        }

        outputFile.close();
        return "File received.";
    }

    static string sendFile(const string& filePath, SOCKET& clientSocket) {
        if (!exists(filePath)) {
            return "File does not exist: " + filePath;
        }

        ifstream file(filePath, ios::binary);
        if (!file.is_open()) {
            return "Failed to open file: " + filePath;
        }

        file.seekg(0, ios::end);
        int fileSize = static_cast<int>(file.tellg());
        file.seekg(0, ios::beg);

        send(clientSocket, reinterpret_cast<char*>(&fileSize), sizeof(fileSize), 0);

        char buffer[1024];
        while (file.read(buffer, sizeof(buffer)) || file.gcount()) {
            send(clientSocket, buffer, file.gcount(), 0);
        }

        file.close();
        return "File successfully sent";
    }
};

#endif // NETWORK_UTILS_H