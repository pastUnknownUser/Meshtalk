#include "storage.h"
#include "../util/json.h"
#include "../peer/peer.h"
#include "../room/room.h"
#include "../crypto/crypto.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

static char g_config_dir[512] = {0};

static void get_config_path(char* buf, size_t size) {
    if (g_config_dir[0]) {
        strncpy(buf, g_config_dir, size - 1);
        buf[size - 1] = '\0';
        return;
    }
#ifdef _WIN32
    const char* appdata = getenv("APPDATA");
    if (appdata) snprintf(buf, size, "%s\\meshtalk", appdata);
    else snprintf(buf, size, ".meshtalk");
#else
    const char* home = getenv("HOME");
    if (home) snprintf(buf, size, "%s/.meshtalk", home);
    else snprintf(buf, size, ".meshtalk");
#endif
    strncpy(g_config_dir, buf, sizeof(g_config_dir) - 1);
}

static void ensure_dir(const char* path) {
#ifdef _WIN32
    mkdir(path);
#else
    mkdir(path, 0700);
#endif
}

static int read_file(const char* path, char* buf, size_t buf_size) {
    FILE* f = fopen(path, "r");
    if (!f) return -1;
    size_t n = fread(buf, 1, buf_size - 1, f);
    fclose(f);
    buf[n] = '\0';
    return (int)n;
}

static int write_file(const char* path, const char* data) {
    FILE* f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "%s\n", data);
    fclose(f);
    return 0;
}

int storage_save_config(const char* node_id, const char* username) {
    char path[512];
    get_config_path(path, sizeof(path));
    ensure_dir(path);
    strncat(path, "/", sizeof(path) - strlen(path) - 1);
    strncat(path, CONFIG_FILE, sizeof(path) - strlen(path) - 1);

    json_builder_t jb;
    json_init(&jb);
    json_add_string(&jb, "node_id", node_id);
    json_add_string(&jb, "username", username);
    const char* json = json_build(&jb);
    int ret = write_file(path, json);
    json_free(&jb);
    return ret;
}

int storage_load_config(char* node_id, size_t id_size, char* username, size_t uname_size) {
    char path[512];
    get_config_path(path, sizeof(path));
    ensure_dir(path);
    strncat(path, "/", sizeof(path) - strlen(path) - 1);
    strncat(path, CONFIG_FILE, sizeof(path) - strlen(path) - 1);

    char buf[4096];
    if (read_file(path, buf, sizeof(buf)) <= 0) return -1;

    if (json_parse_string(buf, "node_id", node_id, (int)id_size) != 0) return -1;
    json_parse_string(buf, "username", username, (int)uname_size);
    return 0;
}

int storage_save_peers(void) {
    char path[512];
    get_config_path(path, sizeof(path));
    strncat(path, "/peers.txt", sizeof(path) - strlen(path) - 1);

    FILE* f = fopen(path, "w");
    if (!f) return -1;

    peer_lock();
    for (int i = 0; ; i++) {
        peer_t* p = peer_get(i);
        if (!p) break;
        fprintf(f, "%s %s %d\n", p->node_id, p->addr, p->port);
    }
    peer_unlock();
    fclose(f);
    return 0;
}

int storage_load_peers(void) {
    char path[512];
    get_config_path(path, sizeof(path));
    strncat(path, "/peers.txt", sizeof(path) - strlen(path) - 1);

    char buf[65536];
    int n = read_file(path, buf, sizeof(buf));
    if (n <= 0) return -1;

    char* line = buf;
    while (line && *line) {
        while (*line == ' ' || *line == '\t' || *line == '\n' || *line == '\r') line++;
        if (!*line) break;

        char* space = strchr(line, ' ');
        if (!space) break;
        *space = '\0';
        char* node_id = line;
        line = space + 1;

        space = strchr(line, ' ');
        if (!space) break;
        *space = '\0';
        char* addr = line;
        line = space + 1;

        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char* cr = strchr(line, '\r');
        if (cr) *cr = '\0';

        int port = atoi(line);
        if (node_id[0] && addr[0] && port > 0) {
            peer_add(node_id, NULL, addr, port);
        }

        line = nl ? nl + 1 : (cr ? cr + 1 : NULL);
    }
    return 0;
}

int storage_save_history(const char* room_name, const message_list_t* history) {
    (void)room_name;
    (void)history;
    return 0;
}

int storage_load_history(const char* room_name, message_list_t* history) {
    (void)room_name;
    (void)history;
    return 0;
}

int storage_save_identity(const unsigned char* sk, size_t sk_size,
                           const unsigned char* pk, size_t pk_size) {
    char dir[512];
    get_config_path(dir, sizeof(dir));
    ensure_dir(dir);

    char path[640];
    snprintf(path, sizeof(path), "%s/identity.key", dir);
    char b64[256];
    crypto_b64_encode(sk, sk_size, b64, sizeof(b64));
    if (write_file(path, b64) != 0) return -1;

    snprintf(path, sizeof(path), "%s/identity.pub", dir);
    crypto_b64_encode(pk, pk_size, b64, sizeof(b64));
    return write_file(path, b64);
}

int storage_load_identity(unsigned char* sk, size_t sk_size,
                           unsigned char* pk, size_t pk_size) {
    char dir[512];
    get_config_path(dir, sizeof(dir));
    ensure_dir(dir);

    char path[640];
    snprintf(path, sizeof(path), "%s/identity.key", dir);
    char b64[256];
    if (read_file(path, b64, sizeof(b64)) <= 0) return -1;
    char* nl = strchr(b64, '\n');
    if (nl) *nl = '\0';
    if (crypto_b64_decode(b64, sk, sk_size) != 0) return -1;

    snprintf(path, sizeof(path), "%s/identity.pub", dir);
    if (read_file(path, b64, sizeof(b64)) <= 0) return -1;
    nl = strchr(b64, '\n');
    if (nl) *nl = '\0';
    return crypto_b64_decode(b64, pk, pk_size) == 0 ? 0 : -1;
}

static char trusted_file[640] = {0};

#define MAX_TRUSTED_KEYS 256
static char g_trusted_ids[MAX_TRUSTED_KEYS][UUID_STR_LEN + 1];
static char g_trusted_keys[MAX_TRUSTED_KEYS][BASE64_PK_LEN];
static int g_trusted_count = 0;

static int get_trusted_path(void) {
    if (trusted_file[0]) return 0;
    char dir[512];
    get_config_path(dir, sizeof(dir));
    ensure_dir(dir);
    snprintf(trusted_file, sizeof(trusted_file), "%s/trusted_keys.txt", dir);
    return 0;
}

int storage_add_trusted_key(const char* node_id, const char* public_key_b64) {
    if (get_trusted_path() != 0) return -1;

    for (int i = 0; i < g_trusted_count; i++) {
        if (strcmp(g_trusted_ids[i], node_id) == 0) {
            strncpy(g_trusted_keys[i], public_key_b64, BASE64_PK_LEN - 1);
            g_trusted_keys[i][BASE64_PK_LEN - 1] = '\0';
            return storage_save_trusted_keys();
        }
    }

    if (g_trusted_count >= MAX_TRUSTED_KEYS) return -1;
    strncpy(g_trusted_ids[g_trusted_count], node_id, UUID_STR_LEN);
    g_trusted_ids[g_trusted_count][UUID_STR_LEN] = '\0';
    strncpy(g_trusted_keys[g_trusted_count], public_key_b64, BASE64_PK_LEN - 1);
    g_trusted_keys[g_trusted_count][BASE64_PK_LEN - 1] = '\0';
    g_trusted_count++;
    return storage_save_trusted_keys();
}

int storage_is_key_trusted(const char* node_id) {
    for (int i = 0; i < g_trusted_count; i++) {
        if (strcmp(g_trusted_ids[i], node_id) == 0) return 1;
    }
    return 0;
}

int storage_save_trusted_keys(void) {
    if (get_trusted_path() != 0) return -1;

    FILE* f = fopen(trusted_file, "w");
    if (!f) return -1;
    for (int i = 0; i < g_trusted_count; i++) {
        fprintf(f, "%s %s\n", g_trusted_ids[i], g_trusted_keys[i]);
    }
    fclose(f);
    return 0;
}

int storage_load_trusted_keys(void) {
    if (get_trusted_path() != 0) return -1;

    char buf[65536];
    int n = read_file(trusted_file, buf, sizeof(buf));
    if (n <= 0) return -1;

    g_trusted_count = 0;
    char* line = buf;
    while (line && *line && g_trusted_count < MAX_TRUSTED_KEYS) {
        while (*line == ' ' || *line == '\t' || *line == '\n' || *line == '\r') line++;
        if (!*line) break;

        char* space = strchr(line, ' ');
        if (!space) break;
        *space = '\0';

        char* key = space + 1;
        char* nl = strchr(key, '\n');
        if (nl) *nl = '\0';
        char* cr = strchr(key, '\r');
        if (cr) *cr = '\0';

        if (line[0] && key[0]) {
            strncpy(g_trusted_ids[g_trusted_count], line, UUID_STR_LEN);
            g_trusted_ids[g_trusted_count][UUID_STR_LEN] = '\0';
            strncpy(g_trusted_keys[g_trusted_count], key, BASE64_PK_LEN - 1);
            g_trusted_keys[g_trusted_count][BASE64_PK_LEN - 1] = '\0';
            g_trusted_count++;
        }

        line = nl ? nl + 1 : NULL;
    }
    return 0;
}

int storage_save_rooms(void) {
    char dir[512];
    get_config_path(dir, sizeof(dir));
    ensure_dir(dir);
    char path[640];
    snprintf(path, sizeof(path), "%s/rooms.json", dir);

    FILE* f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "[\n");

    room_lock();
    int first = 1;
    for (int i = 0; i < room_count(); i++) {
        room_t* r = room_get(i);
        if (!r) continue;

        if (r->is_encrypted) {
            unsigned char sealed_buf[256];
            size_t sealed_len = 0;
            char room_key_b64[256];
            if (crypto_seal_for_storage(r->room_key, ROOM_KEY_LEN,
                                         sealed_buf, &sealed_len,
                                         room_key_b64, sizeof(room_key_b64)) == 0) {
                if (!first) fprintf(f, ",\n");
                first = 0;
                fprintf(f, "  {\"name\":\"%s\",\"encrypted\":true,\"room_key\":\"%s\"}",
                        r->name, room_key_b64);
            }
        } else {
            if (!first) fprintf(f, ",\n");
            first = 0;
            fprintf(f, "  {\"name\":\"%s\",\"encrypted\":false}", r->name);
        }
    }
    room_unlock();

    fprintf(f, "\n]\n");
    fclose(f);
    return 0;
}

int storage_load_rooms(void) {
    char dir[512];
    get_config_path(dir, sizeof(dir));
    char path[640];
    snprintf(path, sizeof(path), "%s/rooms.json", dir);

    char buf[65536];
    int n = read_file(path, buf, sizeof(buf));
    if (n <= 0) return -1;

    room_init();

    const char* p = buf;
    while (*p && *p != '[') p++;
    if (!*p) return 0;

    p++;
    while (*p) {
        while (*p && *p != '{') { if (*p == ']') return 0; p++; }
        if (!*p || *p != '{') break;

        char name[128] = {0};
        int encrypted = 0;
        char room_key_b64[256] = {0};

        json_parse_string(p, "name", name, sizeof(name));
        int64_t enc_val = 0;
        json_parse_int(p, "encrypted", &enc_val);
        encrypted = (int)enc_val;
        json_parse_string(p, "room_key", room_key_b64, sizeof(room_key_b64));

        if (encrypted && name[0]) {
            room_create_encrypted(name);

            if (room_key_b64[0]) {
                unsigned char rk[ROOM_KEY_LEN];
                unsigned char sealed_buf[256];
                if (crypto_unseal_from_storage(room_key_b64, rk, sizeof(rk),
                                                sealed_buf, sizeof(sealed_buf)) > 0) {
                    room_set_key(name, rk);
                }
            }
        } else if (name[0]) {
            room_create(name);
        }

        while (*p && *p != '}') p++;
        if (*p) p++;
    }

    return 0;
}
