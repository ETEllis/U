#ifndef CDC_TOPOLOGY_H
#define CDC_TOPOLOGY_H

#include <stdint.h>

#include "cdc_frame.h"

typedef enum {
    CDC_TOPOLOGY_OK = 0,
    CDC_TOPOLOGY_EARG,
    CDC_TOPOLOGY_EBOUNDARY,
    CDC_TOPOLOGY_ESECTOR,
    CDC_TOPOLOGY_EWITNESS,
    CDC_TOPOLOGY_ECLOCK
} cdc_topology_status;

typedef struct {
    uint64_t frame_version;
    uint64_t topology_version;
    uint64_t logical_clock;
    int winding;
    double maximum_edge_delta;
    uint8_t snapshot_digest[CDC_DIGEST_SIZE];
    uint8_t oriented_boundary_digest[CDC_DIGEST_SIZE];
    uint8_t sector_digest[CDC_DIGEST_SIZE];
} cdc_topology_state;

typedef enum {
    CDC_TOPOLOGY_EVENT_NONE = 0,
    CDC_TOPOLOGY_EVENT_LOCAL_DEFORMATION = 1,
    CDC_TOPOLOGY_EVENT_PHASE_SLIP = 2,
    CDC_TOPOLOGY_EVENT_FRAME_CHANGE = 3
} cdc_topology_event_kind;

typedef struct {
    cdc_topology_event_kind kind;
    uint64_t from_member;
    uint64_t to_member;
    uint64_t logical_clock;
    uint8_t evidence_digest[CDC_DIGEST_SIZE];
} cdc_topology_witness;

/*
 * Computes the same canonical oriented-boundary commitment used by
 * classification, directly from a ring specification. Input array order is
 * irrelevant; the traversal is anchored at the lowest member identifier.
 */
cdc_topology_status
cdc_topology_boundary_digest(
    const uint64_t *member_ids, const uint64_t *successor_ids,
    size_t member_count, uint64_t topology_version,
    uint8_t out[CDC_DIGEST_SIZE]);

cdc_topology_status
cdc_topology_classify(const cdc_frame_snapshot *snapshot,
                      cdc_topology_state *out);

/*
 * A sector change is never inferred from a changed winding alone. It requires
 * a named phase-slip witness on an actual oriented edge, with a clock strictly
 * after the prior state and no later than the next state. V1 accepts one
 * winding-sector step per witnessed transition.
 */
cdc_topology_status
cdc_topology_verify_transition(const cdc_topology_state *previous,
                               const cdc_topology_state *next,
                               const cdc_frame_snapshot *next_snapshot,
                               const cdc_topology_witness *witness);

const char *cdc_topology_status_name(cdc_topology_status status);

#endif
