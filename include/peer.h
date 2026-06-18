#ifndef MESHTALK_PEER_H
#define MESHTALK_PEER_H

#include "net.h"
#include "message.h"
#include <stdbool.h>

#define MT_MAX_PEERS 256
#define MT_MAX_PEER_HISTORY 1000

/* Peer connection states */
typedef enum {
    MT_PEER_DISCONNECTED,
    MT_PEER_CONNECTING,
    MT_PEER_CONNECTED,
    MT_PEER_RECONNECTING
} mt_peer_state_t;

/* Peer info */
typedef struct {
    char node_id[MT_MAX_NODE_ID_LEN];
    char username[MT_MAX_USERNAME_LEN];
    char ip_addr[46];
    uint16_t tcp_port;
    net_socket_t sock;
    mt_peer_state_t state;
    uint32_t last_seen;
    uint32_t connected_at;
    bool is_outbound;  /* true if we initiated the connection */
} mt_peer_info_t;

/* Peer manager */
typedef struct mt_peer_manager mt_peer_manager_t;

/* Callback for incoming frames from peers */
typedef void (*mt_peer_frame_callback)(const char *from_node_id, const mt_frame_t *frame, void *user_data);
typedef void (*mt_peer_connect_callback)(const char *node_id, void *user_data);
typedef void (*mt_peer_disconnect_callback)(const char *node_id, void *user_data);

/* Create peer manager */
mt_peer_manager_t *mt_peer_manager_create(
    mt_peer_frame_callback frame_cb,
    mt_peer_connect_callback connect_cb,
    mt_peer_disconnect_callback disconnect_cb,
    void *user_data);

/* Destroy peer manager */
void mt_peer_manager_destroy(mt_peer_manager_t *mgr);

/* Add a new peer to connect to */
int mt_peer_manager_add_peer(mt_peer_manager_t *mgr, const char *node_id,
                              const char *username, const char *ip_addr, uint16_t tcp_port);

/* Register an incoming connection */
int mt_peer_manager_register_incoming(mt_peer_manager_t *mgr, net_socket_t sock,
                                       const char *node_id, const char *username,
                                       const char *ip_addr, uint16_t tcp_port);

/* Send frame to a specific peer */
int mt_peer_manager_send_to(mt_peer_manager_t *mgr, const char *node_id, const mt_frame_t *frame);

/* Send frame to all connected peers (except sender) */
int mt_peer_manager_broadcast(mt_peer_manager_t *mgr, const char *sender_id, const mt_frame_t *frame);

/* Check for new connections to accept */
void mt_peer_manager_check_connections(mt_peer_manager_t *mgr, net_socket_t listener);

/* Timeout check - mark dead peers */
void mt_peer_manager_timeout_check(mt_peer_manager_t *mgr);

/* Get peer count */
int mt_peer_manager_count(mt_peer_manager_t *mgr);

/* Get connected peer count */
int mt_peer_manager_connected_count(mt_peer_manager_t *mgr);

/* Get all peer entries */
int mt_peer_manager_get_entries(mt_peer_manager_t *mgr, mt_peer_entry_t *out, uint16_t max_count);

/* Check if peer is connected */
bool mt_peer_manager_is_connected(mt_peer_manager_t *mgr, const char *node_id);

/* Remove a peer */
void mt_peer_manager_remove_peer(mt_peer_manager_t *mgr, const char *node_id);

/* Find peer by node_id */
const mt_peer_info_t *mt_peer_manager_find(mt_peer_manager_t *mgr, const char *node_id);

#endif /* MESHTALK_PEER_H */
