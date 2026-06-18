#ifndef MESHTALK_CORE_H
#define MESHTALK_CORE_H

#include "net.h"
#include "message.h"
#include "discovery.h"
#include "peer.h"
#include "dedup.h"
#include "room.h"
#include "presence.h"
#include "persistence.h"
#include "ui.h"
#include "security.h"
#include <stdbool.h>

/* Meshtalk instance - the main orchestrator */
typedef struct {
    /* Identity */
    char node_id[MT_MAX_NODE_ID_LEN];
    char username[MT_MAX_USERNAME_LEN];
    uint16_t tcp_port;

    /* Networking */
    net_socket_t tcp_listener;

    /* Subsystems */
    mt_discovery_t *discovery;
    mt_peer_manager_t *peers;
    mt_dedup_t *dedup;
    mt_room_manager_t *rooms;
    mt_presence_t *presence;
    mt_persistence_t *persistence;
    mt_ui_t *ui;

    /* State */
    bool running;
    uint32_t last_heartbeat;
    uint32_t last_dedup_cleanup;
} mt_context_t;

/* Initialize the Meshtalk context */
int mt_init(mt_context_t *ctx, const char *username, uint16_t tcp_port);

/* Run the main event loop */
void mt_run(mt_context_t *ctx);

/* Shutdown gracefully */
void mt_shutdown(mt_context_t *ctx);

/* Send a chat message to the current room */
int mt_send_chat(mt_context_t *ctx, const char *room, const char *payload);

/* Send a private message */
int mt_send_private(mt_context_t *ctx, const char *dest_username, const char *payload);

/* Handle incoming frame from a peer */
void mt_handle_frame(mt_context_t *ctx, const char *from_node_id, const mt_frame_t *frame);

/* Handle discovery callback */
void mt_on_discovery(mt_context_t *ctx, const char *node_id, const char *username,
                      const char *ip_addr, uint16_t tcp_port, void *user_data);

/* Handle peer connect */
void mt_on_peer_connect(mt_context_t *ctx, const char *node_id, void *user_data);

/* Handle peer disconnect */
void mt_on_peer_disconnect(mt_context_t *ctx, const char *node_id, void *user_data);

/* Handle room join/leave callbacks */
void mt_on_room_join(mt_context_t *ctx, const char *room, const char *node_id,
                      const char *username, void *user_data);
void mt_on_room_leave(mt_context_t *ctx, const char *room, const char *node_id,
                       const char *username, void *user_data);

/* Handle presence callbacks */
void mt_on_presence_join(mt_context_t *ctx, const char *node_id,
                          const char *username, void *user_data);
void mt_on_presence_leave(mt_context_t *ctx, const char *node_id,
                           const char *username, void *user_data);

/* Process periodic tasks */
void mt_periodic_tasks(mt_context_t *ctx);

#endif /* MESHTALK_CORE_H */
