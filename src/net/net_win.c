#include "net.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int net_init(void) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return -1;
    return 0;
}

void net_cleanup(void) {
    WSACleanup();
}

int net_close(net_socket_t sock) {
    if (sock == INVALID_SOCKET) return -1;
    shutdown(sock, SD_BOTH);
    return closesocket(sock);
}

int net_tcp_listen(uint16_t port, net_socket_t* out_sock) {
    SOCKET sock;
    struct sockaddr_in addr;
    int opt = 1;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) return -1;

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock);
        return -1;
    }
    if (listen(sock, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(sock);
        return -1;
    }
    *out_sock = sock;
    return 0;
}

int net_tcp_accept(net_socket_t listen_sock, net_conn_t* out_conn, int timeout_ms) {
    socklen_t addr_len = sizeof(out_conn->addr);

    if (timeout_ms > 0) {
        struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(listen_sock, &fds);
        int ret = select(0, &fds, NULL, NULL, &tv);
        if (ret <= 0) return -1;
    }

    SOCKET client = accept(listen_sock, (struct sockaddr*)&out_conn->addr, &addr_len);
    if (client == INVALID_SOCKET) return -1;

    out_conn->sock = client;
    out_conn->addr_len = addr_len;
    return 0;
}

int net_tcp_connect(const char* host, uint16_t port, net_conn_t* out_conn, int timeout_ms) {
    SOCKET sock;
    struct sockaddr_in addr;
    struct hostent* he;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) return -1;

    if (timeout_ms > 0) {
        net_set_nonblocking(sock, true);
    }

    he = gethostbyname(host);
    if (!he) {
        closesocket(sock);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        if (timeout_ms > 0 && WSAGetLastError() == WSAEWOULDBLOCK) {
            struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(sock, &wfds);
            int ret = select(0, NULL, &wfds, NULL, &tv);
            if (ret <= 0) {
                closesocket(sock);
                return -1;
            }
            int so_error = 0;
            socklen_t slen = sizeof(so_error);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&so_error, &slen);
            if (so_error != 0) {
                closesocket(sock);
                return -1;
            }
        } else {
            closesocket(sock);
            return -1;
        }
    }

    net_set_nonblocking(sock, false);
    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&flag, sizeof(flag));

    out_conn->sock = sock;
    out_conn->addr_len = sizeof(addr);
    memcpy(&out_conn->addr, &addr, sizeof(addr));
    return 0;
}

int net_send(net_socket_t sock, const void* data, size_t len) {
    return send(sock, (const char*)data, (int)len, 0);
}

int net_recv(net_socket_t sock, void* buf, size_t len, int timeout_ms) {
    if (timeout_ms > 0) {
        struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        int ret = select(0, &fds, NULL, NULL, &tv);
        if (ret <= 0) return -1;
    }
    return recv(sock, (char*)buf, (int)len, 0);
}

int net_send_all(net_socket_t sock, const void* data, size_t len) {
    const char* ptr = (const char*)data;
    size_t remaining = len;
    while (remaining > 0) {
        int n = send(sock, ptr, (int)remaining, 0);
        if (n <= 0) return -1;
        ptr += n;
        remaining -= n;
    }
    return (int)len;
}

int net_recv_all(net_socket_t sock, void* buf, size_t len, int timeout_ms) {
    char* ptr = (char*)buf;
    size_t remaining = len;
    while (remaining > 0) {
        if (timeout_ms > 0) {
            struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(sock, &fds);
            int ret = select(0, &fds, NULL, NULL, &tv);
            if (ret <= 0) return -1;
        }
        int n = recv(sock, ptr, (int)remaining, 0);
        if (n <= 0) return -1;
        ptr += n;
        remaining -= n;
    }
    return (int)len;
}

int net_udp_broadcast_socket(uint16_t port, net_socket_t* out_sock) {
    SOCKET sock;
    BOOL broadcast = TRUE;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) return -1;

    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcast, sizeof(broadcast));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    bind(sock, (struct sockaddr*)&addr, sizeof(addr));

    *out_sock = sock;
    return 0;
}

int net_udp_listen_socket(uint16_t port, net_socket_t* out_sock) {
    SOCKET sock;
    BOOL reuse = TRUE;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) return -1;

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock);
        return -1;
    }
    *out_sock = sock;
    return 0;
}

int net_udp_send(net_socket_t sock, const void* data, size_t len, const char* addr_str, uint16_t port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(addr_str);
    return sendto(sock, (const char*)data, (int)len, 0, (struct sockaddr*)&addr, sizeof(addr));
}

int net_udp_recv(net_socket_t sock, void* buf, size_t len, char* from_addr, uint16_t* from_port, int timeout_ms) {
    if (timeout_ms > 0) {
        struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        int ret = select(0, &fds, NULL, NULL, &tv);
        if (ret <= 0) return -1;
    }

    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);
    int n = recvfrom(sock, (char*)buf, (int)len, 0, (struct sockaddr*)&from, &from_len);
    if (n > 0 && from_addr) {
        strcpy(from_addr, inet_ntoa(from.sin_addr));
    }
    if (n > 0 && from_port) {
        *from_port = ntohs(from.sin_port);
    }
    return n;
}

int net_set_nonblocking(net_socket_t sock, bool nonblock) {
    u_long mode = nonblock ? 1 : 0;
    return ioctlsocket(sock, FIONBIO, &mode);
}

int net_get_error(void) {
    return WSAGetLastError();
}

const char* net_strerror(int err) {
    static char buf[256];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, err, 0, buf, sizeof(buf), NULL);
    return buf;
}

bool net_is_valid_socket(net_socket_t sock) {
    return sock != INVALID_SOCKET;
}
