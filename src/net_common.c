#include "net.h"

/* Platform-independent init/cleanup */

int net_init(void) {
    /* On Unix, nothing special needed */
    return NET_OK;
}

void net_cleanup(void) {
    /* On Unix, nothing special needed */
}
