import socket
import os
import struct


class Client:
    def __init__(self):
        self.client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_ip = '127.0.0.1'
        self.port = 12345
        self.client_dir = 'C:/Users/sofma/client-dir'
        self.is_connected = False

    def connect_to_server(self):
        try:
            self.client_socket.connect((self.server_ip, self.port))
            self.is_connected = True
        except socket.error as e:
            print(f"Connect failed with error: {e}")
            self.is_connected = False

    def is_ready(self):
        return self.is_connected

    def send_message(self, message):
        data_to_send = message + "<END>"  # end marker
        buffer_size = 1024
        total_sent = 0
        data_length = len(data_to_send)

        while total_sent < data_length:
            to_send = data_to_send[total_sent:total_sent + buffer_size].encode()
            total_sent += self.client_socket.send(to_send)


    def receive_message(self):
        end_marker = "<END>"
        total_data = []
        while True:
            data = self.client_socket.recv(1024).decode()
            if end_marker in data:
                total_data.append(data[:data.find(end_marker)])
                break
            total_data.append(data)
            if not data:
                break
        return ''.join(total_data)

    def send_file(self, file_path):
        with open(file_path, 'rb') as file:
            while True:
                data = file.read(1024)  # Read in chunks of 1024 bytes
                if not data:
                    break
                self.client_socket.sendall(data)

        eof_marker = "<EOF>"
        self.send_message(eof_marker)  # send end-of-file marker
        print(self.receive_message())

    def receive_file(self, filename):
        output_file_path = os.path.join(self.client_dir, filename)

        if os.path.exists(output_file_path):
            print("File already exists:", output_file_path)
            return

        try:
            with open(output_file_path, 'wb') as file:
                eof_marker = "<EOF>".encode()
                while True:
                    data = self.client_socket.recv(1024)
                    if eof_marker in data:
                        # Split at the EOF marker, write first part, and break
                        eof_index = data.find(eof_marker)
                        file.write(data[:eof_index])
                        break
                    else:
                        file.write(data)

            print("File transfer completed")
            self.send_message("File transfer completed")
        except IOError as e:
            error_message = f"Failed to open file for writing: {e}"
            print(error_message)
            self.send_message(error_message)

    def input_command(self):
        line = input()
        if line:
            parts = line.split()
            command = parts[0]
            filename = parts[1] if len(parts) > 1 else ""

            if command == "PUT":
                file_path = os.path.join(self.client_dir, filename)
                if not os.path.exists(file_path):
                    print("File does not exist:", file_path)
                else:
                    self.send_message(line)
                    self.send_file(file_path)
                    print(self.receive_message())
            elif command in ["LIST", "INFO", "DELETE"]:
                self.send_message(line)
                print(self.receive_message())
            elif command == "GET":
                self.send_message(line)
                server_response = self.receive_message()
                if "File does not exist" not in server_response:
                    self.receive_file(filename)
                else:
                    print(server_response)
            else:
                print("Invalid command")

    def close(self):
        self.client_socket.close()


def main():
    client = Client()
    client.connect_to_server()
    if client.is_ready():
        try:
            while True:
                client.input_command()
        except KeyboardInterrupt:
            print("\nClient closing.")
            client.close()
    else:
        print("Client is not ready to connect")
        sys.exit(1)


if __name__ == "__main__":
    main()
