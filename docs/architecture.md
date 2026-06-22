# Meshtalk Architecture

## Overview

Meshtalk is a fully decentralized peer-to-peer terminal chat application. There is no central server. Every node acts as both a client and a server.

## Architecture Diagram

```
+------------------------------------------------------+
|                    Meshtalk Node                       |
|                                                       |
|  +------------------+  +---------------------------+  |
|  |   Terminal UI    |  |     Event/Msg Queue       |  |
|  |  (ncurses/Win32) |  |                           |  |
|  +--------+---------+  +------------+--------------+  |
|           |                          |                 |
|  +--------+---------+  +------------+--------------+  |
|  |   Main Loop      |  |    Message Router        |  |
|  |   (event driven) |  |   (dedup + flood)        |  |
|  +--------+---------+  +------------+--------------+  |
|           |                          |                 |
|  +--------+---------+  +------------+--------------+  |
|  |   Room Manager   |  |    Peer Manager          |  |
|  +------------------+  +------------+--------------+  |
|                                     |                 |
|  +------------------+  +------------+--------------+  |
|  |   Persistence    |  |    Discovery (UDP)       |  |
|  |   (JSON files)   |  |    + TCP Listener        |  |
|  +------------------+  +---------------------------+  |
|                                                       |
|  +-------------------------------------------------+  |
|  |          Network Abstraction Layer               |  |
|  |  (POSIX sockets / Winsock2)                      |  |
|  +-------------------------------------------------+  |
+------------------------------------------------------+
```

## Module Overview

### 1. Network Abstraction Layer (`src/net/`)

Provides a uniform API across Windows (Winsock2) and POSIX (BSD sockets).

**Functions:**
- `net_init()` / `net_cleanup()` - Platform init/shutdown
- `net_tcp_listen()` / `net_tcp_accept()` - TCP server
- `net_tcp_connect()` - TCP client
- `net_send()` / `net_recv()` - Data I/O
- `net_udp_broadcast_socket()` / `net_udp_listen_socket()` - UDP sockets
- `net_udp_send()` / `net_udp_recv()` - UDP I/O

**Platform-specific files:**
- `net_posix.c` - Linux/macOS implementation
- `net_win.c` - Windows implementation

### 2. Peer Discovery (`src/discovery/`)

Implements automatic LAN peer discovery using UDP broadcast.

- Broadcasts `DISCOVER` messages every 5 seconds
- Responds to `DISCOVER` with `HERE` message
- Automatically adds discovered peers to the peer list
- Ignores self-discovery

### 3. Peer Manager (`src/peer/`)

Manages the list of known peers and their connection state.

- Thread-safe linked list of peers
- Connection state tracking (connecting, connected, disconnected)
- Automatic reconnection logic
- Timeout detection (60s inactivity)

### 4. Message System (`src/message/`)

Handles message serialization, deserialization, and routing.

- JSON-based message format
- UUID-based message IDs for deduplication
- LRU cache for duplicate suppression (2000 entries, 60s TTL)
- Flood routing to all connected peers

### 5. Room Manager (`src/room/`)

Manages chat rooms.

- `/join`, `/create`, `/leave` commands
- Message history per room (last 1000 messages)
- Room presence notifications

### 6. Terminal UI (`src/tui/`)

Provides the user interface.

- **POSIX:** ncurses-based split-panel layout
- **Windows:** Console API-based interface

Layout:
```
+---------------------------+
| Messages           |Users |
|                    |      |
+---------------------------+
| Input                     |
+---------------------------+
```

### 7. Persistence (`src/persistence/`)

Stores configuration and data to the filesystem.

- `~/.meshtalk/meshtalk.json` - Node ID and username
- `~/.meshtalk/peers.json` - Known peer list
- Configuration is persisted between launches

### 8. Crypto (`src/crypto/`)

Optional TLS encryption layer using OpenSSL.

- Enabled with `-DENABLE_TLS=ON`
- Wraps TCP connections with SSL/TLS

## Threading Model

| Thread               | Purpose                                      |
|----------------------|----------------------------------------------|
| Main/TUI             | User input, event processing, UI drawing    |
| Discovery Broadcast  | Sends UDP DISCOVER every 5s                 |
| Discovery Listener   | Listens for UDP responses                   |
| TCP Listener         | Accepts incoming connections                |
| Connection Handler   | One per peer connection, handles I/O        |
| Connector            | Manages outgoing reconnections              |

## Thread Safety

- Global peer list is protected by `g_peers_lock` mutex
- Room list is protected by `g_rooms_lock` mutex
- Message cache uses lock-free hashing for lookups
- Event queue uses mutex + condition variable

## Data Flow

### Sending a message:
1. User types message in TUI
2. Main thread creates `message_t` with UUID
3. Adds to duplicate cache
4. Serializes to JSON
5. Floods to all connected peers

### Receiving a message:
1. Peer handler thread reads length-prefixed JSON
2. Deserializes into `message_t`
3. Pushes to event queue
4. Main thread pops event from queue
5. Checks duplicate cache
6. If new: displays in TUI, adds to room history
7. Forwards to all other connected peers

## Build Configuration

```bash
# Standard build
mkdir build && cd build
cmake ..
make

# With AddressSanitizer
cmake .. -DENABLE_ASAN=ON
make

# With TLS support
cmake .. -DENABLE_TLS=ON
make

# With tests
cmake .. -DBUILD_TESTS=ON
make
```

## Dependencies

| Platform  | Dependency       | Purpose        |
|-----------|------------------|----------------|
| All       | C17 compiler     | Compilation    |
| Linux     | ncurses          | Terminal UI    |
| macOS     | ncurses          | Terminal UI    |
| Windows   | Windows SDK      | Sockets, Console |
| Optional  | OpenSSL          | TLS encryption |
