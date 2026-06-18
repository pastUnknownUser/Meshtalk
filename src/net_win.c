#include "net.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")

static int g_winsock_initialized = 0;

int net_init(void) {
    if (g_winsock_initialized) return NET_OK;

    WSADATA wsa_data;
    int ret = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (ret != 0) return NET_ERROR;

    g_winsock_initialized = 1;
    return NET_OK;
}

void net_cleanup(void) {
    if (g_winsock_initialized) {
        WSACleanup();
        g_winsock_initialized = 0;
    }
}

int net_connect(net_socket_t *sock, const char *host, uint16_t port) {
    SOCKET fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET) return NET_ERROR;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        closesocket(fd);
        return NET_ERROR;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(fd);
        return NET_ERROR;
    }

    *sock = (int)fd;
    return NET_OK;
}

int net_listen(net_socket_t *sock, const char *bind_addr, uint16_t port) {
    SOCKET fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET) return NET_ERROR;

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(bind_addr);
    if (addr.sin_addr.s_addr == INADDR_NONE) {
        addr.sin_addr.s_addr = INADDR_ANY;
    }

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(fd);
        return NET_ERROR;
    }

    if (listen(fd, 128) == SOCKET_ERROR) {
        closesocket(fd);
        return NET_ERROR;
    }

    *sock = (int)fd;
    return NET_OK;
}

int net_accept(net_socket_t listener, net_socket_t *new_sock, char *peer_addr, size_t addr_len) {
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(addr);

    SOCKET fd = accept((SOCKET)listener, (struct sockaddr *)&addr, &addr_size);
    if (fd == INVALID_SOCKET) return NET_ERROR;

    if (peer_addr && addr_len > 0) {
        inet_ntop(AF_INET, &addr.sin_addr, peer_addr, (int)addr_len);
    }

    *new_sock = (int)fd;
    return NET_OK;
}

int net_udp_create(net_socket_t *sock, uint16_t port) {
    SOCKET fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == INVALID_SOCKET) return NET_ERROR;

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (const char *)&opt, sizeof(opt));
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    if (port > 0) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
            closesocket(fd);
            return NET_ERROR;
        }
    }

    *sock = (int)fd;
    return NET_OK;
}

int net_udp_broadcast(net_socket_t sock, const void *data, size_t len, uint16_t port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    if (sendto((SOCKET)sock, (const char *)data, (int)len, 0,
               (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        return NET_ERROR;
    }
    return NET_OK;
}

int net_udp_recvfrom(net_socket_t sock, void *buf, size_t buf_len,
                     char *peer_addr, size_t addr_len) {
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(addr);

    int n = recvfrom((SOCKET)sock, (char *)buf, (int)buf_len, 0,
                     (struct sockaddr *)&addr, &addr_size);
    if (n < 0) return NET_ERROR;

    if (peer_addr && addr_len > 0) {
        inet_ntop(AF_INET, &addr.sin_addr, peer_addr, (int)addr_len);
    }

    return n;
}

static int send_all(net_socket_t sock, const uint8_t *data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        int n = send((SOCKET)sock, (const char *)(data + sent), (int)(len - sent), 0);
        if (n < 0) {
            int err = WSAGetLastError();
            if (err == WSAEINTR) continue;
            if (err == WSAEWOULDBLOCK) return NET_WOULD_BLOCK;
            return NET_ERROR;
        }
        sent += n;
    }
    return (int)len;
}

int net_send(net_socket_t sock, const void *data, size_t len) {
    return send_all(sock, (const uint8_t *)data, len);
}

static int recv_all(net_socket_t sock, uint8_t *buf, size_t len) {
    size_t received = 0;
    while (received < len) {
        int n = recv((SOCKET)sock, (char *)(buf + received), (int)(len - received), 0);
        if (n < 0) {
            int err = WSAGetLastError();
            if (err == WSAEINTR) continue;
            if (err == WSAEWOULDBLOCK) return NET_WOULD_BLOCK;
            return NET_ERROR;
        }
        if (n == 0) return NET_CLOSED;
        received += n;
    }
    return (int)len;
}

int net_recv(net_socket_t sock, void *buf, size_t len) {
    return recv_all(sock, (uint8_t *)buf, len);
}

void net_close(net_socket_t sock) {
    if (sock >= 0) {
        closesocket((SOCKET)sock);
    }
}

int net_set_nonblock(net_socket_t sock, bool nonblock) {
    u_long mode = nonblock ? 1 : 0;
    return ioctlsocket((SOCKET)sock, FIONBIO, &mode) == 0 ? NET_OK : NET_ERROR;
}

int net_get_peer_addr(net_socket_t sock, char *addr, size_t addr_len, uint16_t *port) {
    struct sockaddr_in name;
    socklen_t name_size = sizeof(name);

    if (getpeername((SOCKET)sock, (struct sockaddr *)&name, &name_size) != 0) return NET_ERROR;

    if (addr && addr_len > 0) {
        inet_ntop(AF_INET, &name.sin_addr, addr, (int)addr_len);
    }
    if (port) {
        *port = ntohs(name.sin_port);
    }
    return NET_OK;
}

int net_get_local_addr(net_socket_t sock, char *addr, size_t addr_len, uint16_t *port) {
    struct sockaddr_in name;
    socklen_t name_size = sizeof(name);

    if (getsockname((SOCKET)sock, (struct sockaddr *)&name, &name_size) != 0) return NET_ERROR;

    if (addr && addr_len > 0) {
        inet_ntop(AF_INET, &name.sin_addr, addr, (int)addr_len);
    }
    if (port) {
        *port = ntohs(name.sin_port);
    }
    return NET_OK;
}

/* Windows needs its own net_init/net_cleanup, so override the common ones */
/* (net_common.c provides stubs for Unix; this file replaces them on Windows) */
