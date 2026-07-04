#include "server.h"
#include <ws2tcpip.h>
#include <stdio.h>



#pragma comment(lib, "ws2_32.lib")

using namespace std;

// Constructor initializes variables
Server::Server(int port) : port(port), server_socket(INVALID_SOCKET)
{
}

// Destructor cleans up when the server shuts down
Server::~Server()
{
    if (server_socket != INVALID_SOCKET)
    {
        closesocket(server_socket);
    }
    WSACleanup();
}

bool Server::start()
{
    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0)
    {
        printf("Winsock initialization failed!\n");
        return false;
    }
    printf("Winsock initialized successfully.\n");

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET)
    {
        cout << "Socket creation failed " << endl;
        return false;
    }
    cout << "Socket creation successful" << endl;

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR)
    {
        cout << "Bind failed" << endl;
        return false;
    }
    cout << "Bind successful" << endl;

    if (listen(server_socket, 5) == SOCKET_ERROR)
    {
        cout << "Listen failed" << endl;
        return false;
    }
    cout << "Listen successful. Server running on port " << port << endl;
    return true;
}

// Outer loop: Keeps accepting new clients indefinitely
void Server::listenForClients()
{
    // 1. Create the Master List (The Buzzer Board)
    fd_set master_set;
    FD_ZERO(&master_set);
    FD_SET(server_socket, &master_set); // Add the front door to the list

    while (true)
    {
        // select() is destructive, so we give it a copy of the list
        fd_set copy_set = master_set; 
        
        // 2. Put the Waiter to sleep until ANY socket buzzes!
        int socketCount = select(0, &copy_set, nullptr, nullptr, nullptr);

        // 3. A buzzer went off! Find out who it was:
        for (int i = 0; i < copy_set.fd_count; i++)
        {
            SOCKET buzzed_socket = copy_set.fd_array[i];

            if (buzzed_socket == server_socket)
            {
                // The front door buzzed! A new client is here.
                struct sockaddr_in client_addr;
                int client_size = sizeof(client_addr);
                SOCKET client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_size);
                
                if (client_socket != INVALID_SOCKET)
                {
                    FD_SET(client_socket, &master_set); // Hand them a buzzer
                }
            }
            else
            {
                // A client's table buzzed! They sent data.
                bool is_still_connected = handleClient(buzzed_socket);
                
                if (!is_still_connected)
                {
                    // They left, take their buzzer back
                    FD_CLR(buzzed_socket, &master_set);
                }
            }
        }
    }
}

// Inner loop: Reads data from a single client until they disconnect
bool Server::handleClient(SOCKET client_socket) 
{
    char buffer[1024] = {0};

    memset(buffer, 0, sizeof(buffer));
    int bytes_recieved = recv(client_socket, buffer, sizeof(buffer), 0);

    if (bytes_recieved > 0)
    {
        Parser parser;
        vector<string>tokens=parser.parse(buffer);
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
    else if (bytes_recieved == 0)
    {
        cout << "Connection closed by client\n";
        closesocket(client_socket);
        return false; // Break the loop so the server can accept the next client
    }
    else
    {
        cout << "Recieve error " << WSAGetLastError() << endl;
        closesocket(client_socket);
        return false;
    }
    
    return true;
}
