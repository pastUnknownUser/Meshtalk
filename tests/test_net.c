#include "common.h"
#include "net/net.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)

static void test_net_init_cleanup(void) {
    TEST("net_init/net_cleanup");
    assert(net_init() == 0);
    net_cleanup();
    PASS();
}

static void test_tcp_listen(void) {
    TEST("TCP listen");
    assert(net_init() == 0);
    net_socket_t sock;
    int ret = net_tcp_listen(0, &sock);
    if (ret == 0) {
        assert(net_is_valid_socket(sock));
        net_close(sock);
        PASS();
    } else {
        FAIL("Could not create listener");
    }
    net_cleanup();
}

static void test_tcp_connect(void) {
    TEST("TCP connect");
    assert(net_init() == 0);
    net_socket_t listen_sock;
    if (net_tcp_listen(9999, &listen_sock) != 0) {
        FAIL("Could not create listener");
        net_cleanup();
        return;
    }

    net_conn_t conn;
    int ret = net_tcp_connect("127.0.0.1", 9999, &conn, 3000);
    if (ret == 0) {
        net_close(conn.sock);
        net_close(listen_sock);
        PASS();
    } else {
        net_close(listen_sock);
        FAIL("Could not connect");
    }
    net_cleanup();
}

static void test_udp_send_recv(void) {
    TEST("UDP send/recv");
    assert(net_init() == 0);
    net_socket_t s1, s2;
    assert(net_udp_listen_socket(19999, &s1) == 0);
    assert(net_udp_listen_socket(19998, &s2) == 0);

    const char* msg = "hello";
    int n = net_udp_send(s1, msg, strlen(msg), "127.0.0.1", 19998);
    assert(n > 0);

    char buf[64];
    char from[64];
    uint16_t port;
    n = net_udp_recv(s2, buf, sizeof(buf), from, &port, 1000);
    if (n > 0) {
        buf[n] = '\0';
        assert(strcmp(buf, "hello") == 0);
        net_close(s1);
        net_close(s2);
        PASS();
    } else {
        net_close(s1);
        net_close(s2);
        FAIL("Did not receive UDP message");
    }
    net_cleanup();
}

int main(void) {
    printf("Network Tests:\n");
    test_net_init_cleanup();
    test_tcp_listen();
    test_tcp_connect();
    test_udp_send_recv();

    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
