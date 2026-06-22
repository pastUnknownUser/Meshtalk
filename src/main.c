#include "common.h"
#include "net/net.h"
#include "util/uuid.h"
#include "util/json.h"
#include "message/message.h"
#include "peer/peer.h"
#include "discovery/discovery.h"
#include "room/room.h"
#include "persistence/storage.h"
#include "crypto/crypto.h"
#include "tui/tui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

static tui_t g_tui;
static char g_node_id[UUID_STR_LEN + 1];
static char g_username[MAX_USERNAME_LEN + 1];
static net_socket_t g_listen_sock = INVALID_NET_SOCKET;
static net_socket_t g_udp_sock = INVALID_NET_SOCKET;
static volatile bool g_running = false;
static pthread_t g_listener_thread;
static pthread_t g_connector_thread;

typedef struct msg_event_s {
    message_t msg;
    char from_id[UUID_STR_LEN + 1];
    struct msg_event_s* next;
} msg_event_t;

static msg_event_t* g_event_head = NULL;
static msg_event_t* g_event_tail = NULL;
static pthread_mutex_t g_event_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_event_cond = PTHREAD_COND_INITIALIZER;

static void push_event(const message_t* msg, const char* from_id) {
    msg_event_t* e = (msg_event_t*)malloc(sizeof(msg_event_t));
    memcpy(&e->msg, msg, sizeof(message_t));
    strncpy(e->from_id, from_id, UUID_STR_LEN);
    e->from_id[UUID_STR_LEN] = '\0';
    e->next = NULL;

    pthread_mutex_lock(&g_event_lock);
    if (g_event_tail) {
        g_event_tail->next = e;
    } else {
        g_event_head = e;
    }
    g_event_tail = e;
    pthread_cond_signal(&g_event_cond);
    pthread_mutex_unlock(&g_event_lock);
}

static msg_event_t* pop_event(void) {
    pthread_mutex_lock(&g_event_lock);
    if (!g_event_head) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        ts.tv_sec += 0;
        ts.tv_nsec += 100000000;
        pthread_cond_timedwait(&g_event_cond, &g_event_lock, &ts);
    }
    msg_event_t* e = g_event_head;
    if (e) {
        g_event_head = e->next;
        if (!g_event_head) g_event_tail = NULL;
    }
    pthread_mutex_unlock(&g_event_lock);
    return e;
}

static void process_command(const char* cmd);
static void process_message(const message_t* msg, const char* from_id);
static void* listener_thread(void* arg);
static void* peer_handler_thread(void* arg);
static void* connector_thread(void* arg);

static void handle_signal(int sig) {
    (void)sig;
    g_running = false;
}

static bool is_valid_username(const char* name) {
    if (!name || !*name) return false;
    size_t len = strlen(name);
    if (len < 1 || len > MAX_USERNAME_LEN) return false;
    for (size_t i = 0; i < len; i++) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
              c == '_' || c == '-' || c == ' '))
            return false;
    }
    return true;
}

static int app_init(void) {
    if (storage_load_config(g_node_id, sizeof(g_node_id),
                            g_username, sizeof(g_username)) != 0) {
        uuid_generate(g_node_id);
        strncpy(g_username, "anonymous", MAX_USERNAME_LEN);
        storage_save_config(g_node_id, g_username);
    }

    if (!uuid_validate(g_node_id)) {
        uuid_generate(g_node_id);
        storage_save_config(g_node_id, g_username);
    }

    if (!is_valid_username(g_username)) {
        strncpy(g_username, "anonymous", MAX_USERNAME_LEN);
        storage_save_config(g_node_id, g_username);
    }

    if (crypto_init() != 0) {
        fprintf(stderr, "Failed to init crypto\n");
        return -1;
    }

    if (crypto_load_identity() != 0) {
        if (crypto_generate_identity() != 0) {
            fprintf(stderr, "Failed to generate identity\n");
            return -1;
        }
    }

    if (net_init() != 0) return -1;

    peer_init();
    room_init();

    storage_load_rooms();
    storage_load_trusted_keys();

    if (net_tcp_listen(TCP_PORT, &g_listen_sock) != 0) {
        fprintf(stderr, "Failed to listen on port %d\n", TCP_PORT);
        return -1;
    }

    if (discovery_init(&g_udp_sock, g_node_id, g_username, TCP_PORT) != 0) {
        fprintf(stderr, "Failed to init discovery\n");
        return -1;
    }

    if (tui_init(&g_tui) != 0) {
        fprintf(stderr, "Failed to init TUI\n");
        return -1;
    }

    storage_load_peers();
    return 0;
}

static void app_cleanup(void) {
    g_running = false;
    discovery_cleanup();
    tui_cleanup(&g_tui);
    if (g_listen_sock != INVALID_NET_SOCKET) {
        net_close(g_listen_sock);
        g_listen_sock = INVALID_NET_SOCKET;
    }
    peer_disconnect_all();
    peer_cleanup();
    storage_save_rooms();
    storage_save_trusted_keys();
    room_cleanup();
    net_cleanup();
    storage_save_config(g_node_id, g_username);
    storage_save_peers();
    crypto_cleanup();
}

static char* build_user_list(void) {
    static char buf[4096];
    buf[0] = '\0';
    size_t pos = 0;

    peer_lock();
    for (int i = 0; ; i++) {
        peer_t* p = peer_get(i);
        if (!p) break;
        if (!p->connected) continue;
        size_t len = strlen(p->username);
        if (pos + len + 2 > sizeof(buf)) break;
        memcpy(buf + pos, p->username, len);
        pos += len;
        buf[pos++] = '\0';
    }
    peer_unlock();

    size_t mylen = strlen(g_username);
    if (pos + mylen + 1 <= sizeof(buf)) {
        memcpy(buf + pos, g_username, mylen);
        pos += mylen;
        buf[pos++] = '\0';
        buf[pos] = '\0';
    }

    return buf;
}

/* Count connected peers (lock internally) */
static int connected_peer_count(void) {
    int c = 0;
    peer_lock();
    for (int i = 0; ; i++) {
        peer_t* p = peer_get(i);
        if (!p) break;
        if (p->connected) c++;
    }
    peer_unlock();
    return c;
}

static void send_message(const char* text, message_type_t type, const char* target, const char* room) {
    if (!text || !*text) return;

    message_t msg;
    message_t hist_msg;
    memset(&msg, 0, sizeof(msg));
    message_generate_id(msg.msg_id);
    strncpy(msg.sender_id, g_node_id, UUID_STR_LEN);
    strncpy(msg.sender_name, g_username, MAX_USERNAME_LEN);
    msg.timestamp_ms = now_ms();
    msg.type = type;
    strncpy(msg.room, room ? room : room_current(), MAX_ROOM_LEN);
    if (target) strncpy(msg.target_id, target, UUID_STR_LEN);

    if (type == MSG_PRIV && target) {
        char pk_b64[BASE64_PK_LEN] = {0};
        if (peer_get_public_key(target, pk_b64, sizeof(pk_b64))) {
            unsigned char pk[32];
            if (crypto_b64_decode(pk_b64, pk, sizeof(pk)) == 0) {
                if (crypto_encrypt_dm(text, strlen(text), pk,
                                       msg.payload, sizeof(msg.payload),
                                       msg.nonce, sizeof(msg.nonce),
                                       msg.encrypted_key, sizeof(msg.encrypted_key)) == 0) {
                    msg.type = PRIV_ENC;
                }
            }
        }
    }

    if (type == MSG_CHAT) {
        unsigned char room_key[ROOM_KEY_LEN];
        if (room_get_key(msg.room, room_key) == 0) {
            if (crypto_encrypt_room_msg(text, strlen(text), room_key,
                                         msg.payload, sizeof(msg.payload),
                                         msg.nonce, sizeof(msg.nonce)) == 0) {
                msg.type = ROOM_ENC;
            }
        }
    }

    if (msg.type != PRIV_ENC && msg.type != ROOM_ENC) {
        strncpy(msg.payload, text, MAX_PAYLOAD_LEN - 1);
    }

    /* Save plaintext copy for history display */
    memcpy(&hist_msg, &msg, sizeof(msg));
    hist_msg.type = type;
    strncpy(hist_msg.payload, text, MAX_PAYLOAD_LEN - 1);

    message_cache_add(msg.msg_id);
    message_flood(&msg, NULL);

    if (type == MSG_CHAT || type == MSG_PRIV) {
        char display[512];
        if (msg.type == PRIV_ENC)
            snprintf(display, sizeof(display), "[E2EE to %s] %s", target, text);
        else if (type == MSG_PRIV)
            snprintf(display, sizeof(display), "[to %s] %s", target, text);
        else
            snprintf(display, sizeof(display), "%s", text);
        tui_add_message(&g_tui, g_username, display);
        room_add_history(hist_msg.room, &hist_msg);
    }
}

static void process_message(const message_t* msg, const char* from_id) {
    if (!msg || !msg->msg_id[0]) return;

    if (message_is_duplicate(msg->msg_id)) return;
    message_cache_add(msg->msg_id);

    switch (msg->type) {
        case KEY_EXCHANGE: {
            if (msg->public_key[0]) {
                peer_set_public_key(msg->sender_id, msg->public_key);
                char display[128];
                snprintf(display, sizeof(display),
                         "*** Received public key from %s", msg->sender_name);
                tui_add_message(&g_tui, NULL, display);
            }
            message_flood(msg, from_id);
            break;
        }
        case PRIV_ENC: {
            if (strcmp(msg->target_id, g_node_id) == 0) {
                char plaintext[MAX_PAYLOAD_LEN] = {0};
                int ptlen = crypto_decrypt_dm(msg->payload, msg->nonce,
                                               msg->encrypted_key,
                                               plaintext, sizeof(plaintext));
                if (ptlen > 0) {
                    char display[512];
                    snprintf(display, sizeof(display), "[E2EE] %s", plaintext);
                    tui_add_message(&g_tui, msg->sender_name, display);

                    message_t hist_msg = *msg;
                    strncpy(hist_msg.payload, plaintext, MAX_PAYLOAD_LEN - 1);
                    room_add_history(msg->room, &hist_msg);
                } else {
                    tui_add_message(&g_tui, NULL,
                                     "*** Failed to decrypt private message");
                }
            }
            message_flood(msg, from_id);
            break;
        }
        case ROOM_KEY: {
            unsigned char room_key[ROOM_KEY_LEN];
            if (strcmp(msg->target_id, g_node_id) == 0 &&
                crypto_unseal_room_key(msg->encrypted_key, room_key) == 0) {
                room_set_key(msg->room, room_key);
                room_create(msg->room);
                char display[128];
                snprintf(display, sizeof(display),
                         "*** Received key for encrypted room '%s'", msg->room);
                tui_add_message(&g_tui, NULL, display);
            }
            message_flood(msg, from_id);
            break;
        }
        case ROOM_ENC: {
            unsigned char room_key[ROOM_KEY_LEN];
            if (room_get_key(msg->room, room_key) == 0) {
                char plaintext[MAX_PAYLOAD_LEN] = {0};
                int ptlen = crypto_decrypt_room_msg(msg->payload, msg->nonce,
                                                     room_key,
                                                     plaintext, sizeof(plaintext));
                if (ptlen > 0) {
                    char display[512];
                    snprintf(display, sizeof(display), "[E2EE] %s", plaintext);
                    tui_add_message(&g_tui, msg->sender_name, display);

                    message_t hist_msg = *msg;
                    strncpy(hist_msg.payload, plaintext, MAX_PAYLOAD_LEN - 1);
                    room_add_history(msg->room, &hist_msg);
                } else {
                    tui_add_message(&g_tui, NULL,
                                     "*** Cannot decrypt room message (wrong key?)");
                }
            } else {
                char display[128];
                snprintf(display, sizeof(display),
                         "*** [Encrypted message in room '%s' - you don't have the key]",
                         msg->room);
                tui_add_message(&g_tui, NULL, display);
            }
            message_flood(msg, from_id);
            break;
        }
        case MSG_CHAT: {
            if (room_is_joined(msg->room)) {
                tui_add_message(&g_tui, msg->sender_name, msg->payload);
                room_add_history(msg->room, msg);
            }
            message_flood(msg, from_id);
            break;
        }
        case MSG_PRIV: {
            if (strcmp(msg->target_id, g_node_id) == 0) {
                tui_add_message(&g_tui, msg->sender_name, msg->payload);
            }
            message_flood(msg, from_id);
            break;
        }
        case MSG_JOIN: {
            peer_set_username(msg->sender_id, msg->sender_name);
            char buf[128];
            snprintf(buf, sizeof(buf), "*** %s joined the room", msg->sender_name);
            tui_add_message(&g_tui, NULL, buf);
            message_flood(msg, from_id);
            break;
        }
        case MSG_LEAVE: {
            char buf[128];
            snprintf(buf, sizeof(buf), "*** %s left the room", msg->sender_name);
            tui_add_message(&g_tui, NULL, buf);
            message_flood(msg, from_id);
            break;
        }
        case MSG_PING: {
            message_t pong;
            memset(&pong, 0, sizeof(pong));
            message_generate_id(pong.msg_id);
            strncpy(pong.sender_id, g_node_id, UUID_STR_LEN);
            strncpy(pong.sender_name, g_username, MAX_USERNAME_LEN);
            pong.timestamp_ms = now_ms();
            pong.type = MSG_PONG;
            message_cache_add(pong.msg_id);

            char serialized[MAX_PACKET_SIZE];
            int slen = message_serialize(&pong, serialized, sizeof(serialized));
            if (slen > 0 && from_id) {
                net_socket_t target_sock = INVALID_NET_SOCKET;
                peer_lock();
                peer_t* p = peer_find(from_id);
                if (p && p->connected) target_sock = p->sock;
                peer_unlock();
                if (target_sock != INVALID_NET_SOCKET) {
                    uint32_t nlen = htonl((uint32_t)slen + 4);
                    net_send_all(target_sock, (const char*)&nlen, 4);
                    net_send_all(target_sock, serialized, (size_t)slen);
                }
            }
            break;
        }
        case MSG_PONG: {
            peer_lock();
            peer_t* p = peer_find(msg->sender_id);
            if (p) p->last_seen_ms = now_ms();
            peer_unlock();
            break;
        }
        case MSG_INFO: {
            tui_add_message(&g_tui, NULL, msg->payload);
            break;
        }
        default:
            break;
    }
}

static void process_command(const char* cmd) {
    if (!cmd || !*cmd) return;

    if (cmd[0] != '/') {
        send_message(cmd, MSG_CHAT, NULL, room_current());
        return;
    }

    char buf[4096];
    strncpy(buf, cmd + 1, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* arg = strchr(buf, ' ');
    if (arg) {
        *arg = '\0';
        arg++;
        while (*arg == ' ') arg++;
    }

    if (strcmp(buf, "help") == 0) {
        tui_add_message(&g_tui, NULL, "Commands:");
        tui_add_message(&g_tui, NULL, "  /help            - Show this help");
        tui_add_message(&g_tui, NULL, "  /join <room>     - Join a room");
        tui_add_message(&g_tui, NULL, "  /create <room>   - Create a plain room");
        tui_add_message(&g_tui, NULL, "  /leave           - Leave current room");
        tui_add_message(&g_tui, NULL, "  /msg <user> <txt> - Private message (E2EE if key known)");
        tui_add_message(&g_tui, NULL, "  /users           - Show online users");
        tui_add_message(&g_tui, NULL, "  /nick <name>     - Change username");
        tui_add_message(&g_tui, NULL, "  /history [n]     - Show message history");
        tui_add_message(&g_tui, NULL, "  /quit            - Exit");
        tui_add_message(&g_tui, NULL, "  /ping            - Ping all peers");
        tui_add_message(&g_tui, NULL, "  /e2ee_create <room> - Create encrypted room");
        tui_add_message(&g_tui, NULL, "  /e2ee_invite <user> <room> - Invite to encrypted room");
        tui_add_message(&g_tui, NULL, "  /trust <user>    - Trust a peer's public key");
        tui_add_message(&g_tui, NULL, "  /key             - Show your public key");
    } else if (strcmp(buf, "join") == 0) {
        if (arg && *arg) {
            room_join(arg);
            tui_add_message(&g_tui, NULL, "Joined room");
            message_t msg;
            memset(&msg, 0, sizeof(msg));
            message_generate_id(msg.msg_id);
            strncpy(msg.sender_id, g_node_id, UUID_STR_LEN);
            strncpy(msg.sender_name, g_username, MAX_USERNAME_LEN);
            msg.timestamp_ms = now_ms();
            msg.type = MSG_JOIN;
            strncpy(msg.room, arg, MAX_ROOM_LEN);
            message_cache_add(msg.msg_id);
            message_flood(&msg, NULL);
        }
    } else if (strcmp(buf, "create") == 0) {
        if (arg && *arg) {
            room_create(arg);
            room_join(arg);
            tui_add_message(&g_tui, NULL, "Room created and joined");
        }
    } else if (strcmp(buf, "leave") == 0) {
        const char* cur = room_current();
        if (strcmp(cur, "general") == 0) {
            tui_add_message(&g_tui, NULL, "Cannot leave general room");
        } else {
            message_t msg;
            memset(&msg, 0, sizeof(msg));
            message_generate_id(msg.msg_id);
            strncpy(msg.sender_id, g_node_id, UUID_STR_LEN);
            strncpy(msg.sender_name, g_username, MAX_USERNAME_LEN);
            msg.timestamp_ms = now_ms();
            msg.type = MSG_LEAVE;
            strncpy(msg.room, cur, MAX_ROOM_LEN);
            message_cache_add(msg.msg_id);
            message_flood(&msg, NULL);
            room_leave(cur);
            tui_add_message(&g_tui, NULL, "Left room");
        }
    } else if (strcmp(buf, "msg") == 0) {
        if (arg) {
            char* text = strchr(arg, ' ');
            if (text) {
                *text = '\0';
                text++;
                while (*text == ' ') text++;

                char target_id[UUID_STR_LEN + 1] = {0};
                bool found = false;

                peer_lock();
                for (int i = 0; ; i++) {
                    peer_t* p = peer_get(i);
                    if (!p) break;
                    if (strcmp(p->username, arg) == 0) {
                        memcpy(target_id, p->node_id, UUID_STR_LEN);
                        found = true;
                        break;
                    }
                }
                peer_unlock();

                if (!found) {
                    peer_lock();
                    peer_t* t = peer_find(arg);
                    if (t) {
                        memcpy(target_id, t->node_id, UUID_STR_LEN);
                        found = true;
                    }
                    peer_unlock();
                }

                if (found) {
                    send_message(text, MSG_PRIV, target_id, room_current());
                } else {
                    tui_add_message(&g_tui, NULL, "User not found");
                }
            }
        }
    } else if (strcmp(buf, "users") == 0) {
        char buf2[128] = "Online: ";
        peer_lock();
        for (int i = 0; ; i++) {
            peer_t* p = peer_get(i);
            if (!p) break;
            if (p->connected) {
                strncat(buf2, p->username, sizeof(buf2) - strlen(buf2) - 1);
                strncat(buf2, " ", sizeof(buf2) - strlen(buf2) - 1);
            }
        }
        peer_unlock();
        strncat(buf2, g_username, sizeof(buf2) - strlen(buf2) - 1);
        tui_add_message(&g_tui, NULL, buf2);
    } else if (strcmp(buf, "nick") == 0) {
        if (arg && is_valid_username(arg)) {
            strncpy(g_username, arg, MAX_USERNAME_LEN);
            storage_save_config(g_node_id, g_username);
            tui_add_message(&g_tui, NULL, "Username changed");
            char status[128];
            snprintf(status, sizeof(status), "Username: %s | %d peers",
                     g_username, connected_peer_count());
            tui_set_status(&g_tui, status);
        } else {
            tui_add_message(&g_tui, NULL, "Invalid username");
        }
    } else if (strcmp(buf, "history") == 0) {
        int limit = 50;
        if (arg) limit = atoi(arg);
        if (limit < 1) limit = 1;
        if (limit > MAX_MESSAGE_HISTORY) limit = MAX_MESSAGE_HISTORY;

        message_t* hist = (message_t*)malloc(sizeof(message_t) * (size_t)limit);
        int n = room_get_history(room_current(), limit, hist);
        for (int i = 0; i < n; i++) {
            char display[1024];
            snprintf(display, sizeof(display), "[%s] <%s> %s",
                     hist[i].room, hist[i].sender_name, hist[i].payload);
            tui_add_message(&g_tui, NULL, display);
        }
        free(hist);
        if (n == 0) tui_add_message(&g_tui, NULL, "No history");
    } else if (strcmp(buf, "quit") == 0 || strcmp(buf, "exit") == 0) {
        g_running = false;
    } else if (strcmp(buf, "ping") == 0) {
        message_t msg;
        memset(&msg, 0, sizeof(msg));
        message_generate_id(msg.msg_id);
        strncpy(msg.sender_id, g_node_id, UUID_STR_LEN);
        strncpy(msg.sender_name, g_username, MAX_USERNAME_LEN);
        msg.timestamp_ms = now_ms();
        msg.type = MSG_PING;
        message_cache_add(msg.msg_id);
        message_flood(&msg, NULL);
        tui_add_message(&g_tui, NULL, "Ping sent");
    } else if (strcmp(buf, "e2ee_create") == 0) {
        if (arg && *arg) {
            if (room_create_encrypted(arg) == 0) {
                room_join(arg);
                tui_add_message(&g_tui, NULL, "Encrypted room created and joined");
            } else {
                tui_add_message(&g_tui, NULL, "Failed to create encrypted room");
            }
        }
    } else if (strcmp(buf, "e2ee_invite") == 0) {
        if (arg) {
            char* room_name = strchr(arg, ' ');
            if (room_name) {
                *room_name = '\0';
                room_name++;
                while (*room_name == ' ') room_name++;

                unsigned char room_key[ROOM_KEY_LEN];
                if (room_get_key(room_name, room_key) != 0) {
                    tui_add_message(&g_tui, NULL, "Room is not encrypted or not found");
                } else {
                    char target_id[UUID_STR_LEN + 1] = {0};
                    bool found = false;
                    peer_lock();
                    for (int i = 0; ; i++) {
                        peer_t* p = peer_get(i);
                        if (!p) break;
                        if (strcmp(p->username, arg) == 0) {
                            memcpy(target_id, p->node_id, UUID_STR_LEN);
                            found = true;
                            break;
                        }
                    }
                    peer_unlock();

                    if (!found) tui_add_message(&g_tui, NULL, "User not found");
                    else {
                        char pk_b64[BASE64_PK_LEN] = {0};
                        if (!peer_get_public_key(target_id, pk_b64, sizeof(pk_b64))) {
                            tui_add_message(&g_tui, NULL, "User has no public key");
                        } else {
                            unsigned char pk[32];
                            if (crypto_b64_decode(pk_b64, pk, sizeof(pk)) != 0) {
                                tui_add_message(&g_tui, NULL, "Invalid public key");
                            } else {
                                char ek_b64[BASE64_ENCKEY_LEN] = {0};
                                if (crypto_seal_room_key(room_key, pk, ek_b64, sizeof(ek_b64)) != 0) {
                                    tui_add_message(&g_tui, NULL, "Failed to encrypt room key");
                                } else {
                                    message_t invite;
                                    memset(&invite, 0, sizeof(invite));
                                    message_generate_id(invite.msg_id);
                                    strncpy(invite.sender_id, g_node_id, UUID_STR_LEN);
                                    strncpy(invite.sender_name, g_username, MAX_USERNAME_LEN);
                                    invite.timestamp_ms = now_ms();
                                    invite.type = ROOM_KEY;
                                    strncpy(invite.room, room_name, MAX_ROOM_LEN);
                                    strncpy(invite.target_id, target_id, UUID_STR_LEN);
                                    strncpy(invite.encrypted_key, ek_b64, BASE64_ENCKEY_LEN - 1);
                                    message_cache_add(invite.msg_id);
                                    message_flood(&invite, NULL);

                                    char display[128];
                                    snprintf(display, sizeof(display),
                                             "Invited %s to encrypted room '%s'", arg, room_name);
                                    tui_add_message(&g_tui, NULL, display);
                                }
                            }
                        }
                    }
                }
            }
        }
    } else if (strcmp(buf, "trust") == 0) {
        if (arg && *arg) {
            char target_id[UUID_STR_LEN + 1] = {0};
            bool found = false;
            peer_lock();
            for (int i = 0; ; i++) {
                peer_t* p = peer_get(i);
                if (!p) break;
                if (strcmp(p->username, arg) == 0) {
                    memcpy(target_id, p->node_id, UUID_STR_LEN);
                    found = true;
                    break;
                }
            }
            peer_unlock();
            if (!found) {
                tui_add_message(&g_tui, NULL, "User not found");
            } else {
                char pk_b64[BASE64_PK_LEN] = {0};
                if (!peer_get_public_key(target_id, pk_b64, sizeof(pk_b64))) {
                    tui_add_message(&g_tui, NULL, "User has no public key");
                } else {
                    crypto_verify_and_trust(target_id, pk_b64);
                    tui_add_message(&g_tui, NULL, "Public key trusted");
                }
            }
        }
    } else if (strcmp(buf, "key") == 0) {
        if (g_identity.loaded) {
            char display[128];
            snprintf(display, sizeof(display), "Your public key: %s",
                     g_identity.public_key_b64);
            tui_add_message(&g_tui, NULL, display);
        } else {
            tui_add_message(&g_tui, NULL, "No identity loaded");
        }
    } else {
        tui_add_message(&g_tui, NULL, "Unknown command. Type /help");
    }
}

static void* listener_thread(void* arg) {
    (void)arg;
    while (g_running) {
        net_conn_t conn;
        if (net_tcp_accept(g_listen_sock, &conn, 500) != 0) continue;
        if (!g_running) { net_close(conn.sock); break; }

        char ident[512];
        uint32_t len;
        if (net_recv_all(conn.sock, &len, 4, 5000) != 4) {
            net_close(conn.sock); continue;
        }
        len = ntohl(len);
        if (len > sizeof(ident)) { net_close(conn.sock); continue; }
        if (net_recv_all(conn.sock, ident, len, 5000) != (int)len) {
            net_close(conn.sock); continue;
        }
        ident[len] = '\0';

        message_t ident_msg;
        if (message_deserialize(ident, len, &ident_msg) != 0 ||
            strcmp(ident_msg.msg_id, "IDENT") != 0) {
            net_close(conn.sock); continue;
        }

        if (ident_msg.public_key[0]) {
            peer_set_public_key(ident_msg.sender_id, ident_msg.public_key);
        }

        bool already_connected = false;
        peer_lock();
        peer_t* existing = peer_find(ident_msg.sender_id);
        if (existing && existing->connected) already_connected = true;
        peer_unlock();

        if (already_connected) { net_close(conn.sock); continue; }

        message_t our_ident;
        memset(&our_ident, 0, sizeof(our_ident));
        strncpy(our_ident.msg_id, "IDENT", sizeof(our_ident.msg_id));
        strncpy(our_ident.sender_id, g_node_id, UUID_STR_LEN);
        strncpy(our_ident.sender_name, g_username, MAX_USERNAME_LEN);
        our_ident.type = MSG_INFO;
        if (g_identity.loaded) {
            strncpy(our_ident.public_key, g_identity.public_key_b64,
                    BASE64_PK_LEN - 1);
        }

        char buf[MAX_PACKET_SIZE];
        int slen = message_serialize(&our_ident, buf, sizeof(buf));
        if (slen > 0) {
            uint32_t nlen = htonl((uint32_t)slen + 4);
            net_send_all(conn.sock, (const char*)&nlen, 4);
            net_send_all(conn.sock, buf, (size_t)slen);
        }

        peer_add_connection(ident_msg.sender_id, ident_msg.sender_name, conn.sock, false);

        pthread_t handler_thread;
        pthread_create(&handler_thread, NULL, peer_handler_thread,
                       (void*)(intptr_t)conn.sock);
        pthread_detach(handler_thread);

        char status[128];
        snprintf(status, sizeof(status), "Connected: %s", ident_msg.sender_name);
        tui_set_status(&g_tui, status);
    }
    return NULL;
}

static void* peer_handler_thread(void* arg) {
    net_socket_t sock = (net_socket_t)(intptr_t)arg;
    char buf[MAX_PACKET_SIZE];

    while (g_running) {
        bool still_connected = false;
        peer_lock();
        peer_t* p = peer_find_by_sock(sock);
        if (p && p->connected) still_connected = true;
        peer_unlock();
        if (!still_connected) break;

        uint32_t len;
        int n = net_recv_all(sock, &len, 4, 5000);
        if (n != 4) break;

        len = ntohl(len);
        if (len == 0 || len > MAX_PACKET_SIZE) break;

        n = net_recv_all(sock, buf, len, 5000);
        if (n != (int)len) break;

        buf[len] = '\0';

        message_t msg;
        if (message_deserialize(buf, len, &msg) != 0) continue;

        if (strcmp(msg.msg_id, "IDENT") == 0) continue;

        push_event(&msg, msg.sender_id);
    }

    peer_lock();
    peer_t* peer = peer_find_by_sock(sock);
    if (peer) {
        char msg[128];
        snprintf(msg, sizeof(msg), "*** %s disconnected", peer->username);
        tui_add_message(&g_tui, NULL, msg);
        peer->connected = false;
        if (peer->sock != INVALID_NET_SOCKET) {
            net_close(peer->sock);
            peer->sock = INVALID_NET_SOCKET;
        }
    } else if (sock != INVALID_NET_SOCKET) {
        peer_unlock();
        net_close(sock);
        return NULL;
    }
    peer_unlock();

    return NULL;
}

typedef struct {
    char node_id[UUID_STR_LEN + 1];
    char addr[INET6_ADDRSTRLEN];
    int port;
} reconnect_target_t;

static void* connector_thread(void* arg) {
    (void)arg;
    while (g_running) {
        reconnect_target_t targets[MAX_PEERS];
        int ntargets = 0;

        peer_lock();
        for (int i = 0; ; i++) {
            peer_t* p = peer_get(i);
            if (!p) break;
            if (p->connected) continue;
            if (now_ms() - p->last_seen_ms < RECONNECT_INTERVAL_MS) continue;
            if (ntargets < MAX_PEERS) {
                memcpy(targets[ntargets].node_id, p->node_id, UUID_STR_LEN + 1);
                strncpy(targets[ntargets].addr, p->addr, INET6_ADDRSTRLEN - 1);
                targets[ntargets].port = p->port;
                ntargets++;
            }
        }
        peer_unlock();

        for (int i = 0; i < ntargets && g_running; i++) {
            net_socket_t sock = peer_connect_to(targets[i].addr, targets[i].port);
            if (sock < 0) continue;

            int flag = 1;
            setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&flag, sizeof(flag));

            message_t ident_msg;
            memset(&ident_msg, 0, sizeof(ident_msg));
            strncpy(ident_msg.msg_id, "IDENT", sizeof(ident_msg.msg_id));
            strncpy(ident_msg.sender_id, g_node_id, UUID_STR_LEN);
            strncpy(ident_msg.sender_name, g_username, MAX_USERNAME_LEN);
            ident_msg.type = MSG_INFO;
            if (g_identity.loaded) {
                strncpy(ident_msg.public_key, g_identity.public_key_b64,
                        BASE64_PK_LEN - 1);
            }

            char buf[MAX_PACKET_SIZE];
            int slen = message_serialize(&ident_msg, buf, sizeof(buf));
            if (slen > 0) {
                uint32_t nlen = htonl((uint32_t)slen + 4);
                net_send_all(sock, (const char*)&nlen, 4);
                net_send_all(sock, buf, (size_t)slen);
            }

            uint32_t rlen;
            if (net_recv_all(sock, &rlen, 4, 5000) != 4) { net_close(sock); continue; }
            rlen = ntohl(rlen);
            if (rlen > sizeof(buf)) { net_close(sock); continue; }
            if (net_recv_all(sock, buf, rlen, 5000) != (int)rlen) { net_close(sock); continue; }
            buf[rlen] = '\0';

            message_t peer_ident;
            if (message_deserialize(buf, rlen, &peer_ident) != 0) { net_close(sock); continue; }

            if (peer_ident.public_key[0]) {
                peer_set_public_key(targets[i].node_id, peer_ident.public_key);
            }

            peer_lock();
            peer_t* p = peer_find(targets[i].node_id);
            if (p && !p->connected) {
                p->connected = true;
                p->sock = sock;
                p->last_seen_ms = now_ms();

                pthread_t handler_thread;
                pthread_create(&handler_thread, NULL, peer_handler_thread,
                               (void*)(intptr_t)sock);
                pthread_detach(handler_thread);

                char status[128];
                snprintf(status, sizeof(status), "Connected to: %s", p->username);
                tui_set_status(&g_tui, status);
            } else {
                net_close(sock);
            }
            peer_unlock();
        }

        for (int i = 0; i < 50 && g_running; i++) msleep(100);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    if (app_init() != 0) {
        fprintf(stderr, "Failed to initialize application\n");
        return 1;
    }

    g_running = true;

    discovery_start();
    pthread_create(&g_listener_thread, NULL, listener_thread, NULL);
    pthread_create(&g_connector_thread, NULL, connector_thread, NULL);

    tui_set_status(&g_tui, "Meshtalk started");

    while (g_running) {
        char input[MAX_LINE_LEN];
        int n = tui_get_input(&g_tui, input, sizeof(input));
        if (n > 0) {
            process_command(input);
        }

        msg_event_t* e;
        while ((e = pop_event()) != NULL) {
            process_message(&e->msg, e->from_id);
            free(e);
        }

        char* users = build_user_list();
        tui_update_users(&g_tui, users, connected_peer_count() + 1);

        peer_check_timeouts();

        msleep(50);
    }

    tui_set_status(&g_tui, "Shutting down...");
    discovery_stop();
    g_running = false;

    pthread_join(g_listener_thread, NULL);
    pthread_join(g_connector_thread, NULL);

    app_cleanup();
    return 0;
}
