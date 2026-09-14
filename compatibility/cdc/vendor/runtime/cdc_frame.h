#ifndef CDC_FRAME_H
#define CDC_FRAME_H

#include <stddef.h>
#include <stdint.h>

#include "cdc_digest.h"

enum { CDC_FRAME_ID_MAX = 63 };

typedef enum {
    CDC_FRAME_OK = 0,
    CDC_FRAME_HOLD_STALE,
    CDC_FRAME_EARG,
    CDC_FRAME_ELIMIT,
    CDC_FRAME_EDUPLICATE,
    CDC_FRAME_ETOPOLOGY,
    CDC_FRAME_EWINDOW,
    CDC_FRAME_ECLOCK
} cdc_frame_status;

typedef struct {
    uint64_t member_id;
    uint64_t successor_id;
    double phase;
    uint64_t observed_at;
    uint64_t logical_clock;
    uint8_t source_digest[CDC_DIGEST_SIZE];
} cdc_frame_observation;

typedef struct {
    const char *frame_id;
    uint64_t frame_version;
    uint64_t reducer_version;
    uint64_t topology_version;
    uint64_t window_start;
    uint64_t window_end;
    uint64_t stale_after;
    size_t minimum_members;
    size_t maximum_members;
} cdc_frame_config;

typedef struct {
    char frame_id[CDC_FRAME_ID_MAX + 1];
    uint64_t frame_version;
    uint64_t reducer_version;
    uint64_t topology_version;
    uint64_t window_start;
    uint64_t window_end;
    uint64_t logical_clock;
    uint64_t freshness;
    cdc_frame_observation *observations;
    size_t member_count;
    uint8_t snapshot_digest[CDC_DIGEST_SIZE];
} cdc_frame_snapshot;

void cdc_frame_snapshot_init(cdc_frame_snapshot *snapshot);
void cdc_frame_snapshot_free(cdc_frame_snapshot *snapshot);

/*
 * Seals unordered observations into one immutable oriented-ring snapshot.
 * Successor links, not arrival order, define topology. The lowest member id
 * is the canonical ring anchor, so a cyclic rotation has one representation.
 * `freshness` is the age of the oldest constituent observation at `now`.
 */
cdc_frame_status
cdc_frame_seal(const cdc_frame_config *config,
               const cdc_frame_observation *observations, size_t count,
               uint64_t now, cdc_frame_snapshot *out);

const char *cdc_frame_status_name(cdc_frame_status status);

#endif
