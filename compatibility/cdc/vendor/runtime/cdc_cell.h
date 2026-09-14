#ifndef CDC_CELL_H
#define CDC_CELL_H

#include <stddef.h>
#include <stdint.h>

#include "cdc_frame.h"
#include "cdc_topology.h"

enum { CDC_CELL_ID_MAX = 63 };

typedef enum {
    CDC_CELL_OK = 0,
    CDC_CELL_EARG,
    CDC_CELL_ELIMIT,
    CDC_CELL_EFRAME,
    CDC_CELL_ETOPOLOGY,
    CDC_CELL_ETRANSITION,
    CDC_CELL_ECLOCK
} cdc_cell_status;

typedef struct {
    char cell_id[CDC_CELL_ID_MAX + 1];
    uint64_t generation;
    uint64_t frame_version;
    uint64_t reducer_version;
    uint64_t topology_version;
    uint64_t logical_clock;
    uint64_t causal_horizon;
    uint64_t window_start;
    uint64_t window_end;
    uint64_t freshness;
    double amplitude;
    double mean_phase;
    double dispersion;
    int winding;
    size_t member_count;
    cdc_topology_event_kind transition_kind;
    uint64_t transition_clock;
    uint64_t transition_from_member;
    uint64_t transition_to_member;
    uint8_t frame_snapshot_digest[CDC_DIGEST_SIZE];
    uint8_t oriented_boundary_digest[CDC_DIGEST_SIZE];
    uint8_t sector_digest[CDC_DIGEST_SIZE];
    uint8_t transition_evidence_digest[CDC_DIGEST_SIZE];
    uint8_t hidden_class_digest[CDC_DIGEST_SIZE];
    uint8_t previous_state_digest[CDC_DIGEST_SIZE];
    uint8_t state_digest[CDC_DIGEST_SIZE];
} cdc_cell_state;

/*
 * Reduces one sealed frame to a typed logical-cell state. A same-frame update
 * must pass topology transition verification. Membership/topology replacement
 * requires exactly the next frame version and an explicit FRAME_CHANGE
 * witness; it is never smuggled in as ordinary local deformation.
 */
cdc_cell_status
cdc_cell_reduce(const char *cell_id, const cdc_frame_snapshot *snapshot,
                uint64_t causal_horizon,
                const cdc_cell_state *previous,
                const cdc_topology_witness *witness,
                cdc_cell_state *out);

/*
 * Recomputes every derived digest and validates the canonical state domain.
 * Use this at any persistence, transport, or epoch-import boundary; a nonzero
 * embedded state_digest is not itself evidence that the surrounding fields
 * still match it.
 */
cdc_cell_status cdc_cell_state_validate(const cdc_cell_state *state);

const char *cdc_cell_status_name(cdc_cell_status status);

#endif
