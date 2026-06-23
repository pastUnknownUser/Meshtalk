#include "net.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/select.h>

int net_init(void) {
    signal(SIGPIPE, SIG_IGN);
    return 0;
}

void net_cleanup(void) {}

int net_close(net_socket_t sock) {
    if (sock < 0) return -1;
    shutdown(sock, SHUT_RDWR);
    return close(sock);
}

int net_tcp_listen(uint16_t port, net_socket_t* out_sock) {
    struct sockaddr_in addr;
    int opt = 1;
    net_socket_t sock = socket(AF_INET, SOCK_STREAM | 0, 0);
    if (sock < 0) return -1;

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    if (listen(sock, SOMAXCONN) < 0) {
        close(sock);
        return -1;
    }
    *out_sock = sock;
    return 0;
}

int net_tcp_accept(net_socket_t listen_sock, net_conn_t* out_conn, int timeout_ms) {
    socklen_t addr_len = sizeof(out_conn->addr);
    net_socket_t client;

    if (timeout_ms > 0) {
        struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(listen_sock, &fds);
        int ret = select(listen_sock + 1, &fds, NULL, NULL, &tv);
        if (ret <= 0) return -1;
    }

    client = accept(listen_sock, (struct sockaddr*)&out_conn->addr, &addr_len);
    if (client < 0) return -1;

    out_conn->sock = client;
    out_conn->addr_len = addr_len;
    return 0;
}

int net_tcp_connect(const char* host, uint16_t port, net_conn_t* out_conn, int timeout_ms) {
    struct sockaddr_in addr;
    struct hostent* he;
    net_socket_t sock;

    sock = socket(AF_INET, SOCK_STREAM | 0, 0);
    if (sock < 0) return -1;

    if (timeout_ms > 0) {
        net_set_nonblocking(sock, true);
    }

    he = gethostbyname(host);
    if (!he) {
        close(sock);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        if (timeout_ms > 0 && errno == EINPROGRESS) {
            struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(sock, &wfds);
            int ret = select(sock + 1, NULL, &wfds, NULL, &tv);
            if (ret <= 0) {
                close(sock);
                return -1;
            }
            int so_error = 0;
            socklen_t slen = sizeof(so_error);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &slen);
            if (so_error != 0) {
                close(sock);
                return -1;
            }
        } else {
            close(sock);
            return -1;
        }
    }

    net_set_nonblocking(sock, false);
    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    out_conn->sock = sock;
    out_conn->addr_len = sizeof(addr);
    memcpy(&out_conn->addr, &addr, sizeof(addr));
    return 0;
}

int net_send(net_socket_t sock, const void* data, size_t len) {
    return (int)send(sock, data, len, 0);
}

int net_recv(net_socket_t sock, void* buf, size_t len, int timeout_ms) {
    if (timeout_ms > 0) {
        struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        int ret = select(sock + 1, &fds, NULL, NULL, &tv);
        if (ret <= 0) return -1;
    }
    return (int)recv(sock, buf, len, 0);
}

int net_send_all(net_socket_t sock, const void* data, size_t len) {
    const char* ptr = (const char*)data;
    size_t remaining = len;
    while (remaining > 0) {
        int n = (int)send(sock, ptr, remaining, 0);
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
            int ret = select(sock + 1, &fds, NULL, NULL, &tv);
            if (ret <= 0) return -1;
        }
        int n = (int)recv(sock, ptr, remaining, 0);
        if (n <= 0) return -1;
        ptr += n;
        remaining -= n;
    }
    return (int)len;
}

int net_udp_broadcast_socket(uint16_t port, net_socket_t* out_sock) {
    int broadcast = 1;
    net_socket_t sock = socket(AF_INET, SOCK_DGRAM | 0, 0);
    if (sock < 0) return -1;

    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

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
    int reuse = 1;
    net_socket_t sock = socket(AF_INET, SOCK_DGRAM | 0, 0);
    if (sock < 0) return -1;

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
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
    inet_pton(AF_INET, addr_str, &addr.sin_addr);
    return (int)sendto(sock, data, len, 0, (struct sockaddr*)&addr, sizeof(addr));
}

int net_udp_recv(net_socket_t sock, void* buf, size_t len, char* from_addr, uint16_t* from_port, int timeout_ms) {
    if (timeout_ms > 0) {
        struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        int ret = select(sock + 1, &fds, NULL, NULL, &tv);
        if (ret <= 0) return -1;
    }

    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);
    int n = (int)recvfrom(sock, buf, len, 0, (struct sockaddr*)&from, &from_len);
    if (n > 0 && from_addr) {
        inet_ntop(AF_INET, &from.sin_addr, from_addr, INET_ADDRSTRLEN);
    }
    if (n > 0 && from_port) {
        *from_port = ntohs(from.sin_port);
    }
    return n;
}

int net_set_nonblocking(net_socket_t sock, bool nonblock) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return -1;
    fcntl(sock, F_SETFL, nonblock ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK));
    return 0;
}

int net_get_error(void) {
    return errno;
}

const char* net_strerror(int err) {
    return strerror(err);
}

bool net_is_valid_socket(net_socket_t sock) {
    return sock >= 0;
}
