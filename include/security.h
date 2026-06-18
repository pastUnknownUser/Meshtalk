#ifndef MESHTALK_SECURITY_H
#define MESHTALK_SECURITY_H

#include "message.h"
#include <stdbool.h>
#include <stddef.h>

/* Validation results */
typedef enum {
    MT_VALID_OK = 0,
    MT_VALID_INVALID_TYPE,
    MT_VALID_INVALID_LENGTH,
    MT_VALID_INVALID_USERNAME,
    MT_VALID_INVALID_ROOM,
    MT_VALID_INVALID_PAYLOAD,
    MT_VALID_INVALID_NODE_ID,
    MT_VALID_OVERFLOW
} mt_valid_result_t;

/* Validate a frame's structure (length, type) */
mt_valid_result_t mt_validate_frame(const mt_frame_t *frame);

/* Validate a username (alphanumeric, underscores, hyphens; 1-63 chars) */
mt_valid_result_t mt_validate_username(const char *username);

/* Validate a room name (alphanumeric, underscores, hyphens, dots; 1-127 chars) */
mt_valid_result_t mt_validate_room(const char *room);

/* Validate a node_id (UUID format) */
mt_valid_result_t mt_validate_node_id(const char *node_id);

/* Validate message payload size */
mt_valid_result_t mt_validate_payload(const char *payload, size_t len);

/* Sanitize a string: strip control characters, enforce max length */
void mt_sanitize_string(char *str, size_t max_len);

/* Get human-readable error string */
const char *mt_valid_error(mt_valid_result_t result);

#endif /* MESHTALK_SECURITY_H */
