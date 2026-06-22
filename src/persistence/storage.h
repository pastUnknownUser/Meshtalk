#pragma once

#include "../common.h"

#define CONFIG_FILE "meshtalk.json"

int  storage_save_config(const char* node_id, const char* username);
int  storage_load_config(char* node_id, size_t id_size, char* username, size_t uname_size);

int  storage_save_peers(void);
int  storage_load_peers(void);

int  storage_save_history(const char* room_name, const message_list_t* history);
int  storage_load_history(const char* room_name, message_list_t* history);

int  storage_save_identity(const unsigned char* sk, size_t sk_size,
                            const unsigned char* pk, size_t pk_size);
int  storage_load_identity(unsigned char* sk, size_t sk_size,
                            unsigned char* pk, size_t pk_size);

int  storage_add_trusted_key(const char* node_id, const char* public_key_b64);
int  storage_is_key_trusted(const char* node_id);
int  storage_save_trusted_keys(void);
int  storage_load_trusted_keys(void);

int  storage_save_rooms(void);
int  storage_load_rooms(void);
