#include "room.h"
#include "../message/message.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

pthread_mutex_t g_rooms_lock = PTHREAD_MUTEX_INITIALIZER;

static room_t* g_rooms = NULL;
static int g_room_count = 0;
static char g_current_room[MAX_ROOM_LEN + 1] = "general";

void room_init(void) {
    g_rooms = NULL;
    g_room_count = 0;
    room_create("general");
    room_join("general");
}

void room_cleanup(void) {
    pthread_mutex_lock(&g_rooms_lock);
    room_t* cur = g_rooms;
    while (cur) {
        room_t* tmp = cur;
        cur = cur->next;
        message_list_free(&tmp->history);
        free(tmp);
    }
    g_rooms = NULL;
    g_room_count = 0;
    pthread_mutex_unlock(&g_rooms_lock);
}

int room_create(const char* name) {
    if (!name || !*name) return -1;
    if (room_find(name)) return 0;

    room_t* r = (room_t*)calloc(1, sizeof(room_t));
    strncpy(r->name, name, MAX_ROOM_LEN);
    r->name[MAX_ROOM_LEN] = '\0';
    r->is_joined = false;
    r->is_encrypted = false;
    r->created_ms = now_ms();
    r->history.head = r->history.tail = NULL;
    r->history.count = 0;

    pthread_mutex_lock(&g_rooms_lock);
    r->next = g_rooms;
    g_rooms = r;
    g_room_count++;
    pthread_mutex_unlock(&g_rooms_lock);
    return 0;
}

int room_create_encrypted(const char* name) {
    if (!name || !*name) return -1;

    room_t* r = room_find(name);
    if (r) return r->is_encrypted ? 0 : -1;

    r = (room_t*)calloc(1, sizeof(room_t));
    strncpy(r->name, name, MAX_ROOM_LEN);
    r->name[MAX_ROOM_LEN] = '\0';
    r->is_joined = false;
    r->is_encrypted = true;
    crypto_generate_room_key(r->room_key);
    r->created_ms = now_ms();
    r->history.head = r->history.tail = NULL;
    r->history.count = 0;

    pthread_mutex_lock(&g_rooms_lock);
    r->next = g_rooms;
    g_rooms = r;
    g_room_count++;
    pthread_mutex_unlock(&g_rooms_lock);
    return 0;
}

int room_join(const char* name) {
    room_t* r = room_find(name);
    if (!r) {
        room_create(name);
        r = room_find(name);
        if (!r) return -1;
    }
    r->is_joined = true;
    strncpy(g_current_room, name, MAX_ROOM_LEN);
    g_current_room[MAX_ROOM_LEN] = '\0';
    return 0;
}

int room_leave(const char* name) {
    room_t* r = room_find(name);
    if (!r) return -1;
    r->is_joined = false;
    if (strcmp(g_current_room, name) == 0) {
        strncpy(g_current_room, "general", MAX_ROOM_LEN);
    }
    return 0;
}

bool room_is_encrypted(const char* name) {
    room_t* r = room_find(name);
    return r ? r->is_encrypted : false;
}

int room_set_key(const char* name, const unsigned char* key) {
    room_t* r = room_find(name);
    if (!r) return -1;
    pthread_mutex_lock(&g_rooms_lock);
    memcpy(r->room_key, key, ROOM_KEY_LEN);
    r->is_encrypted = true;
    pthread_mutex_unlock(&g_rooms_lock);
    return 0;
}

int room_get_key(const char* name, unsigned char* out_key) {
    room_t* r = room_find(name);
    if (!r || !r->is_encrypted) return -1;
    pthread_mutex_lock(&g_rooms_lock);
    memcpy(out_key, r->room_key, ROOM_KEY_LEN);
    pthread_mutex_unlock(&g_rooms_lock);
    return 0;
}

bool room_is_joined(const char* name) {
    room_t* r = room_find(name);
    return r ? r->is_joined : false;
}

const char* room_current(void) {
    return g_current_room;
}

void room_set_current(const char* name) {
    if (name) {
        strncpy(g_current_room, name, MAX_ROOM_LEN);
        g_current_room[MAX_ROOM_LEN] = '\0';
    }
}

room_t* room_find(const char* name) {
    if (!name || !*name) return NULL;
    pthread_mutex_lock(&g_rooms_lock);
    room_t* cur = g_rooms;
    while (cur) {
        if (strcmp(cur->name, name) == 0) {
            pthread_mutex_unlock(&g_rooms_lock);
            return cur;
        }
        cur = cur->next;
    }
    pthread_mutex_unlock(&g_rooms_lock);
    return NULL;
}

int room_count(void) {
    return g_room_count;
}

room_t* room_get(int index) {
    pthread_mutex_lock(&g_rooms_lock);
    int i = 0;
    room_t* cur = g_rooms;
    while (cur) {
        if (i == index) {
            pthread_mutex_unlock(&g_rooms_lock);
            return cur;
        }
        cur = cur->next;
        i++;
    }
    pthread_mutex_unlock(&g_rooms_lock);
    return NULL;
}

void room_add_history(const char* room_name, const message_t* msg) {
    room_t* r = room_find(room_name);
    if (r) message_list_add(&r->history, msg);
}

int room_get_history(const char* room_name, int limit, message_t* out) {
    room_t* r = room_find(room_name);
    if (!r) return 0;
    return message_list_get(&r->history, limit, out);
}

void room_lock(void) {
    pthread_mutex_lock(&g_rooms_lock);
}

void room_unlock(void) {
    pthread_mutex_unlock(&g_rooms_lock);
}
