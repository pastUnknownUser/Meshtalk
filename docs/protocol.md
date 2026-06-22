# Meshtalk Protocol Documentation

## Overview
Meshtalk uses a JSON-based text protocol over TCP for peer-to-peer communication and a simple text-based protocol over UDP for LAN discovery.

## Network Layers

### 1. Peer Discovery (UDP)

**Broadcast Port:** 40000

**Discovery Message Format:**
```
DISCOVER\n
<username>\n
<tcp_port>\n
<node_id>\n
```

**Response Message Format:**
```
HERE\n
<username>\n
<tcp_port>\n
<node_id>\n
```

Nodes broadcast `DISCOVER` every 5 seconds. Receiving nodes respond with `HERE` and automatically add the sender to their peer list.

### 2. TCP Communication Protocol

Once peers discover each other via UDP, they establish TCP connections.

**Connection Handshake:**
1. Connecting node sends its identity as a JSON message
2. Accepting node responds with its identity JSON message
3. Both sides store the connection

**Wire Format (length-prefixed JSON):**
```
[4 bytes: message length in network byte order]
[N bytes: JSON message (length bytes, not including the 4-byte header)]
```

**Maximum packet size:** 65,536 bytes (64KB)

### 3. Message Format (JSON)

All messages use the following JSON structure:

```json
{
    "type": <int>,
    "id": "<uuid>",
    "sender_id": "<uuid>",
    "sender_name": "<string>",
    "time": <uint64>,
    "room": "<string>",
    "target": "<uuid>",
    "payload": "<string>"
}
```

### 4. Message Types

| Type    | Value | Description                    |
|---------|-------|--------------------------------|
| CHAT    | 1     | Public chat message            |
| PRIV    | 2     | Private message                |
| JOIN    | 3     | Room join notification         |
| LEAVE   | 4     | Room leave notification        |
| CREATE  | 5     | Room creation                  |
| PEERS   | 6     | Peer list exchange             |
| PRES    | 7     | Presence notification          |
| PING    | 8     | Keepalive ping                 |
| PONG    | 9     | Keepalive response             |
| INFO    | 10    | Informational/system message   |

### 5. Duplicate Suppression

Each message carries a globally unique UUID (`id` field). Nodes maintain an LRU cache of up to 2,000 seen message IDs with a TTL of 60 seconds. Duplicate messages are silently dropped and not forwarded.

### 6. Message Flooding

When a node receives a new (non-duplicate) message, it forwards it to all connected peers except the sender. This ensures network-wide propagation without requiring routing tables.

### 7. Private Message Routing

Private messages (`MSG_PRIV`) include a `target` field with the recipient's node ID. Each node that receives a private message checks if it is the target. If not, it forwards the message to all other connected peers (flood routing).

## Port Usage

| Port  | Protocol | Purpose          |
|-------|----------|------------------|
| 40000 | UDP      | Peer discovery   |
| 40001 | TCP      | Message exchange |

## Security Considerations

- Maximum packet size is enforced (64KB)
- Usernames are validated (alphanumeric, underscore, dash, space)
- Node IDs are validated as UUIDv4
- Malformed packets are rejected
- Optional TLS encryption via OpenSSL
