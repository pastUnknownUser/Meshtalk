#include "common.h"
#include "net/net.h"
#include "util/uuid.h"
#include "util/json.h"
#include "message/message.h"
#include "peer/peer.h"
#include "discovery/discovery.h"
#include "room/room.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>

#define SIM_NODES 100
#define SIM_DURATION_MS 30000

typedef struct {
    char node_id[UUID_STR_LEN + 1];
    char username[32];
    net_socket_t listen_sock;
    net_socket_t udp_sock;
    pthread_t thread;
    volatile bool running;
    int msg_count;
    int peer_count;
} sim_node_t;

static sim_node_t g_nodes[SIM_NODES];
static volatile bool g_running = false;
static int g_total_messages = 0;
static pthread_mutex_t g_stats_lock = PTHREAD_MUTEX_INITIALIZER;

static void* sim_node_thread(void* arg) {
    int idx = (int)(intptr_t)arg;
    sim_node_t* node = &g_nodes[idx];

    net_init();
    uuid_generate(node->node_id);
    snprintf(node->username, sizeof(node->username), "SimUser%d", idx);

    peer_init();
    room_init();

    /* Listen */
    int port = 50000 + idx;
    net_tcp_listen((uint16_t)port, &node->listen_sock);

    /* Discovery */
    discovery_init(&node->udp_sock, node->node_id, node->username, port);

    /* Simulate: generate random messages */
    int msg_count = 0;
    uint64_t start = now_ms();
    node->running = true;

    while (g_running && (now_ms() - start) < SIM_DURATION_MS) {
        /* Send a random message */
        if (rand() % 10 == 0) {
            message_t msg;
            memset(&msg, 0, sizeof(msg));
            message_generate_id(msg.msg_id);
            strncpy(msg.sender_id, node->node_id, UUID_STR_LEN);
            strncpy(msg.sender_name, node->username, MAX_USERNAME_LEN);
            msg.timestamp_ms = now_ms();
            msg.type = MSG_CHAT;
            strncpy(msg.room, "general", MAX_ROOM_LEN);
            snprintf(msg.payload, sizeof(msg.payload),
                     "Message %d from node %d", msg_count++, idx);

            message_cache_add(msg.msg_id);

            /* Simulate flooding to known peers */
            peer_lock();
            for (int i = 0; i < peer_count(); i++) {
                peer_t* p = peer_get(i);
                if (p && p->connected) {
                    char serialized[MAX_PACKET_SIZE];
                    int slen = message_serialize(&msg, serialized, sizeof(serialized));
                    if (slen > 0) {
                        uint32_t nlen = htonl((uint32_t)slen + 4);
                        net_send_all(p->sock, (const char*)&nlen, 4);
                        net_send_all(p->sock, serialized, (size_t)slen);
                    }
                }
            }
            peer_unlock();

            pthread_mutex_lock(&g_stats_lock);
            g_total_messages++;
            pthread_mutex_unlock(&g_stats_lock);
        }

        node->peer_count = peer_count();
        msleep(50);
    }

    node->msg_count = msg_count;
    node->running = false;

    peer_disconnect_all();
    peer_cleanup();
    room_cleanup();
    discovery_cleanup();
    net_close(node->listen_sock);
    net_cleanup();

    return NULL;
}

int main(void) {
    printf("=== Meshtalk Network Simulator ===\n");
    printf("Launching %d virtual nodes...\n", SIM_NODES);

    srand((unsigned)time(NULL));
    g_running = true;

    for (int i = 0; i < SIM_NODES; i++) {
        memset(&g_nodes[i], 0, sizeof(sim_node_t));
        pthread_create(&g_nodes[i].thread, NULL, sim_node_thread,
                       (void*)(intptr_t)i);
    }

    printf("Running for %d seconds...\n", SIM_DURATION_MS / 1000);

    int last_msg = 0;
    for (int i = 0; i < SIM_DURATION_MS / 1000; i++) {
        msleep(1000);
        pthread_mutex_lock(&g_stats_lock);
        int msgs = g_total_messages - last_msg;
        last_msg = g_total_messages;
        printf("  [%ds] Active messages: %d (total: %d)\n",
               i + 1, msgs, g_total_messages);
        pthread_mutex_unlock(&g_stats_lock);
    }

    g_running = false;

    printf("\nWaiting for nodes to finish...\n");
    int total_peers = 0;
    int total_msgs = 0;
    for (int i = 0; i < SIM_NODES; i++) {
        pthread_join(g_nodes[i].thread, NULL);
        total_peers += g_nodes[i].peer_count;
        total_msgs += g_nodes[i].msg_count;
    }

    printf("\n=== Results ===\n");
    printf("Total nodes:        %d\n", SIM_NODES);
    printf("Total messages sent: %d\n", g_total_messages);
    printf("Avg peers per node:  %.1f\n", (float)total_peers / SIM_NODES);
    printf("Avg msgs per node:   %.1f\n", (float)total_msgs / SIM_NODES);
    printf("Simulation: SUCCESS\n");

    return 0;
}
