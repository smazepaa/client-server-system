# Chat Application

This system is a client-server application designed to enable a chat room environment where users can join rooms, send text messages, and share files within those rooms. It leverages TCP/IP sockets for reliable network communication. The server handles multiple client connections, room management, and message/file distribution using a multithreaded approach to ensure concurrent processing. On the client side, multithreading facilitates simultaneous network communication and user interaction, maintaining responsiveness while sending and receiving data. This architecture allows the system to support multiple users efficiently, providing a seamless chat experience.

## Setting Up the Connection
To ensure correct communication between clients and server, the server solution should be run first, followed by clients.

## Directory Structure
`server-dir`: The directory on the server where files are temporarily stored.

`client-dir/<clientName>`: The directory on the client used for storing files.

_**Note**: Change these to the appropriate directories on your machine._

## Application Protocol Description
The protocol includes commands for joining and leaving rooms, sending text messages, and sharing files. Each message sent over the network follows a specific format, where the first part indicates the type of action (e.g., join room, leave room, send message, send file), followed by the necessary parameters such as room ID or file name.


| Command                   | Message Content            | Example         | Total Length                           | Note                          |
|---------------------------|----------------------------|-----------------|----------------------------------------|-------------------------------|
| .m `<Message>`            | `.m` + space + `<Message>` | `.m Hello`      | Variable, (3 + message length) bytes   | User command to send a message. |
| .q                        | `.q`                       | `.q`            | 2 bytes                                | User command to quit a room.  |
| .j `<RoomID>`             | `.j` + space + `<RoomID>`  | `.j 123`        | Variable, (3 + id length) bytes        | User command to join a room.  |
| .f `<Filename>`           | `.f` + space + `<Filename>`| `.f file.txt`   | Variable, (3 + filename length) bytes  | User command to send a file.  |
| .a                        | `.a`                       | `.a  `          | 2 bytes                                | System command for acknowledging file reception. **Not used by users directly.** |
| .y `<Filename>`           | `.y` + space + `<Filename>`| `.y file.txt`   | Variable, (3 + filename length) bytes  | System command to confirm file send request. **Not used by users directly.**  |
| .n                        | `.n`                       | `.n`            | 3 bytes                                | System command to reject file send request. **Not used by users directly.** |

## Commands - Responses

### Send Message
- **Command**: `.m`
- **Usage**: Sends a message to all clients in the same room.
- **Client Response**: "You: `<Message>`"
- **Client Command**: `.m <Message>`
- **Server Response**: No direct response to the sender.
- **Response Details**: The message is broadcasted to all clients in the room. 

### Quit Room
- **Command**: `.q`
- **Usage**: Allows the client to leave the current room.
- **Client Prompt**: "Do you want to leave the room? (y/n)"
- **Client Command**: (If 'y' is chosen) `.q`
- **Server Response**: "You left ROOM `<roomID>`"
- **Response Details**: The confirmation message ("ClientName left ROOM `<roomID>") is broadcasted to all clients in the room.
- **Response Length**: The total length of the response is 14 bytes plus the length of the room ID.

### Join Room
- **Command**: `.j`
- **Usage**: Allows the client to join a specified room.
- **Client Command**: `.j <RoomID>`
- **Server Response**: "You joined ROOM `<roomID>`"
- **Response Details**: (If client is already in a room) The server processes the leave command for the current room before joining the new room. The server sends back a confirmation message including the room ID that the client joined. The message about client joining ("ClientName joined ROOM `<roomID>") is broadcasted to all clients in the room.
- **Response Length**: The total length of the response is 16 bytes plus the length of the room ID.

### Send File
- **Command**: `.f`
- **Usage**: Initiates a file transfer to the server, which is then distributed to other clients in the room.
- **Client Response**: "You: sending the file `<Filename>`"
- **Client Command**: `.f <Filename>`
- **Server Response**: Server broadcasts "ClientName: sending the file <Filename>" to all clients in the room and then sends the file data. Server waits for `.a` acknowledgments from each client.

_**Note**: After each command the line is cleared and only client/server response remains to keep the console clean._

### Acknowledge File Reception
- **Command**: `.a`
- **Usage**: Sent by the client application to acknowledge successful receipt of a file.
- **Client Command**: `.a`
- **Server Response**: Server counts acknowledgments and once all are received, sends "Everyone received the file." to the sender.
- **Response Length**: Constant, 27 bytes

### Accept File
- **Command**: `.y`
- **Usage**: Sent by the client application to accept the incoming file transfer request.
- **Client Command**: `.y <Filename>`
- **Server Response**: Upon receiving this command, the server begins the file transfer to the client.
- **Response Length**: File size

### Reject File
- **Command**: `.n`
- **Usage**: Sent by the client application to reject the incoming file transfer request.
- **Client Command**: `.n`
- **Server Response**: The server receives this command and acknowledges the client's decision to reject the file. It also adjusts the expected acknowledgment count for the ongoing file transfer operation. No direct response to the client, but to the file sender.
