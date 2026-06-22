#include "discovery.h"
#include "../peer/peer.h"
#include "../net/net.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static net_socket_t g_disc_sock = INVALID_NET_SOCKET;
static char g_node_id[UUID_STR_LEN + 1];
static char g_username[MAX_USERNAME_LEN + 1];
static int g_tcp_port;
static volatile bool g_running = false;
static pthread_t g_thread;
static pthread_t g_listener_thread;

#define DISCOVER_MSG "DISCOVER\n%s\n%d\n%s\n"
#define HERE_MSG     "HERE\n%s\n%d\n%s\n"

int discovery_init(net_socket_t* udp_sock, const char* node_id, const char* username, int tcp_port) {
    strncpy(g_node_id, node_id, UUID_STR_LEN);
    g_node_id[UUID_STR_LEN] = '\0';
    strncpy(g_username, username, MAX_USERNAME_LEN);
    g_username[MAX_USERNAME_LEN] = '\0';
    g_tcp_port = tcp_port;

    net_socket_t sock;
    if (net_udp_listen_socket(DISCOVERY_PORT, &sock) != 0) return -1;

    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcast, sizeof(broadcast));

    g_disc_sock = sock;
    *udp_sock = sock;
    return 0;
}

void discovery_cleanup(void) {
    discovery_stop();
    if (g_disc_sock != INVALID_NET_SOCKET) {
        net_close(g_disc_sock);
        g_disc_sock = INVALID_NET_SOCKET;
    }
}

static void* discovery_broadcast_thread(void* arg) {
    (void)arg;
    char buf[256];
    while (g_running) {
        int n = snprintf(buf, sizeof(buf), DISCOVER_MSG, g_username, g_tcp_port, g_node_id);
        net_udp_send(g_disc_sock, buf, (size_t)n, "255.255.255.255", DISCOVERY_PORT);

        for (int i = 0; i < 50 && g_running; i++) msleep(100);
    }
    return NULL;
}

static void* discovery_listener_thread(void* arg) {
    (void)arg;
    char buf[1024];
    char from_addr[INET_ADDRSTRLEN];
    uint16_t from_port;

    while (g_running) {
        int n = net_udp_recv(g_disc_sock, buf, sizeof(buf) - 1, from_addr, &from_port, 100);
        if (n > 0) {
            buf[n] = '\0';
            discovery_process_packet(buf, n, from_addr);
        }
    }
    return NULL;
}

void discovery_start(void) {
    if (g_running) return;
    g_running = true;
    pthread_create(&g_thread, NULL, discovery_broadcast_thread, NULL);
    pthread_create(&g_listener_thread, NULL, discovery_listener_thread, NULL);
}

void discovery_stop(void) {
    if (!g_running) return;
    g_running = false;
    pthread_join(g_thread, NULL);
    pthread_join(g_listener_thread, NULL);
}

int discovery_send_broadcast(void) {
    char buf[256];
    int n = snprintf(buf, sizeof(buf), DISCOVER_MSG, g_username, g_tcp_port, g_node_id);
    net_udp_send(g_disc_sock, buf, (size_t)n, "255.255.255.255", DISCOVERY_PORT);

    buf[0] = 'H';
    buf[1] = 'E';
    buf[2] = 'R';
    buf[3] = 'E';
    net_udp_send(g_disc_sock, buf, (size_t)n, "255.255.255.255", DISCOVERY_PORT);
    return 0;
}

static int parse_discovery_packet(const char* data, char* cmd, size_t cmd_size,
                                   char* username, size_t uname_size,
                                   int* port, char* node_id, size_t id_size) {
    const char* p = data;
    const char* nl = strchr(p, '\n');
    if (!nl) return -1;

    size_t cmd_len = (size_t)(nl - p);
    if (cmd_len >= cmd_size) cmd_len = cmd_size - 1;
    memcpy(cmd, p, cmd_len);
    cmd[cmd_len] = '\0';
    p = nl + 1;

    nl = strchr(p, '\n');
    if (!nl) return -1;
    size_t uname_len = (size_t)(nl - p);
    if (uname_len >= uname_size) uname_len = uname_size - 1;
    memcpy(username, p, uname_len);
    username[uname_len] = '\0';
    p = nl + 1;

    *port = atoi(p);
    nl = strchr(p, '\n');
    if (!nl) return -1;
    p = nl + 1;

    nl = strchr(p, '\n');
    if (!nl) {
        size_t id_len = strlen(p);
        if (id_len >= id_size) id_len = id_size - 1;
        memcpy(node_id, p, id_len);
        node_id[id_len] = '\0';
    } else {
        size_t id_len = (size_t)(nl - p);
        if (id_len >= id_size) id_len = id_size - 1;
        memcpy(node_id, p, id_len);
        node_id[id_len] = '\0';
    }

    return 0;
}

int discovery_process_packet(const char* data, int len, const char* from_addr) {
    (void)len;
    char cmd[16];
    char username[MAX_USERNAME_LEN + 1];
    char node_id[UUID_STR_LEN + 1];
    int port = 0;

    if (parse_discovery_packet(data, cmd, sizeof(cmd), username, sizeof(username), &port, node_id, sizeof(node_id)) != 0)
        return -1;

    if (strcmp(node_id, g_node_id) == 0) return 0;

    if (strcmp(cmd, "DISCOVER") == 0) {
        char buf[256];
        int n = snprintf(buf, sizeof(buf), HERE_MSG, g_username, g_tcp_port, g_node_id);
        net_udp_send(g_disc_sock, buf, (size_t)n, from_addr, DISCOVERY_PORT);

        if (port > 0) {
            peer_lock();
            bool exists = peer_find(node_id) != NULL;
            peer_unlock();
            if (!exists) {
                peer_add(node_id, username, from_addr, port);
            }
        }
    } else if (strcmp(cmd, "HERE") == 0) {
        if (port > 0) {
            peer_lock();
            peer_t* p = peer_find(node_id);
            if (!p) {
                peer_unlock();
                peer_add(node_id, username, from_addr, port);
            } else {
                strncpy(p->username, username, MAX_USERNAME_LEN);
                p->last_seen_ms = now_ms();
                peer_unlock();
            }
        }
    }

    return 0;
}
