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
        string dataToSend = message + "<END>"; // End marker
        const size_t bufferSize = 1024;
        size_t dataLength = dataToSend.length();
        size_t sent = 0;

        while (sent < dataLength) {
            size_t toSend = min(bufferSize, dataLength - sent);
            int bytesSent = send(clientSocket, dataToSend.c_str() + sent, toSend, 0);
            if (bytesSent == SOCKET_ERROR) {
                cerr << "Failed to send data: " << WSAGetLastError() << endl;
                break;
            }
            sent += bytesSent;
        }
    }

    static string receiveMessage(SOCKET& clientSocket) {
        string totalData;
        char buffer[1024];
        const string endMarker = "<END>";
        size_t found;

        while (true) {
            memset(buffer, 0, 1024);
            int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);

            if (bytesReceived > 0) {
                totalData.append(buffer, bytesReceived);

                found = totalData.find(endMarker);
                if (found != string::npos) {
                    totalData.erase(found, endMarker.length()); // remove the end marker
                    break;
                }
            }
            else {
                break;
            }
        }
        return totalData;
    }

    static void sendWithLength(SOCKET socket, const char* data, size_t length) {
        // Send the length of the data first
        send(socket, reinterpret_cast<const char*>(&length), sizeof(size_t), 0);
        // Then send the data
        send(socket, data, length, 0);
    }

    static void recvWithLength(SOCKET socket, string& data) {
        size_t length;
        // Receive the length of the data first
        recv(socket, reinterpret_cast<char*>(&length), sizeof(size_t), 0);
        // Allocate buffer according to the length
        char* buffer = new char[length + 1];
        // Receive the data
        recv(socket, buffer, length, 0);
        // Null-terminate the string
        buffer[length] = '\0';
        // Assign to the data string
        data.assign(buffer, length);
        // Clean up
        delete[] buffer;
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
        size_t fileSize = file.tellg();
        file.seekg(0, ios::beg);

        char* buffer = new char[fileSize];
        file.read(buffer, fileSize);

        // Sending file size and content
        sendWithLength(clientSocket, buffer, fileSize);

        file.close();
        delete[] buffer;

        return "File successfully sent";
    }

    static string receiveFile(const string& outputFilePath, SOCKET& clientSocket) {
        ofstream outputFile(outputFilePath, ios::binary);
        if (!outputFile.is_open()) {
            return "Failed to open file for writing: " + outputFilePath;
        }

        string fileData;
        // Receiving file size and content
        recvWithLength(clientSocket, fileData);

        outputFile.write(fileData.c_str(), fileData.size());
        outputFile.close();

        return "File transfer completed and saved to: " + outputFilePath;
    }


};

#endif // NETWORK_UTILS_H
