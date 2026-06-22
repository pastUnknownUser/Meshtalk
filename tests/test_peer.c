#include "common.h"
#include "peer/peer.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int passed = 0, failed = 0;
#define TEST(n) do { printf("  TEST: %s ... ", n); } while(0)
#define PASS() do { printf("PASS\n"); passed++; } while(0)
#define FAIL(s) do { printf("FAIL: %s\n", s); failed++; } while(0)

static void test_peer_add_find(void) {
    TEST("Add and find peer");
    peer_init();
    assert(peer_count() == 0);

    peer_add("uuid-1", "Alice", "192.168.1.1", 40001);
    assert(peer_count() == 1);

    peer_t* p = peer_find("uuid-1");
    assert(p != NULL);
    assert(strcmp(p->node_id, "uuid-1") == 0);
    assert(strcmp(p->username, "Alice") == 0);
    assert(strcmp(p->addr, "192.168.1.1") == 0);
    assert(p->port == 40001);
    assert(!p->connected);

    peer_cleanup();
    PASS();
}

static void test_peer_remove(void) {
    TEST("Remove peer");
    peer_init();
    peer_add("uuid-1", "Alice", "192.168.1.1", 40001);
    peer_add("uuid-2", "Bob", "192.168.1.2", 40001);
    assert(peer_count() == 2);

    peer_remove_by_id("uuid-1");
    assert(peer_count() == 1);
    assert(peer_find("uuid-1") == NULL);
    assert(peer_find("uuid-2") != NULL);

    peer_cleanup();
    PASS();
}

static void test_peer_duplicate_add(void) {
    TEST("Duplicate peer");
    peer_init();
    peer_add("uuid-1", "Alice", "192.168.1.1", 40001);
    peer_add("uuid-1", "Alice", "192.168.1.2", 40002);
    assert(peer_count() == 1);

    peer_cleanup();
    PASS();
}

static void test_peer_find_by_index(void) {
    TEST("Find by index");
    peer_init();
    peer_add("uuid-1", "Alice", "192.168.1.1", 40001);
    peer_add("uuid-2", "Bob", "192.168.1.2", 40001);

    peer_t* p = peer_get(0);
    assert(p != NULL);
    assert(strcmp(p->node_id, "uuid-2") == 0); /* most recently added is at head */

    p = peer_get(1);
    assert(p != NULL);
    assert(strcmp(p->node_id, "uuid-1") == 0);

    peer_cleanup();
    PASS();
}

static void test_peer_timeout(void) {
    TEST("Peer timeout detection");
    peer_init();
    peer_add("uuid-1", "Alice", "192.168.1.1", 40001);
    peer_t* p = peer_find("uuid-1");
    assert(p != NULL);
    p->connected = true;
    p->last_seen_ms = 0; /* Set to ancient time */

    peer_check_timeouts();
    assert(!p->connected);

    peer_cleanup();
    PASS();
}

int main(void) {
    printf("Peer Tests:\n");
    test_peer_add_find();
    test_peer_remove();
    test_peer_duplicate_add();
    test_peer_find_by_index();
    test_peer_timeout();

    printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
