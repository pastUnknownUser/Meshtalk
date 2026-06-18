#ifndef MESHTALK_DEDUP_H
#define MESHTALK_DEDUP_H

#include <stdbool.h>
#include <stdint.h>

#define MT_DEDUP_MAX_ENTRIES 10000
#define MT_DEDUP_TTL_SECONDS 300  /* 5 minutes */
#define MT_DEDUP_CLEANUP_INTERVAL 60

/* Duplicate suppression cache */
typedef struct mt_dedup mt_dedup_t;

/* Create dedup cache */
mt_dedup_t *mt_dedup_create(void);

/* Destroy dedup cache */
void mt_dedup_destroy(mt_dedup_t *dedup);

/* Check if message_id has been seen. Returns true if duplicate (already seen). */
bool mt_dedup_is_duplicate(mt_dedup_t *dedup, const char *message_id);

/* Record a message_id as seen */
void mt_dedup_record(mt_dedup_t *dedup, const char *message_id);

/* Clean up expired entries */
void mt_dedup_cleanup(mt_dedup_t *dedup);

/* Reset cache */
void mt_dedup_reset(mt_dedup_t *dedup);

/* Current entry count */
int mt_dedup_count(mt_dedup_t *dedup);

#endif /* MESHTALK_DEDUP_H */
