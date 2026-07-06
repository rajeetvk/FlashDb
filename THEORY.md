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

*   **The Current Limitation (Binary Safety):** Parsing by spaces works perfectly for basic text, but it fundamentally breaks if a user tries to store a sentence containing spaces (e.g., `SET message "Hello World"` incorrectly splits into 4 pieces). Furthermore, if a user attempts to store a raw binary file (like a JPEG image), the image data will contain invisible space characters and null bytes, completely corrupting the file when the parser attempts to split it.
*   **The Solution (RESP):** To achieve true binary safety and full ecosystem compatibility with official Redis tools (like the `redis-cli`), the database must eventually adopt the **REdis Serialization Protocol (RESP)**. RESP uses strict length-prefixing (telling the server exactly how many bytes to read) rather than blindly searching for spaces.

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

### Phase 2: Multi-Threading & Mutexes (Archived in `v2-multi-threaded`)
To solve the blocking issue, we temporarily upgraded the server to spawn a new `std::thread` for every client that connects.
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

### Phase 3: I/O Multiplexing Event Loop (Current Implementation)
To achieve true production-scale performance and overcome the C10k Problem, we fired the extra threads and returned to a Single-Threaded architecture, but replaced the blocking loops with an **Event Loop**.
*   **The Concept:** We use the `select()` function to create a "Master List" (`fd_set`) of all connected client sockets. Instead of blocking on a single client, the main thread sleeps and asks the Operating System to wake it up the exact microsecond *any* socket on the list has data ready to read.
*   **The Result:** A single thread can juggle thousands of connected clients simultaneously with almost zero RAM overhead. Because there is only one thread, it is physically impossible to have a data race, allowing us to completely delete the Mutex lock and eliminate Context Switching. 

#### How the Event Loop Works (The Mechanics)
An Event Loop fundamentally changes how the server waits for data. Instead of assigning a thread to wait at a specific socket (which blocks execution), we use **I/O Multiplexing**.
1. **The Roster (`fd_set`):** We maintain a master list of all active file descriptors (sockets), including the main `server_socket` (the "front door") and all connected client sockets.
2. **The `select()` Call:** The single thread calls `select()`, which hands the roster to the Operating System and puts the thread to sleep. While the thread sleeps, the OS hardware (the Network Interface Card) efficiently monitors the incoming network packets in the background.
3. **The Wake-Up Call:** The exact microsecond the OS detects incoming data on *any* of the sockets, it wakes up the thread and provides an array of exactly which sockets have data ready.
4. **The Loop:** The thread loops through those specific sockets, instantly reads the data, updates the `unordered_map`, and then immediately loops back to `select()` to wait for the next event. 

Because the thread is only awake when there is guaranteed, actionable data, the CPU never wastes time waiting. Furthermore, because it delegates the waiting to the physical hardware rather than a software loop, it is incredibly power efficient.

#### Phase 3 Benchmark (The Redis Way)
*   **Benchmark Results:** The Event Loop achieved a peak throughput of **11,127 Requests/Second** while successfully handling 50 simultaneous clients. 
*   **Note on Variance (OS Jitter):** Repeated benchmark runs show normal variance (between 9.3k and 11.1k Req/Sec) due to OS CPU scheduling and background process jitter. However, the magnitude consistently demonstrates a ~2x performance increase over the Phase 2 Mutex architecture, proving the massive efficiency of I/O Multiplexing.

---

## 8. Data Persistence (Crash Recovery)
In an in-memory database, the core storage mechanism (`std::unordered_map`) lives entirely in RAM. While RAM is lightning-fast, it is **volatile**. If the server process terminates or loses power, all data vanishes. 

To make the database production-grade, it must bridge the gap between the speed of RAM and the safety of a Hard Drive. 

### Strategy 1: Snapshotting (RDB)
*   **How it works:** Periodically (e.g., every 5 minutes), the server pauses and copies the entire `unordered_map` to a `.rdb` file on the hard drive.
*   **The Flaw:** If the server crashes at minute 4, you permanently lose 4 minutes of data because the snapshot was not taken yet.

### Strategy 2: Append-Only File (AOF) - *Our Implementation*
Instead of saving the *data*, we record the *commands*. 
*   **How it works:** Every time a client successfully runs a `SET` or `DEL` command, the server instantly appends that exact command to the bottom of a text file (`database.aof`).
*   **The Recovery:** When the server boots up, the `Database` constructor opens `database.aof` in read mode. It acts like a "ghost client", reading the file from top to bottom and re-running every single `SET` and `DEL` command. By the time it reaches the end of the file, the RAM is perfectly rebuilt exactly as it was before the crash.

### Strict Durability: Write-Ahead Logging (WAL)
We implemented AOF using the **Write-Ahead Logging** architecture (the strict durability strategy used by PostgreSQL). 

**The Dilemma (Why not update RAM first?):**
If a database updates RAM first and then attempts to save to the Hard Drive, there is a microscopic window of vulnerability. If the server loses power in the exact microsecond *between* updating RAM and writing to the disk, the data is lost forever. Worse, if the server sent a +OK response to the client during this window, the client incorrectly assumes their data is safe. This creates silent data corruption.

**The Solution (The PostgreSQL Way):**
To guarantee absolute durability, we must write to the Hard Drive *first*. 
The exact order of operations in our `Database::set()` function is:
1. **Write to Disk First:** Append the command (`SET mykey 100`) to the `.aof` file.
2. **Force Flush:** Call `aof_file.flush()` to force the Operating System to physically write the data to the magnetic disk/SSD immediately, bypassing the OS memory buffer.
3. **Update RAM Second:** Only after the Hard Drive write is guaranteed successful, we update the `unordered_map`.
4. **Respond:** Send `+OK` to the client.

By explicitly choosing the PostgreSQL route, it is physically impossible for the client to receive a false `+OK`. RAM acts strictly as a blazing-fast "read cache" that perfectly mirrors the truth of the Hard Drive.

---

## 9. The REdis Serialization Protocol (RESP)
To transition from a custom C++ experiment to a true, industry-standard Redis Clone, the server must implement **RESP**. RESP is the official networking language used by Redis.

### Why do we need RESP? (The Flaw of Space-Splitting)
Initially, our parser decoded commands by splitting the raw text at every space character (e.g., `SET name Raj` became `["SET", "name", "Raj"]`). While this works for simple commands, it creates two major architectural flaws:
1. **No Spaces Allowed:** If a client tries to save a sentence (e.g., `SET message "Hello World"`), the parser splits it into 4 incorrect pieces.
2. **Not Binary Safe:** If a client attempts to save a raw binary file (like a JPEG image), the binary data will contain invisible spaces, null bytes (`\0`), and carriage returns. The space-splitter would instantly corrupt the binary data.

### The Solution: Length-Prefixing
Instead of blindly searching for spaces, RESP requires the client to send a structured package that strictly declares the **exact length** (in bytes) of every word. Because the server knows exactly how many bytes to read, it safely ignores spaces, quotes, and binary gibberish, making the protocol **100% Binary Safe**.

### RESP Syntax and Structure
In RESP, every line must end with `\r\n` (Carriage Return + Line Feed). The very first character of the line tells the server what type of data to expect.

The 5 Core RESP Symbols:
1. `+` **Simple Strings:** Used for simple success messages (e.g., `+OK\r\n`).
2. `-` **Errors:** Used when a command fails. The `-` symbol acts as a dedicated error flag. Official clients instantly recognize it and throw exceptions in red text (e.g., `-ERR unknown command\r\n`).
3. `:` **Integers:** Used for returning numbers (e.g., `:1000\r\n`).
4. `$` **Bulk Strings:** Used for sending actual data. It provides the length of the string, followed by the string itself. (e.g., `$5\r\nHello\r\n`).
5. `*` **Arrays:** Used for sending lists of data, primarily used by the client to send commands. (e.g., `*3\r\n` means an array of 3 Bulk Strings).

### Example: Translating a Command
If you type `SET name Raj` into the official `redis-cli`, the CLI secretly converts it into the following RESP payload before transmitting it over TCP:

```text
*3\r\n
$3\r\n
SET\r\n
$4\r\n
name\r\n
$3\r\n
Raj\r\n
```

**How the C++ Server Parses It:**
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

*   **The Current Limitation (Binary Safety):** Parsing by spaces works perfectly for basic text, but it fundamentally breaks if a user tries to store a sentence containing spaces (e.g., `SET message "Hello World"` incorrectly splits into 4 pieces). Furthermore, if a user attempts to store a raw binary file (like a JPEG image), the image data will contain invisible space characters and null bytes, completely corrupting the file when the parser attempts to split it.
*   **The Solution (RESP):** To achieve true binary safety and full ecosystem compatibility with official Redis tools (like the `redis-cli`), the database must eventually adopt the **REdis Serialization Protocol (RESP)**. RESP uses strict length-prefixing (telling the server exactly how many bytes to read) rather than blindly searching for spaces.

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

### Phase 2: Multi-Threading & Mutexes (Archived in `v2-multi-threaded`)
To solve the blocking issue, we temporarily upgraded the server to spawn a new `std::thread` for every client that connects.
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

### Phase 3: I/O Multiplexing Event Loop (Current Implementation)
To achieve true production-scale performance and overcome the C10k Problem, we fired the extra threads and returned to a Single-Threaded architecture, but replaced the blocking loops with an **Event Loop**.
*   **The Concept:** We use the `select()` function to create a "Master List" (`fd_set`) of all connected client sockets. Instead of blocking on a single client, the main thread sleeps and asks the Operating System to wake it up the exact microsecond *any* socket on the list has data ready to read.
*   **The Result:** A single thread can juggle thousands of connected clients simultaneously with almost zero RAM overhead. Because there is only one thread, it is physically impossible to have a data race, allowing us to completely delete the Mutex lock and eliminate Context Switching. 

#### How the Event Loop Works (The Mechanics)
An Event Loop fundamentally changes how the server waits for data. Instead of assigning a thread to wait at a specific socket (which blocks execution), we use **I/O Multiplexing**.
1. **The Roster (`fd_set`):** We maintain a master list of all active file descriptors (sockets), including the main `server_socket` (the "front door") and all connected client sockets.
2. **The `select()` Call:** The single thread calls `select()`, which hands the roster to the Operating System and puts the thread to sleep. While the thread sleeps, the OS hardware (the Network Interface Card) efficiently monitors the incoming network packets in the background.
3. **The Wake-Up Call:** The exact microsecond the OS detects incoming data on *any* of the sockets, it wakes up the thread and provides an array of exactly which sockets have data ready.
4. **The Loop:** The thread loops through those specific sockets, instantly reads the data, updates the `unordered_map`, and then immediately loops back to `select()` to wait for the next event. 

Because the thread is only awake when there is guaranteed, actionable data, the CPU never wastes time waiting. Furthermore, because it delegates the waiting to the physical hardware rather than a software loop, it is incredibly power efficient.

#### Phase 3 Benchmark (The Redis Way)
*   **Benchmark Results:** The Event Loop achieved a peak throughput of **11,127 Requests/Second** while successfully handling 50 simultaneous clients. 
*   **Note on Variance (OS Jitter):** Repeated benchmark runs show normal variance (between 9.3k and 11.1k Req/Sec) due to OS CPU scheduling and background process jitter. However, the magnitude consistently demonstrates a ~2x performance increase over the Phase 2 Mutex architecture, proving the massive efficiency of I/O Multiplexing.

---

## 8. Data Persistence (Crash Recovery)
In an in-memory database, the core storage mechanism (`std::unordered_map`) lives entirely in RAM. While RAM is lightning-fast, it is **volatile**. If the server process terminates or loses power, all data vanishes. 

To make the database production-grade, it must bridge the gap between the speed of RAM and the safety of a Hard Drive. 

### Strategy 1: Snapshotting (RDB)
*   **How it works:** Periodically (e.g., every 5 minutes), the server pauses and copies the entire `unordered_map` to a `.rdb` file on the hard drive.
*   **The Flaw:** If the server crashes at minute 4, you permanently lose 4 minutes of data because the snapshot was not taken yet.

### Strategy 2: Append-Only File (AOF) - *Our Implementation*
Instead of saving the *data*, we record the *commands*. 
*   **How it works:** Every time a client successfully runs a `SET` or `DEL` command, the server instantly appends that exact command to the bottom of a text file (`database.aof`).
*   **The Recovery:** When the server boots up, the `Database` constructor opens `database.aof` in read mode. It acts like a "ghost client", reading the file from top to bottom and re-running every single `SET` and `DEL` command. By the time it reaches the end of the file, the RAM is perfectly rebuilt exactly as it was before the crash.

### Strict Durability: Write-Ahead Logging (WAL)
We implemented AOF using the **Write-Ahead Logging** architecture (the strict durability strategy used by PostgreSQL). 

**The Dilemma (Why not update RAM first?):**
If a database updates RAM first and then attempts to save to the Hard Drive, there is a microscopic window of vulnerability. If the server loses power in the exact microsecond *between* updating RAM and writing to the disk, the data is lost forever. Worse, if the server sent a +OK response to the client during this window, the client incorrectly assumes their data is safe. This creates silent data corruption.

**The Solution (The PostgreSQL Way):**
To guarantee absolute durability, we must write to the Hard Drive *first*. 
The exact order of operations in our `Database::set()` function is:
1. **Write to Disk First:** Append the command (`SET mykey 100`) to the `.aof` file.
2. **Force Flush:** Call `aof_file.flush()` to force the Operating System to physically write the data to the magnetic disk/SSD immediately, bypassing the OS memory buffer.
3. **Update RAM Second:** Only after the Hard Drive write is guaranteed successful, we update the `unordered_map`.
4. **Respond:** Send `+OK` to the client.

By explicitly choosing the PostgreSQL route, it is physically impossible for the client to receive a false `+OK`. RAM acts strictly as a blazing-fast "read cache" that perfectly mirrors the truth of the Hard Drive.

---

## 9. The REdis Serialization Protocol (RESP)
To transition from a custom C++ experiment to a true, industry-standard Redis Clone, the server must implement **RESP**. RESP is the official networking language used by Redis.

### Why do we need RESP? (The Flaw of Space-Splitting)
Initially, our parser decoded commands by splitting the raw text at every space character (e.g., `SET name Raj` became `["SET", "name", "Raj"]`). While this works for simple commands, it creates two major architectural flaws:
1. **No Spaces Allowed:** If a client tries to save a sentence (e.g., `SET message "Hello World"`), the parser splits it into 4 incorrect pieces.
2. **Not Binary Safe:** If a client attempts to save a raw binary file (like a JPEG image), the binary data will contain invisible spaces, null bytes (`\0`), and carriage returns. The space-splitter would instantly corrupt the binary data.

### The Solution: Length-Prefixing
Instead of blindly searching for spaces, RESP requires the client to send a structured package that strictly declares the **exact length** (in bytes) of every word. Because the server knows exactly how many bytes to read, it safely ignores spaces, quotes, and binary gibberish, making the protocol **100% Binary Safe**.

### RESP Syntax and Structure
In RESP, every line must end with `\r\n` (Carriage Return + Line Feed). The very first character of the line tells the server what type of data to expect.

The 5 Core RESP Symbols:
1. `+` **Simple Strings:** Used for simple success messages (e.g., `+OK\r\n`).
2. `-` **Errors:** Used when a command fails. The `-` symbol acts as a dedicated error flag. Official clients instantly recognize it and throw exceptions in red text (e.g., `-ERR unknown command\r\n`).
3. `:` **Integers:** Used for returning numbers (e.g., `:1000\r\n`).
4. `$` **Bulk Strings:** Used for sending actual data. It provides the length of the string, followed by the string itself. (e.g., `$5\r\nHello\r\n`).
5. `*` **Arrays:** Used for sending lists of data, primarily used by the client to send commands. (e.g., `*3\r\n` means an array of 3 Bulk Strings).

### Example: Translating a Command
If you type `SET name Raj` into the official `redis-cli`, the CLI secretly converts it into the following RESP payload before transmitting it over TCP:

```text
*3\r\n
$3\r\n
SET\r\n
$4\r\n
name\r\n
$3\r\n
Raj\r\n
```

**How the C++ Server Parses It:**
1. Sees `*3`: Expects an array of 3 words.
2. Sees `$3`: Extracts exactly 3 bytes -> `SET`
3. Sees `$4`: Extracts exactly 4 bytes -> `name`
4. Sees `$3`: Extracts exactly 3 bytes -> `Raj`

### Ecosystem Compatibility
By implementing RESP, the custom C++ server immediately becomes a **Drop-in Replacement** for the real Redis. This means you can use official tools like `redis-cli`, RedisInsight (Visual GUI), and standard Node.js/Python Redis libraries to communicate with the custom database, proving its flawless adherence to industry network specifications.

### The "Everything is a String" Design
You might notice that even if a client sends a number (e.g., `SET age 25`), our parser extracts it as a Bulk String (`$2\r\n25\r\n`) and stores it as a `std::string` in our `std::unordered_map`. This is not a shortcut—it is exactly how the real Redis engine works. Redis is fundamentally a **Key-Value String Store**. Everything is safely stored in RAM as raw bytes (strings). If a client calls a math command like `INCR age` (increment), the server will pull the string `"25"`, convert it to an integer on-the-fly (`std::stoi`), perform the math (`26`), convert it back to a string (`"26"`), and save it. The Parser's only job is safe extraction; it is the Server's job to determine data types during command execution.

### The Problem with RESP and Modern Alternatives
While RESP is perfect for a Redis Clone, it is a 10+ year old **Text-Based Protocol**. It was designed so humans could read the raw network traffic for easy debugging. 
However, this creates a major architectural flaw: **Wasted Bandwidth**.
To send a tiny 3-letter word like "Raj", RESP forces the client to send 9 characters (`$3\r\nRaj\r\n`). That means 66% of the network bandwidth is wasted purely on formatting. At the scale of Google or Netflix (billions of requests per second), this wasted bandwidth costs millions of dollars in server bills.

**The Modern Solution (gRPC / Protocol Buffers):**
If you were building a brand-new, hyper-scale microservice today, you would not use RESP. You would use **Protocol Buffers (Protobuf)** via **gRPC** (invented by Google). Protobuf abandons human-readable text and compresses data into **Raw Binary** (1s and 0s). It uses the absolute minimum amount of electricity and bandwidth possible. While Protobuf is superior for general microservices, we specifically chose RESP for this project to ensure 100% ecosystem compatibility with official Redis tools.

---

## 10. Memory Management: LRU Cache Eviction
In a production environment, a database cannot simply grow forever. If clients push 2GB of data into a server with only 1GB of RAM, the Operating System will freeze and crash (Out of Memory). To prevent this, the database must act as a **Cache**. 

We implemented a **Least Recently Used (LRU) Cache Eviction** policy. When the database reaches its capacity limit, it automatically deletes the oldest, least-accessed data to make room for new keys.

### The Data Structure Challenge (O(1) Performance)
Implementing an LRU Cache is a famous systems engineering challenge (LeetCode #146). A standard `std::unordered_map` cannot track the "age" or "usage order" of keys. To achieve this in `O(1)` constant time, we fused two data structures together:
1. **The Hash Map (`std::unordered_map`)**: Provides instant `O(1)` access to keys. 
2. **A Doubly Linked List (`std::list`)**: Tracks the usage history. The *most recently used* key is always at the front, and the *least recently used* key is at the back.

To make the integration seamless, the Hash Map does not just store the string value. It stores a `std::pair` containing the value AND a direct memory pointer (`std::list::iterator`) to the exact node in the Linked List.
*   **On GET:** The server instantly finds the key in the map, uses the pointer to instantly snip the node out of the middle of the linked list, and pastes it at the front (`splice`), marking it as recently used.
*   **On Eviction (When Full):** The server looks at the back of the list, deletes that key from the map, and pops it off the list.

### The Architectural Trade-Off: AOF vs LRU
There is a fascinating quirk in this architecture regarding the `.aof` (Append-Only File) persistence layer. 
The AOF file perfectly records every `SET` and `DEL` command. However, we deliberately **do not log `GET` commands** to the hard drive. 

*   **The Flaw:** If the server crashes and reboots, it perfectly rebuilds the data by replaying the `.aof` file. However, because `GET` commands were not recorded, the database **loses its Read History**. Upon reboot, the LRU list is organized purely by *insertion time*, forgetting any recent `GET` operations.
*   **The Justification:** This is an intentional, industry-standard compromise used by the real Redis engine. The entire purpose of an in-memory database is to provide sub-millisecond reads (~100 nanoseconds). If the server was forced to perform a Hard Drive Write (~2,000,000 nanoseconds) every time a client called `GET`, read performance would be completely destroyed. 

In systems engineering, losing the exact LRU queue order during a crash is considered "acceptable data loss" in order to preserve blazing-fast read performance, so long as the core data itself is safely recovered.

---

## 11. Advanced Persistence and Expiration Mechanics

As an in-memory database scales, managing the strict boundary between RAM (the active dataset) and the Hard Drive (the recovery journal) introduces several complex architectural challenges.

### The Single Source of Truth
When an LRU cache successfully evicts a key from RAM, a natural question arises: *Because the key was originally written to the `.aof` file, why doesn't the database simply fetch the key from the `.aof` file when a user requests it?*

This highlights the fundamental rule of an in-memory database: **The RAM is the only source of truth.** 
Traditional databases (like MySQL) use the hard drive as the primary storage. In contrast, an in-memory database treats the `.aof` file strictly as a **Write-Only Disaster Recovery Journal**. During normal operation, the server never reads from the `.aof` file. When a client sends a `GET` request, the database only checks the RAM (the `std::unordered_map`). If the key was evicted by the LRU policy, the database immediately returns `null`. Fetching data from the hard drive would introduce massive latency, defeating the entire purpose of an ultra-fast cache. The `.aof` file is read exactly once: during the server's initial boot sequence.

### TTL (Time-To-Live) vs LRU Eviction
While LRU protects the server from running out of hardware resources (RAM), **TTL (Time-To-Live)** is used to expire data based on absolute time (e.g., expiring a user session after 24 hours). 

Unlike LRU (which depends entirely on memory capacity limits), TTL is a strict property of the data itself. Because of this, TTL expirations *must* be logged to the `.aof` file to survive server crashes. However, writing `EXPIRE key 60` to the `.aof` file creates a vulnerability: if the server is offline for 5 minutes and reboots, replaying that command would grant the key another 60 seconds of life. To prevent resurrecting dead data, standard databases convert the TTL into an absolute Unix timestamp (e.g., `EXPIREAT key 1717590000`). When the server boots up, it compares the timestamp in the `.aof` file to the current OS clock, instantly deleting the key if the time has passed.

In code, this natural expiration is handled via **Lazy Expiration** (the server checks the timestamp and deletes the key the exact moment a client tries to `GET` it) and **Active Expiration** (a background thread wakes up periodically to sample and delete expired keys). Both mechanisms explicitly synthesize and append a `DEL` command to the `.aof` file when they catch an expired key.

#### Our C++ Implementation
To achieve this in our custom database engine, we upgraded the `std::unordered_map` to store a custom `CacheItem` struct instead of a standard string pair.
```cpp
struct CacheItem {
    std::string value;
    std::list<std::string>::iterator lru_it;
    long long expire_time; // Absolute Unix Timestamp
};
```
By utilizing `<chrono>`, we successfully implemented strict **Lazy Expiration** in `O(1)` time. Whenever a client calls `GET`, the server evaluates the `expire_time`. If the current clock has surpassed the timestamp, the server intercepts the request, instantly executes an internal `del()` operation (which correctly syncs the deletion to the `.aof` file), and returns `null` to the client.


### The AOF Bloat Problem (BGREWRITEAOF)
Since the `.aof` file accurately records every `SET` and `DEL` command, a server running for 6 months will generate millions of lines of history for keys that no longer exist. If the server crashes, replaying this massive file would take hours, mindlessly recreating and deleting data just to end up with an empty state. This is known as **AOF Bloat**.

To solve this, production engines (like Redis) implement an **AOF Rewrite** mechanism (`BGREWRITEAOF`). When the `.aof` file grows too large, the server forks a background process that completely ignores the bloated history file. Instead, it looks only at the current state of the RAM and generates a brand new, microscopic `.aof` file containing only the exact `SET` commands needed to reconstruct the surviving data. It then safely swaps the files, ensuring near-instantaneous crash recovery regardless of server uptime.

---

## 12. The Interactive CLI (Command Line Interface)
To fully replicate the professional database experience, this project includes a custom-built interactive client (`flashdb-cli`). 

Rather than relying on automated PowerShell scripts or third-party diagnostic GUIs, the `flashdb-cli` is a dedicated C++ executable that establishes a persistent TCP stream with the database engine. It acts as a REPL (Read-Eval-Print Loop), intercepting human-readable commands (e.g., `SET`, `GET`), wrapping them in the RESP network termination sequence (`\r\n`), and streaming them to the server. By managing its own Windows socket (`WSADATA`) and maintaining a continuous `recv()` loop, the client provides a lightning-fast, zero-latency prompt that perfectly mimics the behavior of `redis-cli` or `mysql -u root`.
