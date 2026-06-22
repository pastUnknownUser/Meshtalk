#include "common.h"
#include "discovery/discovery.h"
#include "peer/peer.h"
#include "net/net.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int passed = 0, failed = 0;
#define TEST(n) do { printf("  TEST: %s ... ", n); } while(0)
#define PASS() do { printf("PASS\n"); passed++; } while(0)
#define FAIL(s) do { printf("FAIL: %s\n", s); failed++; } while(0)

static void test_discovery_packet_parse(void) {
    TEST("Parse DISCOVER packet");
    net_init();
    peer_init();

    net_socket_t sock;
    int ret = discovery_init(&sock, "test-id-1", "TestUser", 40001);
    if (ret != 0) {
        FAIL("Could not init discovery");
        peer_cleanup();
        net_cleanup();
        return;
    }

    const char* packet = "DISCOVER\nAlice\n40002\npeer-uuid-1234\n";
    ret = discovery_process_packet(packet, (int)strlen(packet), "192.168.1.100");
    assert(ret == 0);

    peer_t* p = peer_find("peer-uuid-1234");
    assert(p != NULL);
    assert(strcmp(p->username, "Alice") == 0);
    assert(strcmp(p->addr, "192.168.1.100") == 0);
    assert(p->port == 40002);

    discovery_cleanup();
    peer_cleanup();
    net_cleanup();
    PASS();
}

static void test_discovery_ignore_self(void) {
    TEST("Ignore self-discovery");
    net_init();
    peer_init();

    net_socket_t sock;
    discovery_init(&sock, "self-uuid", "Self", 40001);

    const char* packet = "HERE\nSelf\n40001\nself-uuid\n";
    int ret = discovery_process_packet(packet, (int)strlen(packet), "127.0.0.1");
    assert(ret == 0);

    peer_t* p = peer_find("self-uuid");
    assert(p == NULL);

    discovery_cleanup();
    peer_cleanup();
    net_cleanup();
    PASS();
}

int main(void) {
    printf("Discovery Tests:\n");
    test_discovery_packet_parse();
    test_discovery_ignore_self();

    printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
