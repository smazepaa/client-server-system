## Project Internals: Network-Based File Management System

### Overview
This system is a C++ implementation of a client-server file management application using Windows Socket API (Winsock). It enables clients to connect to a server to upload, download, list, and delete files. The system is structured into distinct components, each handling specific functionalities in the network communication process.

### Component 1: `NetworkUtils` Class
#### Overview:
`NetworkUtils` is a utility class stored in a separate header file providing static methods for sending and receiving messages and files over TCP/IP sockets that can be used by both client and server.

#### Key Methods:
- `sendMessage(SOCKET&, const string&)`: Sends a string message. It breaks the message into chunks of 1024 bytes if necessary and ensures complete transmission. Every message ends with `<END>` marker, ensuring its correct receiving.
- `receiveMessage(SOCKET&)`: Receives a string message. It continually reads from the socket until the predefined end marker is detected.
- `receiveFile(const string&, SOCKET&)`: Handles the incoming file stream and writes it to the filesystem. It checks for file existence and ensures proper stream termination using an EOF marker.
- `sendFile(const string&, SOCKET&)`: Reads a file from the filesystem and sends it over the socket, splitting it into manageable chunks (1024 bytes). It signals file transfer completion with an EOF marker.

### `Server` Component
#### ConnectionManager (Server-Side)
- **Purpose**: Initializes, configures, and manages the server socket. Handles incoming client connections.
- **Key Operations**:
  - `winsockInit()`: Initializes Winsock.
  - `serverConfig()`: Sets up the server socket and binds it to a port.
  - `acceptClient()`: Accepts incoming client connections and returns a client socket.

#### CommandHandler (Server-Side)
- **Purpose**: Processes commands received from clients and performs corresponding file operations.
- **Key Operations**:
  - `handleCommands(const string&, const string&)`: Parses the command and filename, then delegates to the specific file operation method.
  - `listFiles()`, `fileInfo(const string&)`, `deleteFile(const string&)`: Perform specific file operations.

### `Client` Component
#### ConnectionManager (Client-Side)
- **Purpose**: Manages the client's connection to the server.
- **Key Operations**:
  - `clientConfig()`: Configures the client socket and connects to the server.
  - `winsockInit()`: Initializes Winsock on the client side.

#### CommandHandler (Client-Side)
- **Purpose**: Sends file operation commands to the server and handles responses.
- **Key Operations**:
  - `inputCommand()`: Processes user input, constructs commands, and sends them to the server.
  - `getFile(const string&, const string&)`: Manages the 'GET' command for downloading files.

### System Workflow
1. **Server Initialization**:
   - Initializes Winsock.
   - Configures and binds the server socket, then starts listening for client connections.
2. **Client Connection**:
   - Client initiates Winsock.
   - Establishes a connection to the server.
3. **Command Processing**:
   - The client sends a command.
   - The server's `CommandHandler` processes the command and performs the requested operation.
   - The server sends back a response, which might include file data.
4. **File Operations**:
   - Include upload (`PUT`), download (`GET`), list (`LIST`), retrieve information (`INFO`), and delete (`DELETE`) files.

### Multithreading on the Server
- The server can handle multiple clients concurrently through its `start()` method.
- Each client connection is managed by a dedicated thread, ensuring isolated handling and responsiveness.

### Error Handling
- Comprehensive error handling is implemented, covering network failures, file operation errors, and protocol compliance issues.
- Both client and server sides include checks for socket validity and report detailed error messages.

### Communication Protocol
- The system uses a simple, custom text-based protocol for command and response communication.
- Special markers (`<END>` for messages, `<EOF>` for files) are used to denote the end of transmission, ensuring message integrity.
