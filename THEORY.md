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
