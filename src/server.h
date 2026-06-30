#pragma once
#include <winsock2.h>
#include <windows.h>
#include "parser.h"
#include "database.h"
#include <iostream>

// The Server class will handle all the networking logic (Winsock).
class Server {
private:
    SOCKET server_socket;
    int port;
    Database db;

public:
    Server(int port);
    ~Server();
    
    bool start();
    void listenForClients();
    void handleClient(SOCKET client_socket);
};
