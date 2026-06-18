#ifndef MESHTALK_MESSAGE_H
#define MESHTALK_MESSAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Protocol constants */
#define MT_MAX_PAYLOAD        65536  /* 64 KB max packet */
#define MT_MAX_USERNAME_LEN   64
#define MT_MAX_ROOM_LEN       128
#define MT_MAX_NODE_ID_LEN    37     /* UUID + null */
#define MT_MAX_MESSAGE_ID_LEN 37     /* UUID + null */
#define MT_MAX_TEXT_LEN       4096
#define MT_DEFAULT_TCP_PORT   40001
#define MT_DISCOVERY_PORT     40000
#define MT_MESSAGE_TTL        10     /* Max hops to prevent loops */
#define MT_HEARTBEAT_INTERVAL 15     /* Seconds */
#define MT_PEER_TIMEOUT       45     /* Seconds before peer considered dead */

/* Frame header: [4B length][1B type][N bytes payload] */
#define MT_FRAME_HEADER_SIZE  5

/* Message types */
typedef enum {
    MT_TYPE_DISCOVER   = 0x01,
    MT_TYPE_HERE       = 0x02,
    MT_TYPE_CHAT       = 0x03,
    MT_TYPE_PRIVATE    = 0x04,
    MT_TYPE_ROOM_JOIN  = 0x05,
    MT_TYPE_ROOM_LEAVE = 0x06,
    MT_TYPE_USER_JOIN  = 0x07,
    MT_TYPE_USER_LEAVE = 0x08,
    MT_TYPE_HEARTBEAT  = 0x09,
    MT_TYPE_ACK        = 0x0A,
    MT_TYPE_PEER_LIST  = 0x0B,
    MT_TYPE_ROUTE_ERROR= 0x0C
} mt_message_type_t;

/* Binary protocol structures */

/* DISCOVER / HERE (UDP) */
typedef struct {
    char node_id[MT_MAX_NODE_ID_LEN];
    char username[MT_MAX_USERNAME_LEN];
    uint16_t tcp_port;
} mt_discover_t;

/* CHAT message (flood forwarded) */
typedef struct {
    char message_id[MT_MAX_MESSAGE_ID_LEN];
    char sender_id[MT_MAX_NODE_ID_LEN];
    char sender_name[MT_MAX_USERNAME_LEN];
    char room[MT_MAX_ROOM_LEN];
    uint32_t timestamp;
    uint8_t ttl;
    char payload[MT_MAX_TEXT_LEN];
} mt_chat_t;

/* PRIVATE message (routed) */
typedef struct {
    char message_id[MT_MAX_MESSAGE_ID_LEN];
    char sender_id[MT_MAX_NODE_ID_LEN];
    char sender_name[MT_MAX_USERNAME_LEN];
    char dest_id[MT_MAX_NODE_ID_LEN];
    uint32_t timestamp;
    uint8_t ttl;
    char payload[MT_MAX_TEXT_LEN];
} mt_private_t;

/* ROOM_JOIN / ROOM_LEAVE */
typedef struct {
    char node_id[MT_MAX_NODE_ID_LEN];
    char username[MT_MAX_USERNAME_LEN];
    char room[MT_MAX_ROOM_LEN];
    uint32_t timestamp;
} mt_room_event_t;

/* USER_JOIN / USER_LEAVE */
typedef struct {
    char node_id[MT_MAX_NODE_ID_LEN];
    char username[MT_MAX_USERNAME_LEN];
    uint32_t timestamp;
} mt_user_event_t;

/* HEARTBEAT */
typedef struct {
    char node_id[MT_MAX_NODE_ID_LEN];
    uint32_t timestamp;
} mt_heartbeat_t;

/* ACK */
typedef struct {
    char message_id[MT_MAX_MESSAGE_ID_LEN];
} mt_ack_t;

/* PEER_LIST (sent to new connections) */
typedef struct {
    uint16_t peer_count;
} mt_peer_list_header_t;

typedef struct {
    char node_id[MT_MAX_NODE_ID_LEN];
    char username[MT_MAX_USERNAME_LEN];
    char ip_addr[46];  /* IPv6 max length + null */
    uint16_t tcp_port;
} mt_peer_entry_t;

/* ROUTE_ERROR */
typedef struct {
    char message_id[MT_MAX_MESSAGE_ID_LEN];
    char dest_id[MT_MAX_NODE_ID_LEN];
    char reason[64];
} mt_route_error_t;

/* Generic frame (what travels on the wire) */
typedef struct {
    uint32_t length;        /* Total payload length (excluding header) */
    mt_message_type_t type;
    uint8_t payload[MT_MAX_PAYLOAD];
} mt_frame_t;

/* API */

/* Serialization: struct -> frame payload */
int mt_serialize(mt_frame_t *frame, mt_message_type_t type, const void *data, size_t data_len);

/* Deserialization: frame payload -> struct */
int mt_deserialize(const mt_frame_t *frame, mt_message_type_t type, void *data, size_t data_len);

/* Helper: build specific message types */
int mt_build_discover(mt_frame_t *frame, const char *node_id, const char *username, uint16_t tcp_port);
int mt_build_here(mt_frame_t *frame, const char *node_id, const char *username, uint16_t tcp_port);
int mt_build_chat(mt_frame_t *frame, const char *message_id, const char *sender_id,
                  const char *sender_name, const char *room, uint32_t timestamp,
                  uint8_t ttl, const char *payload);
int mt_build_private(mt_frame_t *frame, const char *message_id, const char *sender_id,
                     const char *sender_name, const char *dest_id, uint32_t timestamp,
                     uint8_t ttl, const char *payload);
int mt_build_room_join(mt_frame_t *frame, const char *node_id, const char *username,
                       const char *room, uint32_t timestamp);
int mt_build_room_leave(mt_frame_t *frame, const char *node_id, const char *username,
                        const char *room, uint32_t timestamp);
int mt_build_user_join(mt_frame_t *frame, const char *node_id, const char *username,
                       uint32_t timestamp);
int mt_build_user_leave(mt_frame_t *frame, const char *node_id, const char *username,
                        uint32_t timestamp);
int mt_build_heartbeat(mt_frame_t *frame, const char *node_id, uint32_t timestamp);
int mt_build_ack(mt_frame_t *frame, const char *message_id);
int mt_build_peer_list(mt_frame_t *frame, const mt_peer_entry_t *peers, uint16_t count);
int mt_build_route_error(mt_frame_t *frame, const char *message_id, const char *dest_id,
                         const char *reason);

/* Parse specific message types from frame */
int mt_parse_discover(const mt_frame_t *frame, mt_discover_t *out);
int mt_parse_here(const mt_frame_t *frame, mt_discover_t *out);
int mt_parse_chat(const mt_frame_t *frame, mt_chat_t *out);
int mt_parse_private(const mt_frame_t *frame, mt_private_t *out);
int mt_parse_room_event(const mt_frame_t *frame, mt_room_event_t *out);
int mt_parse_user_event(const mt_frame_t *frame, mt_user_event_t *out);
int mt_parse_heartbeat(const mt_frame_t *frame, mt_heartbeat_t *out);
int mt_parse_ack(const mt_frame_t *frame, mt_ack_t *out);
int mt_parse_peer_list(const mt_frame_t *frame, mt_peer_entry_t *out, uint16_t *count);
int mt_parse_route_error(const mt_frame_t *frame, mt_route_error_t *out);

/* Generate unique IDs */
void mt_generate_uuid(char *out, size_t out_len);

/* Timestamp helper */
uint32_t mt_timestamp_now(void);

/* Type name for logging */
const char *mt_type_name(mt_message_type_t type);

#endif /* MESHTALK_MESSAGE_H */
