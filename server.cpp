#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <ws2tcpip.h>
#include <stdio.h>
#pragma comment(lib, "ws2_32.lib")
using namespace std;


int main()
{

    WSADATA  wsadata;

    int result=WSAStartup(MAKEWORD(2,2),&wsadata);

    if(result!=0)
    {
        printf("Winsock initialization failed!\n");
        return result;
    }

    printf("Winsock initialized successfully.\n");

    //create SOCKET

    SOCKET server_socket=socket(AF_INET,SOCK_STREAM,0);
    if(server_socket==INVALID_SOCKET)
    {
        cout<<"Socket creation failed "<<endl;
        WSACleanup();
        return 1;
    }
    else
    {
        cout<<"Socket creation successful"<<endl;
    }

    //bind socket to address
    
    struct sockaddr_in address;
    address.sin_family=AF_INET;
    address.sin_addr.s_addr=INADDR_ANY;
    address.sin_port=htons(6379);
    
    int bind_result=bind(server_socket,(struct sockaddr*)&address,sizeof(address));
    if(bind_result==SOCKET_ERROR)
    {
        cout<<"Bind failed"<<endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }
    cout<<"Bind successful"<<endl;

    int listen_result=listen(server_socket,5);
    if(listen_result==SOCKET_ERROR)
    {
        cout<<"Listen failed"<<endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }
    cout<<"Listen successful"<<endl;
    
    struct sockaddr_in client_addr;
    int client_size=sizeof(client_addr);

    SOCKET client_socket=accept(server_socket,(struct sockaddr*)&client_addr,&client_size);
    if(client_socket==INVALID_SOCKET)
    {
        cout<<"Accept failed";
        closesocket(server_socket);
        WSACleanup();
        return 1;

    }
    cout<<"Client connected successfully";
    
    char buffer[1024]={0};

    while(true)
    {

    memset(buffer,0,sizeof(buffer));
    int bytes_recieved=recv(client_socket,buffer,sizeof(buffer),0);

    if(bytes_recieved>0)
    {
        cout<<"Recieved from client:" <<buffer<<endl;
        const char*response="+PONG\r\n";
        send(client_socket,response,strlen(response),0);

    }

    else if(bytes_recieved==0)
    {
        cout<<"Connection closed by client\n";
        break;
    }
    else    
    {
        cout<<"Recieve error"<<WSAGetLastError()<<endl;
        break;
    }
}

    closesocket(client_socket);
    closesocket(server_socket);


    
    WSACleanup();
    return 0;


}