#include "net.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

int net_connect(net_socket_t *sock, const char *host, uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return NET_ERROR;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        close(fd);
        return NET_ERROR;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return NET_ERROR;
    }

    *sock = fd;
    return NET_OK;
}

int net_listen(net_socket_t *sock, const char *bind_addr, uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return NET_ERROR;

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(bind_addr);
    if (addr.sin_addr.s_addr == (uint32_t)-1) {
        addr.sin_addr.s_addr = INADDR_ANY;
    }

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return NET_ERROR;
    }

    if (listen(fd, 128) < 0) {
        close(fd);
        return NET_ERROR;
    }

    *sock = fd;
    return NET_OK;
}

int net_accept(net_socket_t listener, net_socket_t *new_sock, char *peer_addr, size_t addr_len) {
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(addr);

    int fd = accept(listener, (struct sockaddr *)&addr, &addr_size);
    if (fd < 0) return NET_ERROR;

    if (peer_addr && addr_len > 0) {
        inet_ntop(AF_INET, &addr.sin_addr, peer_addr, addr_len);
    }

    *new_sock = fd;
    return NET_OK;
}

int net_udp_create(net_socket_t *sock, uint16_t port) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return NET_ERROR;

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (port > 0) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            close(fd);
            return NET_ERROR;
        }
    }

    *sock = fd;
    return NET_OK;
}

int net_udp_broadcast(net_socket_t sock, const void *data, size_t len, uint16_t port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    if (sendto(sock, data, len, 0, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        return NET_ERROR;
    }
    return NET_OK;
}

int net_udp_recvfrom(net_socket_t sock, void *buf, size_t buf_len,
                     char *peer_addr, size_t addr_len) {
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(addr);

    ssize_t n = recvfrom(sock, buf, buf_len, 0,
                         (struct sockaddr *)&addr, &addr_size);
    if (n < 0) return NET_ERROR;

    if (peer_addr && addr_len > 0) {
        inet_ntop(AF_INET, &addr.sin_addr, peer_addr, addr_len);
    }

    return (int)n;
}

static int send_all(net_socket_t sock, const uint8_t *data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(sock, data + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EWOULDBLOCK || errno == EAGAIN) return NET_WOULD_BLOCK;
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
        ssize_t n = recv(sock, buf + received, len - received, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EWOULDBLOCK || errno == EAGAIN) return NET_WOULD_BLOCK;
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
        close(sock);
    }
}

int net_set_nonblock(net_socket_t sock, bool nonblock) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return NET_ERROR;

    if (nonblock) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    return fcntl(sock, F_SETFL, flags) < 0 ? NET_ERROR : NET_OK;
}

int net_get_peer_addr(net_socket_t sock, char *addr, size_t addr_len, uint16_t *port) {
    struct sockaddr_in name;
    socklen_t name_size = sizeof(name);

    if (getpeername(sock, (struct sockaddr *)&name, &name_size) < 0) return NET_ERROR;

    if (addr && addr_len > 0) {
        inet_ntop(AF_INET, &name.sin_addr, addr, addr_len);
    }
    if (port) {
        *port = ntohs(name.sin_port);
    }
    return NET_OK;
}

int net_get_local_addr(net_socket_t sock, char *addr, size_t addr_len, uint16_t *port) {
    struct sockaddr_in name;
    socklen_t name_size = sizeof(name);

    if (getsockname(sock, (struct sockaddr *)&name, &name_size) < 0) return NET_ERROR;

    if (addr && addr_len > 0) {
        inet_ntop(AF_INET, &name.sin_addr, addr, addr_len);
    }
    if (port) {
        *port = ntohs(name.sin_port);
    }
    return NET_OK;
}
