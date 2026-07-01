# The Theory: Understanding C++ Sockets

If you are new to network programming, looking at C-style socket code can be incredibly overwhelming. It looks like a bunch of random capital letters, cryptic structs, and strange function calls.

To understand how a database server works under the hood, you need to understand **The Receptionist Analogy** and the exact meaning behind the parameters we pass to the OS.

---

## 1. Why `WSAStartup` and `WSACleanup`? (Windows Only)

If you were writing this server on Linux or a Mac, networking is built directly into the core operating system kernel. When you boot Linux, sockets are ready to go immediately.

Windows was historically built for offline computers (MS-DOS). Networking was added later as an external module called **Winsock** (Windows Sockets). Because Windows tries to save memory, it doesn't load the internet drivers into RAM for every single program (a calculator app shouldn't waste RAM on network drivers). 

Therefore, on Windows, you must explicitly call:
* **`WSAStartup`**: "Hey Windows, please load your networking `.dll` files into my program's memory right now."
* **`WSACleanup`**: "I'm done with the network. You can safely delete those drivers from my memory so other programs can use that RAM."

---

## 2. The Receptionist Analogy

A server actually requires **two different types of sockets** to handle a single client:

### The Listening Socket (The Receptionist)
Imagine a large office building. The Listening Socket is the **receptionist** sitting at the front desk by the main door.
* Their *only* job is to wait for new people (clients) to walk through the door. 
* When a client says "I'd like to talk to someone," the receptionist accepts them, but doesn't handle their business right there at the front desk (because that would block the door for everyone else).
* The receptionist **never** actually reads or writes the client's data. 

### The Connection Socket (The Meeting Room)
When the receptionist accepts a new client, they immediately create a brand new, private "meeting room" just for that specific client.
* This new meeting room is the **Connection Socket**.
* The client is sent to this Connection Socket to have their actual conversation (sending `GET` or `SET` commands).
* Meanwhile, the receptionist (Listening Socket) immediately goes back to watching the front door for the next person.

---

## 3. Demystifying the Execution Flow & Parameters

Here is how that analogy maps directly to the C++ code in `server.cpp`:

### Step 1: `socket()` - Hiring the Receptionist
```cpp
SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
```
We create our Listening Socket. However, right now, the receptionist is just standing in an empty field. They don't have a desk, and they don't know where the building is.

### Step 2: `bind()` - The Registration Form
To assign the receptionist to a specific door (Port `6379`), the Operating System forces us to fill out a strict "Registration Form" called `struct sockaddr_in`. 

```cpp
struct sockaddr_in address;
address.sin_family = AF_INET;
address.sin_addr.s_addr = INADDR_ANY;
address.sin_port = htons(6379);

bind(server_socket, (struct sockaddr*)&address, sizeof(address));
```
**Breaking down the parameters:**
* **`AF_INET`**: Tells the OS we are using standard IPv4 addresses (like `192.168.1.5`), not IPv6.
* **`INADDR_ANY`**: Your computer might have multiple IP addresses (e.g., local WiFi, ethernet, and localhost `127.0.0.1`). `INADDR_ANY` means "I don't care which IP the traffic comes from. Bind to **all** available network interfaces on this computer."
* **`htons(6379)`**: Computers store numbers differently based on their CPU architecture (Little Endian vs Big Endian). The internet standard strictly requires numbers to be sent in **Big Endian**. `htons()` stands for **H**ost **TO** **N**etwork **S**hort, and it automatically flips the bytes of `6379` into the safe format for the internet!
* **`(struct sockaddr*)&address`**: The `bind()` function was written decades ago, before IPv4 was the only standard. `bind` takes a generic `sockaddr`. We have to cast (convert) our specific IPv4 `sockaddr_in` into the generic one.

### Step 3: `listen()` - Opening the Doors
```cpp
listen(server_socket, 5);
```
We tell the receptionist to open their eyes and start watching the door. 
* **The `5` Parameter**: This is the "backlog". If multiple clients try to connect at the exact same millisecond, the OS puts them in a waiting line (a queue) while your Receptionist handles the first person. The `5` tells the OS to allow up to 5 people to wait in line. If a 6th person shows up, the OS will turn them away.

### Step 4: `accept()` - Creating the Meeting Room
```cpp
SOCKET client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_size);
```
This function is unique: it **blocks** (pauses) the entire program. It hangs here until a real client connects. When they do, `accept()` returns a brand new integer: `client_socket`. This is our private meeting room!

### Step 5: `recv()` and `send()` - The Conversation
We use a `while(true)` loop to continuously read (`recv`) data from the `client_socket` and write (`send`) data back. 

**Why `memset`?**
```cpp
memset(buffer, 0, sizeof(buffer)); 
```
If you don't clean out your memory buffer array on every loop iteration, old messages will merge with new messages. If the first message was `"HELLO"` and the second message was `"HI"`, without clearing the buffer, the second message would print as `"HULLO"` because the old letters weren't erased!

---

## 4. Why `closesocket()` is Crucial

When a client disconnects, or when we are done with a client, we must call `closesocket(client_socket)` (or `close()` on Linux/Mac). 
Sockets are limited OS resources (often called File Descriptors). A typical OS might limit a process to a few thousand file descriptors at a time. If you forget to close sockets, you create a **socket leak**. Eventually, `accept()` will fail because the OS runs out of room to create new meeting rooms, causing your database to reject all new connections.

---

## 5. The Next Step: Overcoming the Blocking Problem

The simple server architecture described above has a massive flaw: it is **synchronous and blocking**.
Because `accept()` and `recv()` halt the program until something happens, a basic single-threaded server can only handle **one client at a time**. If Client A connects, Client B cannot connect until Client A is finished and disconnects.

To build a real Database Engine like Redis that handles thousands of concurrent connections, we must eventually implement:
* **Non-Blocking I/O & Event Loops**: (The exact architecture Redis uses). Instead of blocking, we configure the sockets to be non-blocking. We then use an Event Loop (like `select`, `epoll`, or `kqueue`) to ask the OS: "Notify me the exact millisecond any of these 10,000 clients have data ready to read, otherwise I'll keep doing other work."

---

## 6. Building the Core Database Engine

To turn our simple network server into a functioning database engine, we modularized our architecture into three distinct components:

### The Server (Network Layer)
We upgraded the networking loop by wrapping the `accept()` function in an infinite `while(true)` loop. In the original version, when a single client disconnected, the entire server process would terminate. Now, when a client disconnects, the server gracefully closes their specific meeting room (`client_socket`) and loops back to the "receptionist" (`accept()`) to wait for the next connection.

### The Parser (Translation Layer)
Computers communicate over TCP streams using raw byte arrays (strings of text). If a client sends `SET mykey 100`, the server just sees a single string. 
We built a `Parser` class that uses C++'s `<sstream>` (String Stream) to automatically split the raw input by spaces, converting it into an actionable `std::vector<std::string>` (e.g., `["SET", "mykey", "100"]`).

### The Database (Storage Layer)
The heart of an in-memory database is just a highly optimized hash map. We used C++'s `std::unordered_map<std::string, std::string>`.
*   **Time Complexity:** Hash maps provide **O(1)** (constant time) average lookup speed. No matter if you have 10 keys or 10 million keys, it takes the exact same amount of time to `GET` or `SET` data.
*   By making the `Database` an instance variable inside the `Server` class, every connected client interacts with the exact same data store in memory. When the server crashes or closes, the RAM is cleared and the data is lost (which is why persistence features like Append-Only Files are needed in production).

---

## 7. Performance Benchmarking & Concurrency Architecture

While building this database engine, we ran a baseline performance benchmark using a PowerShell TCP client. 

### Phase 1: Single-Threaded Architecture (Current Implementation)

**What is "Single-Threaded"?**
* **Definition:** A single-threaded process executes instructions in a single, strictly sequential order. In network programming, this means the server can only handle one operation (like reading data from one client) at any given time, forcing all other clients to wait in a blocked queue.
* **The Restaurant Analogy:** Think of a thread as a single worker (a waiter). A thread can only execute one line of code at a time. 

Imagine a restaurant with only **one waiter**:
1. The waiter stands at the front door (`accept()`). Client A walks in.
2. The waiter walks Client A to a table and waits for them to speak (`recv()`). 
3. If Client A takes 5 minutes to send a command, the waiter stands permanently frozen at their table.
4. Meanwhile, Client B arrives at the front door. Because there is only one waiter, Client B is completely **blocked** and ignored until Client A fully disconnects.

Currently, the database operates on this single thread. The `accept()` loop passes the connection to `handleClient()`, which blocks other users from connecting until the first client leaves.
*   **Benchmark Results:** Despite being single-threaded and blocking, the engine successfully processed **10,000 requests in 0.97 seconds**, achieving a throughput of **10,281 Requests/Second** with an average latency of **0.097 ms per request**. *(Note: This benchmark was run for ONE client only, which is why it did not trigger the blocking problem).*
*   **The Bottleneck:** The primary bottleneck is the blocking `recv()` loop and console I/O (`std::cout`). While blazing fast for one user, it fundamentally cannot scale to concurrent users.

### Phase 2: Multi-Threading & Mutexes (Current Implementation)
To solve the blocking issue, we upgraded the server to spawn a new `std::thread` for every client that connects.
*   **The Concept:** The main thread's only job is to run `accept()`. When a client connects, it hires a "new worker" (thread) to run `handleClient()` independently.

**Code Snippet - Spawning Threads:**
```cpp
// Inside server.cpp
std::thread client_thread(&Server::handleClient, this, client_socket);
client_thread.detach(); // Detach allows it to run in the background
```

*   **The Danger (Data Races):** If two threads attempt to write to the `std::unordered_map` at the exact same microsecond, they will collide in memory, causing a Segmentation Fault. We implemented a **Mutex** (Mutual Exclusion) to lock the database during writes.

**Code Snippet - Mutex Locks:**
```cpp
// Inside database.cpp
void Database::set(string key, string value) {
    std::lock_guard<std::mutex> lock(mtx); // Locks the cash register
    store[key] = value;
} // Automatically unlocks when the function ends
```

#### The Multi-Threading Benchmarks (The Trade-offs)
We ran two separate benchmarks on this architecture to mathematically prove the downsides of threading.
1. **The Overhead Test (1 Client):** Throughput dropped to **8,395 Requests/Second**. Because we added a Mutex, the single thread was forced to waste CPU cycles "turning the key" to lock and unlock the register 10,000 times, introducing *Base Mutex Overhead*.
2. **The Contention Test (50 Simultaneous Clients):** Throughput dropped further to **5,951 Requests/Second**. With 50 threads fighting over a single Mutex lock, the Operating System spent more time managing the traffic jam (Context Switching) than processing data. This is known as *Lock Contention*.

*   **The Conclusion:** While Multi-threading solved our blocking problem and allows 50 concurrent users, it sacrificed raw top-speed. Handling 10,000 concurrent clients this way would require 10,000 heavy threads, crashing the Operating System (The C10k Problem).

### Phase 3: I/O Multiplexing (The Redis Way)
To achieve true production-scale performance, we must eventually avoid threads entirely.
*   **The Concept:** We keep a single thread but set the sockets to "non-blocking." We then use an Event Loop (like `select()`, `epoll`, or `kqueue`) to ask the OS to notify us only when a socket has data ready to read.
*   **The Result:** A single thread can juggle 100,000+ connected clients simultaneously with almost zero RAM overhead. This is the exact architecture used by the real Redis engine and Node.js.
