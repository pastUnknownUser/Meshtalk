#ifndef MESHTALK_DISCOVERY_H
#define MESHTALK_DISCOVERY_H

#include "net.h"
#include "message.h"

#define MT_DISCOVERY_INTERVAL 5  /* seconds between broadcasts */

/* Discovery context (internal state) */
typedef struct mt_discovery mt_discovery_t;

/* Callback: called when a new peer is discovered */
typedef void (*mt_discovery_callback)(const char *node_id, const char *username,
                                      const char *ip_addr, uint16_t tcp_port, void *user_data);

/* Initialize discovery system */
mt_discovery_t *mt_discovery_create(const char *node_id, const char *username,
                                     uint16_t tcp_port, mt_discovery_callback cb, void *user_data);

/* Start discovery (runs in background) */
int mt_discovery_start(mt_discovery_t *disc);

/* Stop discovery */
void mt_discovery_stop(mt_discovery_t *disc);

/* Destroy discovery context */
void mt_discovery_destroy(mt_discovery_t *disc);

#endif /* MESHTALK_DISCOVERY_H */
