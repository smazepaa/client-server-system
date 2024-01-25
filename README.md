# Enhanced Client-Server Communication Protocol
This protocol outlines the communication strategy for a multithreaded client-server file management system. It enables multiple clients to simultaneously interact with the server for various file operations, using a chunk-based transfer method with distinct markers for message and file end.

## Setup
- **Server**: Initiates and listens on a specified port, handling multiple client connections concurrently through multithreading.
- **Client**: Connects to the server's IP and port.
- **Shared Utility (NetworkUtils.h)**: A common header for network operations (message/file receiving and sending), included in both client and server projects. Project properties should be configured to include the directory of this header.

## Multithreading and Concurrent Handling
### Server Multithreading:
- Each client connection is handled by a separate thread, allowing the server to manage multiple clients concurrently.
- Thread management involves creating, executing, and safely terminating threads after client disconnection or completion of tasks.

### Client Handling:
- At the beginning client provides its name for further distinction (each has its own subfolders in main client and server directories).
- Each client operates independently, sending commands and receiving responses as if it were the sole connected client.

## Communication Structure
**Message Format**: `[COMMAND]` `[ARGUMENTS]`, chunk-based (1024 bytes), followed by an `<END>` marker.

**File Transfer**: Chunk-based, each chunk being 1024 bytes, ending with an `<EOF>` marker.

## Commands

### **1. PUT `<filename>`**
   - Uploads a file from the client to the server.
   - **Server Response**: Success, error, or file existence status.

### **2. LIST**
   - Requests a list of files and directories from the server.
   - **Server Response**: Listing of files/directories or notice of an empty directory.

### **3. INFO `<filename>`**
   - Retrieves details of a specified file or directory.
   - **Server Response**: File information (name, size, type, last modified) or non-existence message.

### **4. DELETE `<filename>`**
   - Deletes a specific file or directory on the server.
   - **Server Response**: Confirmation of deletion or error message.

### **5. GET `<filename>`**
   - Downloads a file from the server to the client.
   - **Server Response**: File transfer status or error information.

## Chunk-Based File Transfer
- **File Uploads (PUT)**: Files are sent in chunks (1024 bytes each), with each chunk being appended to the target file on the server. The presence of an `<EOF>` marker signifies the end of the file.
- **File Downloads (GET)**: The server sends file data in chunks (1024 bytes each), and the client writes these to the target file. The `<EOF>` marker indicates the end of the file data, prompting the client to stop writing and close the file.

## Error Handling
- **Connection Issues**: Addressing lost connections by closing sockets and cleaning up resources.
- **Data Transmission Errors**: Aborting the command and communicating the error via a corresponding message.
