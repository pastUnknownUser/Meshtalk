#include "common.h"
#include "message/message.h"
#include "util/json.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int passed = 0, failed = 0;
#define TEST(n) do { printf("  TEST: %s ... ", n); } while(0)
#define PASS() do { printf("PASS\n"); passed++; } while(0)
#define FAIL(s) do { printf("FAIL: %s\n", s); failed++; } while(0)

static void test_serialize_deserialize(void) {
    TEST("Serialize/Deserialize");
    message_t msg;
    memset(&msg, 0, sizeof(msg));
    strcpy(msg.msg_id, "f47ac10b-58cc-4372-a567-0e02b2c3d479");
    strcpy(msg.sender_id, "550e8400-e29b-41d4-a716-446655440000");
    strcpy(msg.sender_name, "Alice");
    msg.timestamp_ms = 1234567890;
    msg.type = MSG_CHAT;
    strcpy(msg.room, "general");
    strcpy(msg.payload, "Hello, world!");

    char buf[4096];
    int slen = message_serialize(&msg, buf, sizeof(buf));
    assert(slen > 0);

    message_t msg2;
    memset(&msg2, 0, sizeof(msg2));
    int ret = message_deserialize(buf, (size_t)slen, &msg2);
    assert(ret == 0);

    assert(strcmp(msg.msg_id, msg2.msg_id) == 0);
    assert(strcmp(msg.sender_id, msg2.sender_id) == 0);
    assert(strcmp(msg.sender_name, msg2.sender_name) == 0);
    assert(msg.timestamp_ms == msg2.timestamp_ms);
    assert(msg.type == msg2.type);
    assert(strcmp(msg.room, msg2.room) == 0);
    assert(strcmp(msg.payload, msg2.payload) == 0);

    PASS();
}

static void test_duplicate_suppression(void) {
    TEST("Duplicate suppression");
    const char* id1 = "aaaa-1111";
    const char* id2 = "bbbb-2222";

    assert(!message_is_duplicate(id1));
    message_cache_add(id1);
    assert(message_is_duplicate(id1));

    assert(!message_is_duplicate(id2));
    message_cache_add(id2);
    assert(message_is_duplicate(id2));

    PASS();
}

static void test_message_cache_cleanup(void) {
    TEST("Cache cleanup");
    message_cache_add("test-cache-1");
    message_cache_add("test-cache-2");
    assert(message_is_duplicate("test-cache-1"));
    message_cache_cleanup();
    PASS();
}

static void test_message_list(void) {
    TEST("Message list");
    message_list_t list = {NULL, NULL, 0};

    message_t msgs[5];
    for (int i = 0; i < 5; i++) {
        memset(&msgs[i], 0, sizeof(message_t));
        snprintf(msgs[i].msg_id, sizeof(msgs[i].msg_id), "msg-%d", i);
        snprintf(msgs[i].payload, sizeof(msgs[i].payload), "Payload %d", i);
        message_list_add(&list, &msgs[i]);
    }

    assert(list.count == 5);

    message_t out[10];
    int n = message_list_get(&list, 3, out);
    assert(n == 3);
    assert(strcmp(out[0].payload, "Payload 2") == 0);
    assert(strcmp(out[1].payload, "Payload 3") == 0);
    assert(strcmp(out[2].payload, "Payload 4") == 0);

    message_list_free(&list);
    PASS();
}

int main(void) {
    printf("Message Tests:\n");
    test_serialize_deserialize();
    test_duplicate_suppression();
    test_message_cache_cleanup();
    test_message_list();

    printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
