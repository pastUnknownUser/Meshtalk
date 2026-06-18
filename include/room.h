#ifndef MESHTALK_ROOM_H
#define MESHTALK_ROOM_H

#include "message.h"
#include <stdbool.h>

#define MT_MAX_ROOMS 64
#define MT_MAX_ROOM_MEMBERS 256
#define MT_DEFAULT_ROOM "general"

/* Room membership info */
typedef struct {
    char node_id[MT_MAX_NODE_ID_LEN];
    char username[MT_MAX_USERNAME_LEN];
    uint32_t joined_at;
} mt_room_member_t;

/* Room info */
typedef struct {
    char name[MT_MAX_ROOM_LEN];
    mt_room_member_t members[MT_MAX_ROOM_MEMBERS];
    int member_count;
    bool exists;
    uint32_t created_at;
    char created_by[MT_MAX_NODE_ID_LEN];
} mt_room_t;

/* Room manager */
typedef struct mt_room_manager mt_room_manager_t;

/* Callbacks */
typedef void (*mt_room_join_callback)(const char *room, const char *node_id,
                                       const char *username, void *user_data);
typedef void (*mt_room_leave_callback)(const char *room, const char *node_id,
                                        const char *username, void *user_data);

/* Create room manager */
mt_room_manager_t *mt_room_manager_create(mt_room_join_callback join_cb,
                                           mt_room_leave_callback leave_cb,
                                           void *user_data);

/* Destroy room manager */
void mt_room_manager_destroy(mt_room_manager_t *mgr);

/* Create a new room (propagates to peers) */
int mt_room_manager_create_room(mt_room_manager_t *mgr, const char *room,
                                 const char *node_id, const char *username);

/* Join a room */
int mt_room_manager_join(mt_room_manager_t *mgr, const char *room,
                          const char *node_id, const char *username);

/* Leave a room */
int mt_room_manager_leave(mt_room_manager_t *mgr, const char *room,
                           const char *node_id, const char *username);

/* Leave all rooms */
void mt_room_manager_leave_all(mt_room_manager_t *mgr, const char *node_id);

/* Check if node is in room */
bool mt_room_manager_is_member(mt_room_manager_t *mgr, const char *room, const char *node_id);

/* Get rooms a node is in */
int mt_room_manager_get_node_rooms(mt_room_manager_t *mgr, const char *node_id,
                                    char rooms[][MT_MAX_ROOM_LEN], int max_rooms);

/* Get room members */
int mt_room_manager_get_members(mt_room_manager_t *mgr, const char *room,
                                 mt_room_member_t *members, int max_members);

/* Get current room for local node */
const char *mt_room_manager_current_room(mt_room_manager_t *mgr, const char *node_id);

/* Handle remote room join event */
int mt_room_manager_handle_remote_join(mt_room_manager_t *mgr, const char *node_id,
                                        const char *username, const char *room);

/* Handle remote room leave event */
int mt_room_manager_handle_remote_leave(mt_room_manager_t *mgr, const char *node_id,
                                         const char *username, const char *room);

/* Get room count */
int mt_room_manager_room_count(mt_room_manager_t *mgr);

/* Get all room names */
int mt_room_manager_list_rooms(mt_room_manager_t *mgr, char rooms[][MT_MAX_ROOM_LEN], int max_rooms);

#endif /* MESHTALK_ROOM_H */
