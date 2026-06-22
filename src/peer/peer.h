#pragma once

#include "../common.h"
#include "../net/net.h"
#include <pthread.h>

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 64
#endif

typedef struct peer_s {
    char node_id[UUID_STR_LEN + 1];
    char username[MAX_USERNAME_LEN + 1];
    char public_key[BASE64_PK_LEN];
    char addr[INET6_ADDRSTRLEN];
    int port;
    net_socket_t sock;
    volatile bool connected;
    uint64_t last_seen_ms;
    uint64_t last_ping_ms;
    pthread_t thread;
    pthread_mutex_t lock;
    bool outgoing;
    struct peer_s* next;
} peer_t;

extern pthread_mutex_t g_peers_lock;

void peer_init(void);
void peer_cleanup(void);

/* Low-level: caller MUST hold g_peers_lock */
peer_t* peer_find(const char* node_id);
peer_t* peer_find_by_sock(net_socket_t sock);
int     peer_find_index(const char* node_id);
int     peer_count(void);
peer_t* peer_get(int index);

/* High-level: lock internally, safe to call from any thread */
bool    peer_exists(const char* node_id);
bool    peer_get_username(const char* node_id, char* out, size_t size);
bool    peer_is_connected(const char* node_id);
int     peer_add(const char* node_id, const char* username, const char* addr, int port);
int     peer_add_connection(const char* node_id, const char* username, net_socket_t sock, bool outgoing);
void    peer_remove(int index);
void    peer_remove_by_id(const char* node_id);
void    peer_set_username(const char* node_id, const char* username);
void    peer_set_public_key(const char* node_id, const char* public_key);
bool    peer_get_public_key(const char* node_id, char* out, size_t size);
void    peer_set_connected(const char* node_id, bool connected);
int     peer_connect_to(const char* addr, int port);
void    peer_disconnect(int index);
void    peer_disconnect_all(void);
void    peer_check_timeouts(void);

/* Lock management */
void peer_lock(void);
void peer_unlock(void);
void peer_lock_specific(peer_t* p);
void peer_unlock_specific(peer_t* p);

/* Iteration */
typedef void (*peer_callback_t)(peer_t* p, void* userdata);
void peer_for_each(peer_callback_t cb, void* userdata);
