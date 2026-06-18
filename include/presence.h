#ifndef MESHTALK_PRESENCE_H
#define MESHTALK_PRESENCE_H

#include "message.h"
#include <stdbool.h>

#define MT_MAX_ONLINE_USERS 256

/* User presence info */
typedef struct {
    char node_id[MT_MAX_NODE_ID_LEN];
    char username[MT_MAX_USERNAME_LEN];
    uint32_t last_seen;
    uint32_t joined_at;
    bool online;
} mt_presence_user_t;

/* Presence manager */
typedef struct mt_presence mt_presence_t;

/* Callbacks */
typedef void (*mt_presence_join_callback)(const char *node_id, const char *username, void *user_data);
typedef void (*mt_presence_leave_callback)(const char *node_id, const char *username, void *user_data);

/* Create presence manager */
mt_presence_t *mt_presence_create(const char *local_node_id,
                                   mt_presence_join_callback join_cb,
                                   mt_presence_leave_callback leave_cb,
                                   void *user_data);

/* Destroy presence manager */
void mt_presence_destroy(mt_presence_t *pres);

/* Mark self as online (broadcast to peers) */
int mt_presence_go_online(mt_presence_t *pres, const char *username);

/* Mark self as offline (broadcast on shutdown) */
int mt_presence_go_offline(mt_presence_t *pres);

/* Handle remote user join */
void mt_presence_handle_join(mt_presence_t *pres, const char *node_id,
                              const char *username, uint32_t timestamp);

/* Handle remote user leave */
void mt_presence_handle_leave(mt_presence_t *pres, const char *node_id,
                               const char *username, uint32_t timestamp);

/* Update last seen for a peer */
void mt_presence_update_seen(mt_presence_t *pres, const char *node_id);

/* Timeout inactive peers */
void mt_presence_timeout_check(mt_presence_t *pres);

/* Get online users */
int mt_presence_get_users(mt_presence_t *pres, mt_presence_user_t *users, int max_users);

/* Get online user count */
int mt_presence_online_count(mt_presence_t *pres);

/* Find user by node_id */
const mt_presence_user_t *mt_presence_find(mt_presence_t *pres, const char *node_id);

/* Find user by username (returns first match) */
const mt_presence_user_t *mt_presence_find_by_name(mt_presence_t *pres, const char *username);

/* Remove a peer entirely (on disconnect) */
void mt_presence_remove(mt_presence_t *pres, const char *node_id);

/* Update username for a node */
void mt_presence_update_username(mt_presence_t *pres, const char *node_id, const char *username);

#endif /* MESHTALK_PRESENCE_H */
