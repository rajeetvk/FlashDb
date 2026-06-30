#include "server.h"

// This is the entry point of your application.
int main() {
    // Instantiate our Server on port 6379
    Server server(6379);
    
    // If it starts up successfully, begin the infinite listening loop
    if (server.start()) {
        server.listenForClients();
    }
    
    return 0;
}
