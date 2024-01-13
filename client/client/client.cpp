#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>
#include <WinSock2.h>
#include <Ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;
namespace fs = std::filesystem;

class Client {

    SOCKET clientSocket;
    sockaddr_in serverAddr;
    int port = 12345;
    PCWSTR serverIp = L"127.0.0.1";
    string clientDirectory = "C:/Users/sofma/client-dir";

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

    void connectToServer() {
        
        clientConfig();

        if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
            cerr << "Connect failed with error: " << WSAGetLastError() << endl;
            return;
        }
    }

    void winsockInit() {
        // Initialize Winsock
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            cerr << "WSAStartup failed" << endl;
            return;
        }
    }

    void sendMessage(const char* message) {
        send(clientSocket, message, (int)strlen(message), 0);
    }

    

public:
    Client(){
        winsockInit();
        connectToServer();
    }

    ~Client() {
        closesocket(clientSocket);
        WSACleanup();
    }

    void inputCommand() {
        vector<string> inputParams;

        string line;
        getline(cin, line);

        if (line != "") {

            istringstream iss(line);
            string word;
            while (iss >> word) {
                inputParams.push_back(word);
            }
            
            string command = inputParams[0];
            string filename;
            if (inputParams.size() == 2) {
                filename = inputParams[1];
            }

            if (command == "PUT") {

                string filePath = clientDirectory + "/" + filename;

                if (!filesystem::exists(filePath)) {
                    cout << "File does not exist: " << filePath << endl << endl;
                }
                else {
                    sendMessage(line.c_str());
                    sendFile(filename);
                }
                
            }
            else if (command == "LIST") {
                sendMessage(line.c_str());
            }
        }
        else {
            cout << "Empty input" << endl;
        }
        
    }

    string receiveMessage() {
        char buffer[1024];
        memset(buffer, 0, 1024);
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived > 0) {
            return string(buffer);
        }
        return "";
    }

    void sendFile(const string& filename) {

        string filePath = clientDirectory + "/" + filename;

        ifstream file(filePath, ios::binary);
        if (!file.is_open()) {
            cout << "Failed to open file" << endl;
            return;
        }

        const size_t bufferSize = 1024;
        char buffer[bufferSize];

        while (file.read(buffer, bufferSize) || file.gcount()) {
            send(clientSocket, buffer, file.gcount(), 0); // send content by chunks
        }

        file.close();

        const char* eofMarker = "<EOF>";
        sendMessage(eofMarker); // send end-of-file marker
    }

};

int main() {

    Client client;
    while (true) {
        client.inputCommand();
        cout << client.receiveMessage() << endl;
    }
    // client.sendCommand(p);
    // client.sendMessage("Hello, server! How are you?");

    /*client.sendFile("airflight-booking.pdf");
    cout << client.receiveMessage();*/
    
    return 0;
}
