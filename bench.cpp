#include <iostream>
#include <vector>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// This is the function each thread will run
void clientTask(int num_requests) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(6379);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        return; // Connection failed
    }

    char buffer[1024];
    for (int i = 0; i < num_requests; i++) {
        const char* req = "SET benchkey 99\r\n";
        send(sock, req, strlen(req), 0);
        recv(sock, buffer, sizeof(buffer), 0); // Wait for +OK
    }
    closesocket(sock);
}

int main() {
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);

    int num_clients = 50;
    int requests_per_client = 200; // 50 * 200 = 10,000 total

    cout << "Starting Concurrent Benchmark with " << num_clients << " simultaneous clients..." << endl;

    auto start_time = chrono::high_resolution_clock::now();

    // Spin up 50 clients at the exact same time!
    vector<thread> threads;
    for (int i = 0; i < num_clients; i++) {
        threads.push_back(thread(clientTask, requests_per_client));
    }

    // Wait for all of them to finish
    for (auto& t : threads) {
        t.join();
    }

    auto end_time = chrono::high_resolution_clock::now();
    chrono::duration<double> elapsed = end_time - start_time;

    int total_requests = num_clients * requests_per_client;
    
    cout << "\n--- CONCURRENT BENCHMARK RESULTS ---" << endl;
    cout << "Total Requests:  " << total_requests << endl;
    cout << "Total Time:      " << elapsed.count() << " seconds" << endl;
    cout << "Throughput:      " << total_requests / elapsed.count() << " Requests / Second" << endl;

    WSACleanup();
    return 0;
}
