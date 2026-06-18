#ifndef MESHTALK_PERSISTENCE_H
#define MESHTALK_PERSISTENCE_H

#include "message.h"
#include <stdbool.h>

#define MT_MAX_HISTORY 1000
#define MT_DATA_DIR ".meshtalk"

/* Stored message */
typedef struct {
    char message_id[MT_MAX_MESSAGE_ID_LEN];
    char sender_id[MT_MAX_NODE_ID_LEN];
    char sender_name[MT_MAX_USERNAME_LEN];
    char room[MT_MAX_ROOM_LEN];
    uint32_t timestamp;
    char payload[MT_MAX_TEXT_LEN];
} mt_stored_message_t;

/* Persistence context */
typedef struct mt_persistence mt_persistence_t;

/* Create persistence layer (creates data directory if needed) */
mt_persistence_t *mt_persistence_create(void);

/* Destroy persistence layer */
void mt_persistence_destroy(mt_persistence_t *pers);

/* Node ID management */
int  mt_persistence_load_node_id(mt_persistence_t *pers, char *node_id, size_t len);
int  mt_persistence_save_node_id(mt_persistence_t *pers, const char *node_id);

/* Username management */
int  mt_persistence_load_username(mt_persistence_t *pers, char *username, size_t len);
int  mt_persistence_save_username(mt_persistence_t *pers, const char *username);

/* Known peers */
int  mt_persistence_load_peers(mt_persistence_t *pers, mt_peer_entry_t *peers, int max_peers);
int  mt_persistence_save_peers(mt_persistence_t *pers, const mt_peer_entry_t *peers, int count);

/* Message history (per room) */
int  mt_persistence_load_history(mt_persistence_t *pers, const char *room,
                                  mt_stored_message_t *msgs, int max_msgs);
int  mt_persistence_save_message(mt_persistence_t *pers, const mt_stored_message_t *msg);
int  mt_persistence_history_count(mt_persistence_t *pers, const char *room);

/* Get data directory path */
const char *mt_persistence_data_dir(mt_persistence_t *pers);

#endif /* MESHTALK_PERSISTENCE_H */
