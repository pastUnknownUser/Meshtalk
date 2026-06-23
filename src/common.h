#pragma once

#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200112L
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define UUID_STR_LEN      36
#define MAX_USERNAME_LEN  32
#define MAX_ROOM_LEN      64
#define MAX_PAYLOAD_LEN   65536
#define MAX_PACKET_SIZE   65536
#define DISCOVERY_PORT    40000
#define DISCOVERY_INTERVAL_MS 5000
#define TCP_PORT          40001
#define MAX_PEERS         200
#define MSG_CACHE_SIZE    2000
#define MSG_CACHE_TTL_MS  60000
#define RECONNECT_INTERVAL_MS 10000
#define MAX_MESSAGE_HISTORY 1000
#define PING_INTERVAL_MS  15000
#define PEER_TIMEOUT_MS   60000
#define MAX_LINE_LEN      4096

typedef enum {
    MSG_UNKNOWN = 0,
    MSG_CHAT = 1,
    MSG_PRIV = 2,
    MSG_JOIN = 3,
    MSG_LEAVE = 4,
    MSG_CREATE_ROOM = 5,
    MSG_PEER_LIST = 6,
    MSG_PRESENCE = 7,
    MSG_PING = 8,
    MSG_PONG = 9,
    MSG_INFO = 10,
    KEY_EXCHANGE = 20,
    PRIV_ENC = 21,
    ROOM_KEY = 22,
    ROOM_ENC = 23,
} message_type_t;

#define BASE64_PK_LEN     48
#define BASE64_SK_LEN     96
#define BASE64_ENCKEY_LEN 128
#define BASE64_NONCE_LEN  48

typedef struct {
    char msg_id[UUID_STR_LEN + 1];
    char sender_id[UUID_STR_LEN + 1];
    char sender_name[MAX_USERNAME_LEN + 1];
    uint64_t timestamp_ms;
    message_type_t type;
    char room[MAX_ROOM_LEN + 1];
    char target_id[UUID_STR_LEN + 1];
    char payload[MAX_PAYLOAD_LEN];
    char public_key[BASE64_PK_LEN];
    char encrypted_key[BASE64_ENCKEY_LEN];
    char nonce[BASE64_NONCE_LEN];
} message_t;

typedef struct message_node_s {
    message_t msg;
    uint64_t received_ms;
    struct message_node_s* next;
} message_node_t;

typedef struct {
    message_node_t* head;
    message_node_t* tail;
    int count;
} message_list_t;

#ifdef _WIN32
    #define MSCT_EXPORT __declspec(dllexport)
#else
    #define MSCT_EXPORT
#endif

#ifdef _WIN32
static inline uint64_t now_ms(void) {
    return (uint64_t)GetTickCount64();
}
static inline void msleep(uint32_t ms) {
    Sleep(ms);
}
#else
static inline uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}
static inline void msleep(uint32_t ms) {
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000 };
    nanosleep(&ts, NULL);
}
#endif
