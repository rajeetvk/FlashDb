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

**3. Test the Connection:**
While the server is running, open a new PowerShell window and use the following script to connect to the database via a raw TCP stream and execute commands (SET, GET, EXPIRE, DEL).

```powershell
$tcp = New-Object System.Net.Sockets.TcpClient("127.0.0.1", 6379)
$stream = $tcp.GetStream()
$writer = New-Object System.IO.StreamWriter($stream)
$reader = New-Object System.IO.StreamReader($stream)

# Test SET command
$writer.WriteLine("SET mykey 100")
$writer.Flush()
Write-Host "Server replied:" $reader.ReadLine()

# Test GET command
$writer.WriteLine("GET mykey")
$writer.Flush()
Write-Host "Server replied:" $reader.ReadLine()

$tcp.Close()
```
