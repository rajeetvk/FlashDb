#include <iostream>
#include <string>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

int main() {
    // 1. Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "Failed to initialize Winsock." << endl;
        return 1;
    }

    // 2. Create the Socket
    SOCKET client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == INVALID_SOCKET) {
        cout << "Failed to create socket." << endl;
        WSACleanup();
        return 1;
    }

    // 3. Setup the Server Address (Connecting to our Localhost Server)
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(6379);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // 4. Connect to the Server
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        cout << "Failed to connect to FlashDb on 127.0.0.1:6379." << endl;
        cout << "Make sure ./server.exe is running!" << endl;
        closesocket(client_socket);
        WSACleanup();
        return 1;
    }

    cout << "Connected to FlashDb! Type 'exit' to quit." << endl;

    string command;
    char buffer[1024];

    // 5. The Interactive CLI Loop
    while (true) {
        cout << "flashdb> ";
        getline(cin, command);

        if (command == "exit" || command == "quit") {
            break;
        }
        if (command.empty()) {
            continue;
        }

        // Add the RESP network line ending so the server parser understands it
        string network_command = command + "\r\n"; 
        
        // Send the command
        send(client_socket, network_command.c_str(), network_command.length(), 0);

        // Wait for the server's response
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        
        if (bytes_received > 0) {
            // The server's response already contains \r\n, so we just print it directly
            cout << buffer;
        } else {
            cout << "Connection lost." << endl;
            break;
        }
    }

    // 6. Cleanup
    closesocket(client_socket);
    WSACleanup();
    return 0;
}
