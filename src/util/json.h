#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    char* data;
    size_t len;
    size_t cap;
} json_builder_t;

void json_init(json_builder_t* jb);
void json_free(json_builder_t* jb);
void json_add_string(json_builder_t* jb, const char* key, const char* value);
void json_add_int(json_builder_t* jb, const char* key, int64_t value);
void json_add_uint64(json_builder_t* jb, const char* key, uint64_t value);
const char* json_build(json_builder_t* jb);

int json_parse_string(const char* json, const char* key, char* out, size_t out_size);
int json_parse_int(const char* json, const char* key, int64_t* out);
int json_parse_uint64(const char* json, const char* key, uint64_t* out);

char* json_escape(const char* str);
void json_unescape(char* str);
