# Mini Redis Clone (C++)

This project is a custom, in-memory key-value database engine written in C++ from scratch. It is designed to mimic the core functionality of Redis. 

Currently, this repository focuses on the fundamental **Systems Programming and Networking** required to build a TCP server on Windows using Winsock.

> **📖 Learning Resources**
> If you are a beginner looking to understand the C++ socket code in this repository, please read [THEORY.md](./THEORY.md) for a complete breakdown of the networking concepts using simple analogies!

---

## 🚀 How to Run (Windows)

**Prerequisites:**
* Windows OS
* A C++ compiler (like `g++` via MinGW or MSVC `cl`)

**1. Compile the server:**
```bash
g++ server.cpp -o server -lws2_32
```
*(The `-lws2_32` flag is required to link the Windows networking drivers).*

**2. Start the server:**
```bash
.\server.exe
```

**3. Test the connection (in a separate PowerShell window):**
```powershell
$client = New-Object System.Net.Sockets.TcpClient("127.0.0.1", 6379)
$stream = $client.GetStream()
$writer = New-Object System.IO.StreamWriter($stream)
$reader = New-Object System.IO.StreamReader($stream)

$writer.WriteLine("PING")
$writer.Flush()
Write-Host "Server replied:" $reader.ReadLine()
$client.Close()
```
