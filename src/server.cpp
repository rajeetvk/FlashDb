#include "server.h"
#include <ws2tcpip.h>
#include <stdio.h>
#include <thread>


#pragma comment(lib, "ws2_32.lib")

using namespace std;

// Constructor initializes variables
Server::Server(int port) : port(port), server_socket(INVALID_SOCKET) {}

// Destructor cleans up when the server shuts down
Server::~Server() {
    if (server_socket != INVALID_SOCKET) {
        closesocket(server_socket);
    }
    WSACleanup();
}

bool Server::start() {
    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0) {
        printf("Winsock initialization failed!\n");
        return false;
    }
    printf("Winsock initialized successfully.\n");

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        cout << "Socket creation failed " << endl;
        return false;
    }
    cout << "Socket creation successful" << endl;

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        cout << "Bind failed" << endl;
        return false;
    }
    cout << "Bind successful" << endl;

    if (listen(server_socket, 5) == SOCKET_ERROR) {
        cout << "Listen failed" << endl;
        return false;
    }
    cout << "Listen successful. Server running on port " << port << endl;
    return true;
}

// Outer loop: Keeps accepting new clients indefinitely
void Server::listenForClients() {
    while (true) {
        struct sockaddr_in client_addr;
        int client_size = sizeof(client_addr);

        SOCKET client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_size);
        if (client_socket == INVALID_SOCKET) {
            cout << "Accept failed" << endl;
            continue; // Don't crash, just try to accept the next one
        }
        cout << "Client connected successfully" << endl;

        //pass the client to a new thread of execution
        thread client_thread(&Server::handleClient,this,client_socket);
        //detach creates a new thread of execution that runs independently of the main thread.
        client_thread.detach();
    }
}

// Inner loop: Reads data from a single client until they disconnect
void Server::handleClient(SOCKET client_socket) {
    char buffer[1024] = {0};

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_recieved = recv(client_socket, buffer, sizeof(buffer), 0);

        if (bytes_recieved > 0) {
            cout << "Recieved from client: " << buffer << endl;
            Parser parser;
            vector<string>tokens=parser.split(buffer);
            if(tokens.size()>0)
        {
            string command=tokens[0];
            if(command=="SET" && tokens.size()>=3)
            {
                db.set(tokens[1],tokens[2]);
                string response="+OK\r\n";
                send(client_socket,response.c_str(),response.length(),0);
            }
            else if(command=="GET" && tokens.size()>=2)
            {
                string value=db.get(tokens[1]);
                string response;
                if(value=="")
                {
                    response="+nil\r\n";
                }
                else
                {
                    response =value + "\r\n";
                }
                send(client_socket,response.c_str(),response.length(),0);
            }
            else if(command=="DEL" && tokens.size()>=2)
            {
                db.del(tokens[1]);
                string response="+OK\r\n";
                send(client_socket,response.c_str(),response.length(),0);
            }
            else
            {
                string error = "-ERR wrong number of arguments\r\n";
                send(client_socket, error.c_str(), error.length(), 0);
            }
        }
            
        } 
        else if (bytes_recieved == 0) {
            cout << "Connection closed by client\n";
            break; // Break the loop so the server can accept the next client
        } else {
            cout << "Recieve error " << WSAGetLastError() << endl;
            break;
        }
    }
    closesocket(client_socket);
}
