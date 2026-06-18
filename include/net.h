#ifndef MESHTALK_NET_H
#define MESHTALK_NET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Socket handle (opaque to application code) */
typedef int net_socket_t;

#define NET_SOCKET_INVALID ((net_socket_t)-1)

/* Return codes */
#define NET_OK          0
#define NET_ERROR      -1
#define NET_WOULD_BLOCK -2
#define NET_AGAIN      -3
#define NET_CLOSED     -4

/* Initialization */
int net_init(void);
void net_cleanup(void);

/* Create a TCP socket and connect to remote host */
int net_connect(net_socket_t *sock, const char *host, uint16_t port);

/* Create a listening TCP socket */
int net_listen(net_socket_t *sock, const char *bind_addr, uint16_t port);

/* Accept an incoming TCP connection */
int net_accept(net_socket_t listener, net_socket_t *new_sock, char *peer_addr, size_t addr_len);

/* UDP: create a socket for broadcast discovery */
int net_udp_create(net_socket_t *sock, uint16_t port);

/* UDP: send to broadcast address */
int net_udp_broadcast(net_socket_t sock, const void *data, size_t len, uint16_t port);

/* UDP: receive */
int net_udp_recvfrom(net_socket_t sock, void *buf, size_t buf_len,
                     char *peer_addr, size_t addr_len);

/* Send data over TCP (sends all bytes or fails) */
int net_send(net_socket_t sock, const void *data, size_t len);

/* Receive exactly len bytes over TCP */
int net_recv(net_socket_t sock, void *buf, size_t len);

/* Close a socket */
void net_close(net_socket_t sock);

/* Set socket to non-blocking mode */
int net_set_nonblock(net_socket_t sock, bool nonblock);

/* Get peer address string */
int net_get_peer_addr(net_socket_t sock, char *addr, size_t addr_len, uint16_t *port);

/* Get local address string */
int net_get_local_addr(net_socket_t sock, char *addr, size_t addr_len, uint16_t *port);

#endif /* MESHTALK_NET_H */
