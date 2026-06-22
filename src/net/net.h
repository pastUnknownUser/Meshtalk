#pragma once

#include "../common.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET net_socket_t;
    #define INVALID_NET_SOCKET INVALID_SOCKET
    #define NET_SOCKET_ERROR SOCKET_ERROR
    #define NET_EWOULDBLOCK WSAEWOULDBLOCK
    #define NET_ECONNRESET WSAECONNRESET
    #define NET_EINTR WSAEINTR
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    typedef int net_socket_t;
    #define INVALID_NET_SOCKET (-1)
    #define NET_SOCKET_ERROR (-1)
    #define NET_EWOULDBLOCK EWOULDBLOCK
    #define NET_ECONNRESET ECONNRESET
    #define NET_EINTR EINTR
#endif

typedef struct {
    net_socket_t sock;
    struct sockaddr_storage addr;
    socklen_t addr_len;
} net_conn_t;

int  net_init(void);
void net_cleanup(void);
int  net_close(net_socket_t sock);

int  net_tcp_listen(uint16_t port, net_socket_t* out_sock);
int  net_tcp_accept(net_socket_t listen_sock, net_conn_t* out_conn, int timeout_ms);
int  net_tcp_connect(const char* host, uint16_t port, net_conn_t* out_conn, int timeout_ms);

int  net_send(net_socket_t sock, const void* data, size_t len);
int  net_recv(net_socket_t sock, void* buf, size_t len, int timeout_ms);
int  net_send_all(net_socket_t sock, const void* data, size_t len);
int  net_recv_all(net_socket_t sock, void* buf, size_t len, int timeout_ms);

int  net_udp_broadcast_socket(uint16_t port, net_socket_t* out_sock);
int  net_udp_listen_socket(uint16_t port, net_socket_t* out_sock);
int  net_udp_send(net_socket_t sock, const void* data, size_t len, const char* addr, uint16_t port);
int  net_udp_recv(net_socket_t sock, void* buf, size_t len, char* from_addr, uint16_t* from_port, int timeout_ms);

int  net_set_nonblocking(net_socket_t sock, bool nonblock);
int  net_get_error(void);
const char* net_strerror(int err);
bool net_is_valid_socket(net_socket_t sock);
