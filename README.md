# Client-Server Communication System

This document provides instructions for setting up and using a client-server communication system. It covers everything from setting up the connection to using various commands for file management.

## Setting Up the Connection
To ensure correct communication between client and server, the server solution should be run first, followed by the client.

## Directory Structure
`server-dir`: The directory on the server where files are stored.

`client-dir`: The directory on the client used for storing files.

_**Note**: Change these to the appropriate directories on your machine._

## Commands and Responses
**Syntax**: In the following commands **full filename** should be provided (for files – with both name and file extension, e.g. `text.txt`, for folders – simply their name, e.g. `my-files`). 

Moreover, the filenames **should not contain spaces**.


### PUT `<filename>`
Uploads a file from the client directory to the server.
- **Server Responses**:
  - **File already exists: `<filepath>`** – uploading the file already present in the server directory.
  - **Failed to open file for writing: `<filepath>`** - error in creating the file on the server’s end.
  - **File transfer failed** – caused either by the failure of data receiving or client disconnection.
  - **File transfer completed** – file successfully transferred and saved to the server directory.

- **Client Responses**:
  - **File does not exist: `<filepath>`** – the absence of the mentioned file in the client’s directory.


### LIST
Requests a list of available files on the server.
- **Responses**:
  - **Server directory is empty**
  - **Files in the directory: `<list of files/folders>`** - prints the list of all the files and subdirectories in the server directory.
    Files have `[File]` identifier before them, folders respectively `[Folder]`.
    **Indents** before the elements are used to show the hierarchical structure of the directory.


### INFO `<filename>`
Requests information about a specific file or folder from the server.
- **Responses**:
  - **File does not exist** - no mentioned object present on the server
  - **Information:** **Name**, **Size** (in bytes), **Type** (File/File Folder), **Last Modified** (YY-MM-DD HH:MM:SS).


### DELETE `<filename>`
Requests to delete a specific file or folder from the server.
- **Responses**:
  - **File/Folder does not exist on the server** – deleting the object not available in the server directory.
  - **Failed to delete `<filename>`** - object deletion failure.
  - **File/Folder was successfully deleted** – successful object deletion.


### GET `<filename>`
Requests to upload a specific file from the server to the client directory.
- **Server Responses**:
  - **File already exists: `<filepath>`** – uploading the file already present in the client directory.
  - **File does not exist `<filepath>`** – request file unavailable on the server.
  - **Failed to receive data** – caused either by the failure of data receiving or client disconnection.
  - **File transfer completed** – file successfully transferred and saved to the client directory.

- **Client Responses**:
  - **Failed to open file for writing: `<filepath>`** - error in creating the file on the client’s end.
