#include <iostream>
#include <WinSock2.h>
#include <Ws2tcpip.h>
// Linking the library needed for network communication
#pragma comment(lib, "ws2_32.lib")
int main()
{
	// Initialize Winsock
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::cerr << "WSAStartup failed" << std::endl;
		return 1;
	}
	// Client configuration
	int port = 12345;
	PCWSTR serverIp = L"127.0.0.1";
	SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (clientSocket == INVALID_SOCKET)
	{
		std::cerr << "Error creating socket: " << WSAGetLastError() << std::endl;
		WSACleanup();
		return 1;
	}
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	InetPton(AF_INET, serverIp, &serverAddr.sin_addr);
	// Connect to the server
	if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR)
	{
		std::cerr << "Connect failed with error: " << WSAGetLastError() << std::endl;
		closesocket(clientSocket);
		WSACleanup();
		return 1;
	}
	// Send data to the server
	const char* message = "Hello, server! How are you?";
	send(clientSocket, message, (int)strlen(message), 0);
	// Receive the response from the server
	char buffer[1024];
	memset(buffer, 0, 1024);
	int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
	if (bytesReceived > 0)
	{
		std::cout << "Received from server: " << buffer << std::endl;
	}
	// Clean up
	closesocket(clientSocket);
	WSACleanup();
	return 0;
}
