#include "peer.h"
#include "../message/message.h"
#include "../net/net.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

pthread_mutex_t g_peers_lock = PTHREAD_MUTEX_INITIALIZER;

static peer_t* g_peers = NULL;
static int g_peer_count = 0;

/* All low-level functions assume caller holds g_peers_lock */

peer_t* peer_find(const char* node_id) {
    if (!node_id || !*node_id) return NULL;
    peer_t* cur = g_peers;
    while (cur) {
        if (strcmp(cur->node_id, node_id) == 0) return cur;
        cur = cur->next;
    }
    return NULL;
}

peer_t* peer_find_by_sock(net_socket_t sock) {
    peer_t* cur = g_peers;
    while (cur) {
        if (cur->sock == sock) return cur;
        cur = cur->next;
    }
    return NULL;
}

int peer_find_index(const char* node_id) {
    int i = 0;
    peer_t* cur = g_peers;
    while (cur) {
        if (strcmp(cur->node_id, node_id) == 0) return i;
        cur = cur->next;
        i++;
    }
    return -1;
}

int peer_count(void) {
    int c;
    peer_lock();
    c = g_peer_count;
    peer_unlock();
    return c;
}

peer_t* peer_get(int index) {
    int i = 0;
    peer_t* cur = g_peers;
    while (cur) {
        if (i == index) return cur;
        cur = cur->next;
        i++;
    }
    return NULL;
}

/* Locked helpers */

bool peer_exists(const char* node_id) {
    bool found = false;
    peer_lock();
    if (peer_find(node_id)) found = true;
    peer_unlock();
    return found;
}

bool peer_get_username(const char* node_id, char* out, size_t size) {
    peer_lock();
    peer_t* p = peer_find(node_id);
    if (p && size > 0) {
        strncpy(out, p->username, size - 1);
        out[size - 1] = '\0';
        peer_unlock();
        return true;
    }
    peer_unlock();
    return false;
}

bool peer_is_connected(const char* node_id) {
    bool c = false;
    peer_lock();
    peer_t* p = peer_find(node_id);
    if (p && p->connected) c = true;
    peer_unlock();
    return c;
}

/* Lifecycle */

void peer_init(void) {
    g_peers = NULL;
    g_peer_count = 0;
}

void peer_cleanup(void) {
    peer_disconnect_all();
    peer_lock();
    peer_t* cur = g_peers;
    while (cur) {
        peer_t* tmp = cur;
        cur = cur->next;
        pthread_mutex_destroy(&tmp->lock);
        if (tmp->sock != INVALID_NET_SOCKET) net_close(tmp->sock);
        free(tmp);
    }
    g_peers = NULL;
    g_peer_count = 0;
    peer_unlock();
}

int peer_add(const char* node_id, const char* username, const char* addr, int port) {
    if (!node_id || !addr) return -1;

    peer_lock();
    if (peer_find(node_id)) { peer_unlock(); return 0; }
    peer_unlock();

    peer_t* p = (peer_t*)calloc(1, sizeof(peer_t));
    strncpy(p->node_id, node_id, UUID_STR_LEN);
    p->node_id[UUID_STR_LEN] = '\0';
    if (username) strncpy(p->username, username, MAX_USERNAME_LEN);
    strncpy(p->addr, addr, INET6_ADDRSTRLEN - 1);
    p->port = port;
    p->sock = INVALID_NET_SOCKET;
    p->connected = false;
    p->last_seen_ms = now_ms();
    p->last_ping_ms = 0;
    pthread_mutex_init(&p->lock, NULL);

    peer_lock();
    p->next = g_peers;
    g_peers = p;
    g_peer_count++;
    peer_unlock();
    return 0;
}

int peer_add_connection(const char* node_id, const char* username, net_socket_t sock, bool outgoing) {
    peer_lock();
    peer_t* p = peer_find(node_id);
    if (p) {
        if (p->connected) {
            net_close(sock);
            peer_unlock();
            return 0;
        }
        p->sock = sock;
        p->connected = true;
        p->outgoing = outgoing;
        p->last_seen_ms = now_ms();
        peer_unlock();
        return 0;
    }
    peer_unlock();

    p = (peer_t*)calloc(1, sizeof(peer_t));
    strncpy(p->node_id, node_id, UUID_STR_LEN);
    p->node_id[UUID_STR_LEN] = '\0';
    if (username) strncpy(p->username, username, MAX_USERNAME_LEN);
    p->sock = sock;
    p->connected = true;
    p->outgoing = outgoing;
    p->last_seen_ms = now_ms();
    p->last_ping_ms = 0;
    pthread_mutex_init(&p->lock, NULL);

    peer_lock();
    p->next = g_peers;
    g_peers = p;
    g_peer_count++;
    peer_unlock();
    return 0;
}

void peer_remove(int index) {
    peer_lock();
    int i = 0;
    peer_t** pp = &g_peers;
    while (*pp) {
        if (i == index) {
            peer_t* tmp = *pp;
            *pp = tmp->next;
            if (tmp->sock != INVALID_NET_SOCKET) net_close(tmp->sock);
            pthread_mutex_destroy(&tmp->lock);
            free(tmp);
            g_peer_count--;
            peer_unlock();
            return;
        }
        pp = &(*pp)->next;
        i++;
    }
    peer_unlock();
}

void peer_remove_by_id(const char* node_id) {
    peer_lock();
    peer_t** pp = &g_peers;
    while (*pp) {
        if (strcmp((*pp)->node_id, node_id) == 0) {
            peer_t* tmp = *pp;
            *pp = tmp->next;
            if (tmp->sock != INVALID_NET_SOCKET) net_close(tmp->sock);
            pthread_mutex_destroy(&tmp->lock);
            free(tmp);
            g_peer_count--;
            peer_unlock();
            return;
        }
        pp = &(*pp)->next;
    }
    peer_unlock();
}

void peer_set_username(const char* node_id, const char* username) {
    if (!username) return;
    peer_lock();
    peer_t* p = peer_find(node_id);
    if (p) strncpy(p->username, username, MAX_USERNAME_LEN);
    peer_unlock();
}

void peer_set_public_key(const char* node_id, const char* public_key) {
    if (!public_key) return;
    peer_lock();
    peer_t* p = peer_find(node_id);
    if (p) {
        strncpy(p->public_key, public_key, BASE64_PK_LEN - 1);
        p->public_key[BASE64_PK_LEN - 1] = '\0';
    }
    peer_unlock();
}

bool peer_get_public_key(const char* node_id, char* out, size_t size) {
    peer_lock();
    peer_t* p = peer_find(node_id);
    if (p && p->public_key[0] && size > 0) {
        strncpy(out, p->public_key, size - 1);
        out[size - 1] = '\0';
        peer_unlock();
        return true;
    }
    peer_unlock();
    return false;
}

void peer_set_connected(const char* node_id, bool connected) {
    peer_lock();
    peer_t* p = peer_find(node_id);
    if (p) p->connected = connected;
    peer_unlock();
}

int peer_connect_to(const char* addr, int port) {
    net_conn_t conn;
    if (net_tcp_connect(addr, (uint16_t)port, &conn, 5000) != 0) return -1;
    int flag = 1;
    setsockopt(conn.sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&flag, sizeof(flag));
    return (int)conn.sock;
}

void peer_disconnect(int index) {
    peer_lock();
    peer_t* p = peer_get(index);
    if (!p) { peer_unlock(); return; }
    p->connected = false;
    if (p->sock != INVALID_NET_SOCKET) {
        net_close(p->sock);
        p->sock = INVALID_NET_SOCKET;
    }
    peer_unlock();
}

void peer_disconnect_all(void) {
    peer_lock();
    peer_t* cur = g_peers;
    while (cur) {
        cur->connected = false;
        if (cur->sock != INVALID_NET_SOCKET) {
            net_close(cur->sock);
            cur->sock = INVALID_NET_SOCKET;
        }
        cur = cur->next;
    }
    peer_unlock();
}

void peer_check_timeouts(void) {
    uint64_t now = now_ms();
    peer_lock();
    peer_t* cur = g_peers;
    while (cur) {
        if (cur->connected && (now - cur->last_seen_ms) > PEER_TIMEOUT_MS) {
            cur->connected = false;
            if (cur->sock != INVALID_NET_SOCKET) {
                net_close(cur->sock);
                cur->sock = INVALID_NET_SOCKET;
            }
        }
        cur = cur->next;
    }
    peer_unlock();
}

void peer_lock(void) {
    pthread_mutex_lock(&g_peers_lock);
}

void peer_unlock(void) {
    pthread_mutex_unlock(&g_peers_lock);
}

void peer_lock_specific(peer_t* p) {
    if (p) pthread_mutex_lock(&p->lock);
}

void peer_unlock_specific(peer_t* p) {
    if (p) pthread_mutex_unlock(&p->lock);
}

void peer_for_each(peer_callback_t cb, void* userdata) {
    peer_lock();
    peer_t* cur = g_peers;
    while (cur) {
        cb(cur, userdata);
        cur = cur->next;
    }
    peer_unlock();
}
