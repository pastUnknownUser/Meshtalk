#pragma once

#include "../common.h"
#include "../crypto/crypto.h"
#include <pthread.h>

typedef struct room_s {
    char name[MAX_ROOM_LEN + 1];
    bool is_joined;
    bool is_encrypted;
    unsigned char room_key[ROOM_KEY_LEN];
    uint64_t created_ms;
    message_list_t history;
    struct room_s* next;
} room_t;

extern pthread_mutex_t g_rooms_lock;

void room_init(void);
void room_cleanup(void);

int  room_create(const char* name);
int  room_create_encrypted(const char* name);
int  room_join(const char* name);
int  room_leave(const char* name);
bool room_is_joined(const char* name);
bool room_is_encrypted(const char* name);
const char* room_current(void);
void room_set_current(const char* name);

room_t* room_find(const char* name);
int  room_count(void);
room_t* room_get(int index);

int  room_set_key(const char* name, const unsigned char* key);
int  room_get_key(const char* name, unsigned char* out_key);

void room_add_history(const char* room_name, const message_t* msg);
int  room_get_history(const char* room_name, int limit, message_t* out);

void room_lock(void);
void room_unlock(void);
