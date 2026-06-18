#ifndef MESHTALK_UI_H
#define MESHTALK_UI_H

#include "message.h"
#include "presence.h"
#include <stdbool.h>

/* UI event types */
typedef enum {
    MT_UI_EVT_NONE,
    MT_UI_EVT_QUIT,
    MT_UI_EVT_SEND,
    MT_UI_EVT_COMMAND,
    MT_UI_EVT_RESIZE
} mt_ui_evt_type_t;

/* UI event */
typedef struct {
    mt_ui_evt_type_t type;
    char data[MT_MAX_TEXT_LEN];  /* Message text or command */
} mt_ui_event_t;

/* Chat message for display */
typedef struct {
    char sender_name[MT_MAX_USERNAME_LEN];
    char room[MT_MAX_ROOM_LEN];
    uint32_t timestamp;
    char payload[MT_MAX_TEXT_LEN];
    bool is_system;  /* System messages (joins, leaves, etc.) */
    bool is_private;
} mt_display_message_t;

/* UI context */
typedef struct mt_ui mt_ui_t;

/* Create UI */
mt_ui_t *mt_ui_create(void);

/* Destroy UI */
void mt_ui_destroy(mt_ui_t *ui);

/* Initialize the terminal UI */
int mt_ui_init(mt_ui_t *ui, const char *node_id, const char *username);

/* Shutdown the terminal UI */
void mt_ui_shutdown(mt_ui_t *ui);

/* Main UI loop - returns event */
mt_ui_event_t mt_ui_poll(mt_ui_t *ui, int timeout_ms);

/* Add a chat message to the display */
void mt_ui_add_message(mt_ui_t *ui, const mt_display_message_t *msg);

/* Add a system message */
void mt_ui_add_system(mt_ui_t *ui, const char *format, ...);

/* Update online users list */
void mt_ui_update_users(mt_ui_t *ui, const mt_presence_user_t *users, int count);

/* Set current room display */
void mt_ui_set_room(mt_ui_t *ui, const char *room);

/* Set status bar text */
void mt_ui_set_status(mt_ui_t *ui, const char *format, ...);

/* Refresh the display */
void mt_ui_refresh(mt_ui_t *ui);

/* Get input line (blocking) */
const char *mt_ui_get_input(mt_ui_t *ui);

/* Check for pending input (non-blocking) */
bool mt_ui_has_input(mt_ui_t *ui);

#endif /* MESHTALK_UI_H */
