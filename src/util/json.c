#include "json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void json_init(json_builder_t* jb) {
    jb->data = (char*)malloc(256);
    jb->data[0] = '{';
    jb->len = 1;
    jb->cap = 256;
}

void json_free(json_builder_t* jb) {
    free(jb->data);
    jb->data = NULL;
    jb->len = 0;
    jb->cap = 0;
}

static void json_grow(json_builder_t* jb, size_t needed) {
    if (jb->len + needed + 2 > jb->cap) {
        jb->cap = (jb->len + needed + 2) * 2;
        jb->data = (char*)realloc(jb->data, jb->cap);
    }
}

static void json_add_separator(json_builder_t* jb) {
    if (jb->len > 1) {
        json_grow(jb, 1);
        jb->data[jb->len++] = ',';
    }
}

static void json_add_key(json_builder_t* jb, const char* key) {
    json_add_separator(jb);
    size_t klen = strlen(key);
    json_grow(jb, klen + 4);
    jb->data[jb->len++] = '"';
    memcpy(jb->data + jb->len, key, klen);
    jb->len += klen;
    jb->data[jb->len++] = '"';
    jb->data[jb->len++] = ':';
}

void json_add_string(json_builder_t* jb, const char* key, const char* value) {
    json_add_key(jb, key);
    char* escaped = json_escape(value ? value : "");
    size_t vlen = strlen(escaped);
    json_grow(jb, vlen + 3);
    jb->data[jb->len++] = '"';
    memcpy(jb->data + jb->len, escaped, vlen);
    jb->len += vlen;
    jb->data[jb->len++] = '"';
    free(escaped);
}

void json_add_int(json_builder_t* jb, const char* key, int64_t value) {
    json_add_key(jb, key);
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%lld", (long long)value);
    json_grow(jb, n);
    memcpy(jb->data + jb->len, buf, n);
    jb->len += n;
}

void json_add_uint64(json_builder_t* jb, const char* key, uint64_t value) {
    json_add_key(jb, key);
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%llu", (unsigned long long)value);
    json_grow(jb, n);
    memcpy(jb->data + jb->len, buf, n);
    jb->len += n;
}

const char* json_build(json_builder_t* jb) {
    json_grow(jb, 2);
    jb->data[jb->len++] = '}';
    jb->data[jb->len] = '\0';
    return jb->data;
}

static const char* json_find_key(const char* json, const char* key) {
    if (!json || !key) return NULL;

    size_t klen = strlen(key);
    const char* p = json;

    while (*p) {
        while (*p && *p != '"') p++;
        if (!*p) return NULL;
        p++;

        if (strncmp(p, key, klen) == 0 && p[klen] == '"') {
            p += klen + 1;
            while (*p && (*p == ' ' || *p == ':' || *p == '\t')) p++;
            return p;
        }

        while (*p && *p != '"') p++;
        if (*p) p++;
    }
    return NULL;
}

int json_parse_string(const char* json_str, const char* key, char* out, size_t out_size) {
    const char* p = json_find_key(json_str, key);
    if (!p || *p != '"') return -1;
    p++;

    size_t i = 0;
    while (*p && *p != '"' && i < out_size - 1) {
        if (*p == '\\' && *(p + 1)) {
            p++;
            switch (*p) {
                case 'n': out[i++] = '\n'; break;
                case 't': out[i++] = '\t'; break;
                case 'r': out[i++] = '\r'; break;
                case '\\': out[i++] = '\\'; break;
                case '"': out[i++] = '"'; break;
                default: out[i++] = *p; break;
            }
        } else {
            out[i++] = *p;
        }
        p++;
    }
    out[i] = '\0';
    return 0;
}

int json_parse_int(const char* json_str, const char* key, int64_t* out) {
    const char* p = json_find_key(json_str, key);
    if (!p || (*p != '-' && (*p < '0' || *p > '9'))) return -1;
    *out = strtoll(p, NULL, 10);
    return 0;
}

int json_parse_uint64(const char* json_str, const char* key, uint64_t* out) {
    const char* p = json_find_key(json_str, key);
    if (!p || *p < '0' || *p > '9') return -1;
    *out = strtoull(p, NULL, 10);
    return 0;
}

char* json_escape(const char* str) {
    size_t len = strlen(str);
    size_t cap = len * 2 + 2;
    char* out = (char*)malloc(cap);
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        switch (str[i]) {
            case '"':  out[j++] = '\\'; out[j++] = '"'; break;
            case '\\': out[j++] = '\\'; out[j++] = '\\'; break;
            case '\n': out[j++] = '\\'; out[j++] = 'n'; break;
            case '\r': out[j++] = '\\'; out[j++] = 'r'; break;
            case '\t': out[j++] = '\\'; out[j++] = 't'; break;
            default:   out[j++] = str[i]; break;
        }
    }
    out[j] = '\0';
    return out;
}

void json_unescape(char* str) {
    size_t i = 0, j = 0;
    while (str[i]) {
        if (str[i] == '\\' && str[i + 1]) {
            i++;
            switch (str[i]) {
                case 'n': str[j++] = '\n'; break;
                case 't': str[j++] = '\t'; break;
                case 'r': str[j++] = '\r'; break;
                default: str[j++] = str[i]; break;
            }
        } else {
            str[j++] = str[i];
        }
        i++;
    }
    str[j] = '\0';
}
