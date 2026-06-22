#include "message.h"
#include "../util/uuid.h"
#include "../peer/peer.h"
#include "../net/net.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#define CACHE_BUCKETS 256

typedef struct cache_entry_s {
    char msg_id[UUID_STR_LEN + 1];
    uint64_t added_ms;
    struct cache_entry_s* next;
} cache_entry_t;

static cache_entry_t* g_cache[CACHE_BUCKETS];
static int g_cache_count = 0;

static uint32_t hash_str(const char* s) {
    uint32_t h = 5381;
    while (*s) h = ((h << 5) + h) + (unsigned char)*s++;
    return h % CACHE_BUCKETS;
}

void message_generate_id(char* out_id) {
    uuid_generate(out_id);
}

int message_serialize(const message_t* msg, char* out, size_t out_size) {
    json_builder_t jb;
    json_init(&jb);
    json_add_int(&jb, "type", msg->type);
    json_add_string(&jb, "id", msg->msg_id);
    json_add_string(&jb, "sender_id", msg->sender_id);
    json_add_string(&jb, "sender_name", msg->sender_name);
    json_add_uint64(&jb, "time", msg->timestamp_ms);
    json_add_string(&jb, "room", msg->room);
    json_add_string(&jb, "target", msg->target_id);
    json_add_string(&jb, "payload", msg->payload);
    if (msg->public_key[0])
        json_add_string(&jb, "public_key", msg->public_key);
    if (msg->encrypted_key[0])
        json_add_string(&jb, "encrypted_key", msg->encrypted_key);
    if (msg->nonce[0])
        json_add_string(&jb, "nonce", msg->nonce);

    const char* json = json_build(&jb);
    size_t len = strlen(json);
    if (len + 1 > out_size) {
        json_free(&jb);
        return -1;
    }
    memcpy(out, json, len + 1);
    json_free(&jb);
    return (int)len;
}

int message_deserialize(const char* json, size_t len, message_t* msg) {
    (void)len;
    memset(msg, 0, sizeof(message_t));

    int64_t type;
    if (json_parse_int(json, "type", &type) != 0) return -1;
    msg->type = (message_type_t)type;

    if (json_parse_string(json, "id", msg->msg_id, sizeof(msg->msg_id)) != 0) return -1;
    if (json_parse_string(json, "sender_id", msg->sender_id, sizeof(msg->sender_id)) != 0) return -1;
    json_parse_string(json, "sender_name", msg->sender_name, sizeof(msg->sender_name));

    uint64_t t;
    if (json_parse_uint64(json, "time", &t) == 0) msg->timestamp_ms = t;

    json_parse_string(json, "room", msg->room, sizeof(msg->room));
    json_parse_string(json, "target", msg->target_id, sizeof(msg->target_id));
    json_parse_string(json, "payload", msg->payload, sizeof(msg->payload));
    json_parse_string(json, "public_key", msg->public_key, sizeof(msg->public_key));
    json_parse_string(json, "encrypted_key", msg->encrypted_key, sizeof(msg->encrypted_key));
    json_parse_string(json, "nonce", msg->nonce, sizeof(msg->nonce));

    return 0;
}

bool message_is_duplicate(const char* msg_id) {
    if (!msg_id || !*msg_id) return false;
    uint32_t h = hash_str(msg_id);
    cache_entry_t* e = g_cache[h];
    while (e) {
        if (strcmp(e->msg_id, msg_id) == 0) return true;
        e = e->next;
    }
    return false;
}

void message_cache_add(const char* msg_id) {
    if (message_is_duplicate(msg_id)) return;

    if (g_cache_count >= MSG_CACHE_SIZE) {
        message_cache_cleanup();
    }

    uint32_t h = hash_str(msg_id);
    cache_entry_t* e = (cache_entry_t*)malloc(sizeof(cache_entry_t));
    strncpy(e->msg_id, msg_id, UUID_STR_LEN);
    e->msg_id[UUID_STR_LEN] = '\0';
    e->added_ms = now_ms();
    e->next = g_cache[h];
    g_cache[h] = e;
    g_cache_count++;
}

void message_cache_cleanup(void) {
    uint64_t cutoff = now_ms() - MSG_CACHE_TTL_MS;
    for (int i = 0; i < CACHE_BUCKETS; i++) {
        cache_entry_t** pp = &g_cache[i];
        while (*pp) {
            if ((*pp)->added_ms < cutoff) {
                cache_entry_t* tmp = *pp;
                *pp = (*pp)->next;
                free(tmp);
                g_cache_count--;
            } else {
                pp = &(*pp)->next;
            }
        }
    }
}

int message_list_add(message_list_t* list, const message_t* msg) {
    message_node_t* node = (message_node_t*)malloc(sizeof(message_node_t));
    memcpy(&node->msg, msg, sizeof(message_t));
    node->received_ms = now_ms();
    node->next = NULL;

    if (list->tail) {
        list->tail->next = node;
    } else {
        list->head = node;
    }
    list->tail = node;
    list->count++;

    while (list->count > MAX_MESSAGE_HISTORY) {
        message_node_t* tmp = list->head;
        list->head = list->head->next;
        free(tmp);
        list->count--;
    }
    return 0;
}

int message_list_get(message_list_t* list, int limit, message_t* out) {
    int n = 0;
    int skip = list->count > limit ? list->count - limit : 0;
    message_node_t* cur = list->head;
    for (int i = 0; cur && i < list->count; i++) {
        if (i >= skip && n < limit) {
            memcpy(&out[n], &cur->msg, sizeof(message_t));
            n++;
        }
        cur = cur->next;
    }
    return n;
}

void message_list_free(message_list_t* list) {
    message_node_t* cur = list->head;
    while (cur) {
        message_node_t* tmp = cur;
        cur = cur->next;
        free(tmp);
    }
    list->head = list->tail = NULL;
    list->count = 0;
}

void message_flood(const message_t* msg, const char* exclude_id) {
    char serialized[MAX_PACKET_SIZE];
    int slen = message_serialize(msg, serialized, sizeof(serialized));
    if (slen <= 0) return;

    /* Collect sockets under lock, send outside lock */
    net_socket_t socks[MAX_PEERS];
    int nsocks = 0;

    peer_lock();
    for (int i = 0; i < peer_count(); i++) {
        peer_t* p = peer_get(i);
        if (!p || !p->connected) continue;
        if (exclude_id && strcmp(p->node_id, exclude_id) == 0) continue;
        if (nsocks < MAX_PEERS) socks[nsocks++] = p->sock;
    }
    peer_unlock();

    if (nsocks == 0) return;

    uint32_t nlen = htonl((uint32_t)slen + 4);
    for (int i = 0; i < nsocks; i++) {
        net_send_all(socks[i], (const char*)&nlen, 4);
        net_send_all(socks[i], serialized, (size_t)slen);
    }
}
