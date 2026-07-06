# FlashDb

FlashDb is a high-performance, in-memory key-value database engine written entirely in C++ from scratch. It is engineered to replicate the core architectural principles of industry-standard caching systems like Redis.

## The Name: FlashDb
The name "FlashDb" represents the database's primary architectural goal: lightning-fast, sub-millisecond data processing. By keeping the active dataset entirely in RAM and utilizing Non-Blocking I/O Multiplexing, the engine achieves read and write speeds comparable to a literal flash, bypassing the latency bottlenecks of traditional hard-drive-based databases.

## Architecture and Learnings
Every core Computer Science concept, Systems Architecture design pattern, and engineering trade-off I learned while building this database engine from scratch is thoroughly documented in this repository.

Please read [THEORY.md](./THEORY.md) for my complete learning notes and a comprehensive breakdown of the engine's mechanics, including:
* Non-Blocking I/O Multiplexing (Event Loops)
* Write-Ahead Logging (AOF Persistence)
* RESP (REdis Serialization Protocol) Implementation
* O(1) LRU (Least Recently Used) Cache Eviction
* Lazy Expiration (TTL) Memory Management
## Supported Commands (Operations)
The current architecture supports the following core Redis commands:
* **`SET <key> <value>`**: Stores a string value in the database.
* **`GET <key>`**: Retrieves the value associated with a key (`O(1)` time).
* **`DEL <key>`**: Deletes a key from the database.
* **`EXPIRE <key> <seconds>`**: Sets a Time-To-Live (TTL). The key will be automatically deleted after the specified number of seconds.

## How to Run (Windows)

**Prerequisites:**
* Windows Operating System
* A standard C++ compiler (such as `g++` via MinGW)

**1. Compile the Server:**
Open a terminal in the root directory and compile the source files. The `-lws2_32` flag is strictly required to link the Windows socket networking drivers.
```bash
g++ src/main.cpp src/server.cpp src/database.cpp src/parser.cpp -o server -lws2_32
```

**2. Start the Server:**
Execute the compiled binary to start the database engine on port 6379.
```bash
.\server.exe
```

**3. Connect with the Interactive CLI:**
This repository includes a custom-built, interactive Command Line Interface (`flashdb-cli`) to interact with the database natively, exactly like `redis-cli`. 
Open a new terminal window (keep the server running in the background) and compile the client:
```bash
g++ src/client.cpp -o flashdb-cli -lws2_32
```

Launch the client to start sending commands (`SET`, `GET`, `DEL`, `EXPIRE`):
```bash
.\flashdb-cli.exe

flashdb> SET name FlashDb
+OK
flashdb> GET name
FlashDb
```
