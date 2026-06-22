#pragma once

#include "../common.h"
#include "../net/net.h"

int  discovery_init(net_socket_t* udp_sock, const char* node_id, const char* username, int tcp_port);
void discovery_cleanup(void);
void discovery_start(void);
void discovery_stop(void);
int  discovery_send_broadcast(void);
int  discovery_process_packet(const char* data, int len, const char* from_addr);
