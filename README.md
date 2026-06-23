# Meshtalk - P2P Terminal Chat

A fully decentralized peer-to-peer terminal chat application for LANs. No central server required.

## Quick Start Guide

### Prerequisites

**Linux:**
```bash
sudo apt install build-essential cmake libncurses-dev
# or: sudo dnf install gcc cmake ncurses-devel
```

**macOS:**
```bash
xcode-select --install          # compilers
brew install cmake               # build system
# ncurses is included with macOS
```

**Windows (10/11):**
- Install [Visual Studio 2022](https://visualstudio.microsoft.com/) with "Desktop development with C++"
- Install [CMake](https://cmake.org/download/) (add to PATH)
- PDCurses is optional (falls back to Console API)

### Build

```bash
# Standard build (creates build directory automatically)
cmake -B build

# Compile
cmake --build build

# Binary is at build/meshtalk (or build/meshtalk.exe on Windows)
```

**Options** (add to the `cmake -B build` line):

| Flag | Default | Purpose |
|------|---------|---------|
| `-DENABLE_E2EE=OFF` | ON | Disable libsodium E2EE |
| `-DENABLE_TLS=ON` | OFF | Enable OpenSSL TLS |
| `-DENABLE_ASAN=ON` | OFF | AddressSanitizer |
| `-DBUILD_TESTS=ON` | OFF | Build tests |

Example: `cmake -B build -DENABLE_TLS=ON -DBUILD_TESTS=ON`

Pre-built binaries for all platforms are on the [releases page](https://github.com/pastUnknownUser/Meshtalk/releases):

| Platform | Binary |
|----------|--------|
| Linux x86_64 | `meshtalk-linux-x64` |
| Linux ARM64 | `meshtalk-linux-arm64` |
| macOS ARM64 | `meshtalk-darwin-arm64` |
| Windows x86_64 | `meshtalk-windows-x64.exe` |
| Windows ARM64 | `meshtalk-windows-arm64.exe` |

### Run

```bash
# Start the chat application
./meshtalk
```

**First run:** A unique node ID is generated and saved to `~/.meshtalk/meshtalk.json`. Your default username is "anonymous". Change it with `/nick yourname`.

**On a LAN:** Just launch the app on multiple machines. They will discover each other automatically within 5 seconds. No configuration needed.

### Commands

| Command                            | Description                              |
|------------------------------------|------------------------------------------|
| `/help`                            | Show all commands                        |
| `/nick <name>`                     | Set your display name                    |
| `/join <room>`                     | Join a chat room                         |
| `/create <room>`                   | Create a new room                        |
| `/leave`                           | Leave the current room                   |
| `/msg <user> <text>`               | Send a private message (E2EE if key known)|
| `/users`                           | List online users                        |
| `/history [n]`                     | Show last n messages (default: 50)       |
| `/ping`                            | Ping all connected peers                 |
| `/quit` or `/exit`                 | Exit the application                     |
| `/e2ee_create <room>`              | Create an encrypted room                 |
| `/e2ee_invite <user> <room>`       | Invite a user to an encrypted room       |
| `/trust <user>`                    | Trust a peer's public key                |
| `/key`                             | Show your public key                     |

### Multi-Machine Test

1. Build on two machines on the same LAN
2. Run `./meshtalk` on both
3. Wait ~5 seconds for discovery
4. Type a message and press Enter — it appears on both

### Single-Machine Test

Build once, then run two instances (change port for the second):

```bash
cmake -B build
cmake --build build
# Terminal 1
./build/meshtalk
# Type: /nick Alice

# Terminal 2 (edit TCP_PORT in src/common.h:22 first, rebuild)
# Or just use a VM/container for the second instance
```

## Run Tests

```bash
# Build tests (ASan recommended)
cmake -B build -DBUILD_TESTS=ON -DENABLE_ASAN=ON
cmake --build build

# Run all tests
ctest --test-dir build --output-on-failure

# Or run individual tests:
build/tests/test_message
build/tests/test_peer
build/tests/test_discovery
build/tests/test_net
build/tests/test_simulator   # 100 virtual nodes, 30s
```

## Architecture

```
┌─────────────────────────────────────────────┐
│                Meshtalk Node                 │
│                                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │   TUI    │  │  Router  │  │  Rooms   │   │
│  │ (ncurses)│  │ (dedup + │  │ (chat    │   │
│  │          │  │  flood)  │  │  rooms)  │   │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘   │
│       │              │             │         │
│  ┌────┴──────────────┴─────────────┴─────┐   │
│  │           Peer Manager                 │   │
│  │  (connections, reconnects, timeouts)   │   │
│  └────────────────┬───────────────────────┘   │
│       ┌───────────┴───────────┐              │
│  ┌────┴─────┐           ┌─────┴────┐         │
│  │  TCP     │           │  UDP     │         │
│  │ Listener │           │ Discovery│         │
│  └────┬─────┘           └────┬─────┘         │
│       │                      │               │
│  ┌────┴──────────────────────┴─────┐         │
│  │   Network Abstraction Layer     │         │
│  │   (BSD sockets / Winsock2)      │         │
│  └──────────────────────────────────┘         │
└──────────────────────────────────────────────┘
```

### Key Concepts

- **No central server** — every instance is simultaneously a client and server
- **LAN Discovery** — UDP broadcast on port 40000 every 5 seconds
- **Automatic connection** — discovered peers are connected via TCP on port 40001
- **Flood routing** — messages forward to all connected peers (except sender)
- **Duplicate suppression** — UUID message IDs cached for 60s, loops prevented
- **Self-healing** — disconnected peers are reconnected automatically every 10s

### Ports

| Port  | Protocol | Purpose        |
|-------|----------|----------------|
| 40000 | UDP      | Peer discovery |
| 40001 | TCP      | Chat messages  |

## Protocol

See [docs/protocol.md](docs/protocol.md) for the full wire protocol specification.

## File Structure

```
meshtalk/
├── CMakeLists.txt          # Build system
├── README.md               # This file
├── docs/
│   ├── protocol.md         # Wire protocol spec
│   └── architecture.md     # Architecture docs
├── src/
│   ├── main.c              # Entry point
│   ├── common.h            # Shared types and constants
│   ├── net/                # Network abstraction (BSD/Winsock)
│   ├── util/               # UUID and JSON utilities
│   ├── message/            # Message serialization & dedup
│   ├── peer/               # Peer list & connection management
│   ├── discovery/          # UDP LAN discovery
│   ├── room/               # Chat room management
│   ├── persistence/        # Config file I/O
│   ├── crypto/             # Optional E2EE (libsodium) + TLS (OpenSSL)
│   └── tui/                # Terminal UI
└── tests/
    ├── CMakeLists.txt       # Test build config
    ├── test_net.c           # Network layer tests
    ├── test_message.c       # Serialization & dedup tests
    ├── test_discovery.c     # Discovery protocol tests
    ├── test_peer.c          # Peer management tests
    └── test_simulator.c     # 100-node network simulator
```

## License

MIT
