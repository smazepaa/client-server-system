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

    static string receiveFile(const string& outputFilePath, SOCKET& clientSocket) {

        if (fs::exists(outputFilePath)) {
            string response = "File already exists: " + outputFilePath;
            return response;
        }

        ofstream outputFile(outputFilePath, ios::binary);
        if (!outputFile.is_open()) {
            string response = "Failed to open file for writing: " + outputFilePath;
            return response;
        }

        const size_t bufferSize = 1024;
        char buffer[bufferSize];
        string eofMarker = "<EOF>";
        string fileData;

        while (true) {
            cout << "start receiving" << endl;
            memset(buffer, 0, bufferSize);
            int bytesReceived = recv(clientSocket, buffer, bufferSize, 0);

            if (bytesReceived <= 0) {
                string response = "File transfer failed";
                return response;
            }

            fileData.append(buffer, bytesReceived);

            // check for the end of file
            size_t eofPos = fileData.find(eofMarker);
            if (eofPos != string::npos) {
                outputFile.write(fileData.c_str(), eofPos); // remove end-of-file marker
                break;

                // write to file if buffer is full and eof marker not found
                if (fileData.size() >= bufferSize) {
                    outputFile.write(fileData.c_str(), fileData.size() - eofMarker.size());
                    fileData.erase(0, fileData.size() - eofMarker.size());
                }
            }
        }

        outputFile.close();
        string response = "File transfer completed and saved to: " + outputFilePath;
        return response;
    }

    static string sendFile(const string& filePath, SOCKET& clientSocket) {

        if (!exists(filePath)) {
            string response = "File does not exist: " + filePath;
            return response;
        }

        ifstream file(filePath, ios::binary);
        if (!file.is_open()) {
            string response = "Failed to open file: " + filePath;
            return response;
        }

        const size_t bufferSize = 1024;
        char buffer[bufferSize];

        while (file.read(buffer, bufferSize) || file.gcount()) {
            send(clientSocket, buffer, file.gcount(), 0); // Send content by chunks
        }

        file.close();

        const char* eofMarker = "<EOF>";
        send(clientSocket, eofMarker, strlen(eofMarker), 0); // Send EOF marker

        string response = "File successfully sent";
        return response;
    }
};

#endif // NETWORK_UTILS_H
