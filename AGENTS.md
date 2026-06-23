# Meshtalk — AGENTS.md

## Build & test

```bash
cmake -B build                                    # configure
cmake --build build                               # compile → build/meshtalk
ctest --test-dir build --output-on-failure        # run all tests (needs -DBUILD_TESTS=ON)
build/tests/test_simulator                        # 100-node sim, 30s
```

| Option | Default | Purpose |
|--------|---------|---------|
| `-DBUILD_TESTS=ON` | OFF | Build tests |
| `-DENABLE_ASAN=ON` | OFF | AddressSanitizer |
| `-DENABLE_E2EE=ON` | ON | libsodium E2EE |
| `-DENABLE_TLS=ON` | OFF | OpenSSL TLS |

Add options to the configure line, e.g. `cmake -B build -DBUILD_TESTS=ON -DENABLE_ASAN=ON`.

No linter, formatter, or CI config exists. Compiler warnings via `-Wall -Wextra -pedantic` baked into CMakeLists.txt.

Pre-built binaries for all platforms are on the [releases page](https://github.com/pastUnknownUser/Meshtalk/releases):

| Platform | Binary |
|----------|--------|
| Linux x86_64 | `meshtalk-linux-x64` |
| Linux ARM64 | `meshtalk-linux-arm64` |
| macOS ARM64 | `meshtalk-darwin-arm64` |
| Windows x86_64 | `meshtalk-windows-x64.exe` |
| Windows ARM64 | `meshtalk-windows-arm64.exe` |

Cross-compilation uses [zig cc](https://ziglang.org/) (e.g. `zig cc -target x86_64-windows-gnu`).
See `CMakeLists.txt` for platform sources and linker flags.

## Architecture

Decentralized P2P LAN chat. Every node is both client + server. No central server.

- **UDP port 40000** — LAN broadcast peer discovery (`DISCOVER`/`HERE` text protocol)
- **TCP port 40001** — message exchange (length-prefixed JSON, 4-byte network-order length + JSON payload)
- **Flood routing** — forward every message to all connected peers except the sender
- **Dedup** — LRU cache of 2000 UUIDs, 60s TTL
- **Message history** — last 1000 messages per room in memory
- **Wire protocol** — JSON over TCP, max packet 65536 bytes, full spec in `docs/protocol.md`

Entry point: `src/main.c:938` — `app_init()` → subsystems → threads → event loop.

## Key directories

| Path | Purpose |
|------|---------|
| `src/main.c` | Entry point, event loop, `/` commands |
| `src/common.h` | All shared types, constants, message type enum, ports |
| `src/net/net_posix.c` / `net_win.c` | Socket abstraction (compile-time selected) |
| `src/tui/tui_posix.c` / `tui_win.c` | Terminal UI (ncurses / Console API) |
| `src/message/message.c` | Serialization, dedup cache, flood routing |
| `src/peer/peer.c` | Peer list, connection lifecycle, reconnection |
| `src/discovery/discovery.c` | UDP broadcast discovery |
| `src/room/room.c` | Chat rooms, presence, message history |
| `src/crypto/crypto.c` | libsodium XChaCha20-Poly1305 + Box |
| `src/persistence/storage.c` | JSON file I/O for config/peers/rooms/keys |
| `src/util/json.c` | Minimal hand-rolled JSON (no external dep) |
| `src/util/uuid.c` | UUIDv4 generation |

## Threading model

| Thread | Role |
|--------|------|
| Main/TUI | Input, event processing, drawing |
| Discovery broadcast | Sends UDP `DISCOVER` every 5s |
| Discovery listener | Listens for UDP `HERE` responses |
| TCP listener | Accepts incoming connections |
| Per-peer handler | One per connected peer, handles I/O |
| Connector | Retries disconnected peers every 10s |

## Platform quirks

- **Single-machine testing**: edit `TCP_PORT` in `src/common.h:22` before building the second instance; use a second build directory or rebuild after the change
- **Linux** requires `libncurses-dev`, optional `libsodium-dev`, `libssl-dev`
- **macOS** ncurses is included; libsodium via `brew install libsodium`
- **Windows** needs CMake + Visual Studio; falls back to Console API if PDCurses unavailable
- `_WIN32_WINNT=0x0601` and `WIN32_LEAN_AND_MEAN` set for Windows builds
- **Config**: `~/.meshtalk/meshtalk.json` stores node ID/username; peers, rooms, and keys in adjacent files

## Conventions

- C17, `meshtalk` executable, MIT license
- `message_t` struct in `common.h` is the single wire-format data structure
- All `/` commands handled in `main.c:process_command()`
- All message types processed in `main.c:process_message()`
- No external JSON or UUID libraries; hand-rolled implementations in `src/util/`
