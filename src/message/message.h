#pragma once

#include "../common.h"
#include "../util/json.h"

int  message_serialize(const message_t* msg, char* out, size_t out_size);
int  message_deserialize(const char* json, size_t len, message_t* msg);
void message_generate_id(char* out_id);
bool message_is_duplicate(const char* msg_id);
void message_cache_add(const char* msg_id);
void message_cache_cleanup(void);

int  message_list_add(message_list_t* list, const message_t* msg);
int  message_list_get(message_list_t* list, int limit, message_t* out);
void message_list_free(message_list_t* list);

void message_flood(const message_t* msg, const char* exclude_id);
