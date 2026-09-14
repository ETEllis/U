#include "cdc_topology.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const uint8_t BOUNDARY_DOMAIN[] = "CDC-RFTC-BOUNDARY-V1";
static const uint8_t SECTOR_DOMAIN[] = "CDC-RFTC-SECTOR-V1";
static const double WINDING_TOLERANCE = 1e-9;

static double wrap_pi(double value) {
    double wrapped = fmod(value, 2.0 * M_PI);
    if (wrapped <= -M_PI) {
        wrapped += 2.0 * M_PI;
    } else if (wrapped > M_PI) {
        wrapped -= 2.0 * M_PI;
    }
    return wrapped;
}

static int digest_is_zero(const uint8_t digest[CDC_DIGEST_SIZE]) {
    uint8_t value = 0;
    size_t i;
    for (i = 0; i < CDC_DIGEST_SIZE; i++) {
        value |= digest[i];
    }
    return value == 0;
}

static void digest_u64(cdc_digest_ctx *ctx, uint64_t value) {
    uint8_t bytes[8];
    size_t i;
    for (i = 0; i < sizeof(bytes); i++) {
        bytes[sizeof(bytes) - 1 - i] = (uint8_t)(value >> (8 * i));
    }
    cdc_digest_update(ctx, bytes, sizeof(bytes));
}

static int edge_exists(const cdc_frame_snapshot *snapshot, uint64_t from,
                       uint64_t to) {
    size_t i;
    for (i = 0; i < snapshot->member_count; i++) {
        if (snapshot->observations[i].member_id == from &&
            snapshot->observations[i].successor_id == to) {
            return 1;
        }
    }
    return 0;
}

cdc_topology_status
cdc_topology_boundary_digest(
    const uint64_t *member_ids, const uint64_t *successor_ids,
    size_t member_count, uint64_t topology_version,
    uint8_t out[CDC_DIGEST_SIZE]) {
    cdc_digest_ctx boundary;
    size_t anchor = 0;
    size_t current;
    size_t i;
    if (!member_ids || !successor_ids || !out || member_count < 2 ||
        topology_version == 0) {
        return CDC_TOPOLOGY_EARG;
    }
    for (i = 0; i < member_count; i++) {
        size_t j;
        size_t matches = 0;
        if (member_ids[i] == 0 || successor_ids[i] == 0) {
            return CDC_TOPOLOGY_EBOUNDARY;
        }
        if (member_ids[i] < member_ids[anchor]) {
            anchor = i;
        }
        for (j = i + 1; j < member_count; j++) {
            if (member_ids[i] == member_ids[j] ||
                successor_ids[i] == successor_ids[j]) {
                return CDC_TOPOLOGY_EBOUNDARY;
            }
        }
        for (j = 0; j < member_count; j++) {
            if (successor_ids[i] == member_ids[j]) {
                matches++;
            }
        }
        if (matches != 1) {
            return CDC_TOPOLOGY_EBOUNDARY;
        }
    }
    cdc_digest_init(&boundary);
    cdc_digest_update(&boundary, BOUNDARY_DOMAIN,
                      sizeof(BOUNDARY_DOMAIN));
    digest_u64(&boundary, topology_version);
    digest_u64(&boundary, (uint64_t)member_count);
    current = anchor;
    for (i = 0; i < member_count; i++) {
        size_t next = SIZE_MAX;
        size_t j;
        if (i > 0 && current == anchor) {
            return CDC_TOPOLOGY_EBOUNDARY;
        }
        digest_u64(&boundary, member_ids[current]);
        digest_u64(&boundary, successor_ids[current]);
        for (j = 0; j < member_count; j++) {
            if (member_ids[j] == successor_ids[current]) {
                next = j;
                break;
            }
        }
        if (next == SIZE_MAX) {
            return CDC_TOPOLOGY_EBOUNDARY;
        }
        current = next;
    }
    if (current != anchor) {
        return CDC_TOPOLOGY_EBOUNDARY;
    }
    cdc_digest_final(&boundary, out);
    return CDC_TOPOLOGY_OK;
}

cdc_topology_status
cdc_topology_classify(const cdc_frame_snapshot *snapshot,
                      cdc_topology_state *out) {
    cdc_digest_ctx boundary;
    cdc_digest_ctx sector;
    double winding_total = 0.0;
    double maximum_delta = 0.0;
    size_t i;
    if (!snapshot || !out || !snapshot->observations ||
        snapshot->member_count < 2 ||
        digest_is_zero(snapshot->snapshot_digest)) {
        return CDC_TOPOLOGY_EARG;
    }

    cdc_digest_init(&boundary);
    cdc_digest_update(&boundary, BOUNDARY_DOMAIN, sizeof(BOUNDARY_DOMAIN));
    digest_u64(&boundary, snapshot->topology_version);
    digest_u64(&boundary, (uint64_t)snapshot->member_count);
    for (i = 0; i < snapshot->member_count; i++) {
        const cdc_frame_observation *current = &snapshot->observations[i];
        const cdc_frame_observation *next =
            &snapshot->observations[(i + 1) % snapshot->member_count];
        double delta;
        if (current->successor_id != next->member_id) {
            return CDC_TOPOLOGY_EBOUNDARY;
        }
        delta = wrap_pi(next->phase - current->phase);
        if (fabs(fabs(delta) - M_PI) <= WINDING_TOLERANCE) {
            return CDC_TOPOLOGY_ESECTOR;
        }
        winding_total += delta;
        if (fabs(delta) > maximum_delta) {
            maximum_delta = fabs(delta);
        }
        digest_u64(&boundary, current->member_id);
        digest_u64(&boundary, current->successor_id);
    }

    if (fabs(winding_total / (2.0 * M_PI) -
             round(winding_total / (2.0 * M_PI))) >
        WINDING_TOLERANCE) {
        return CDC_TOPOLOGY_ESECTOR;
    }
    memset(out, 0, sizeof(*out));
    out->frame_version = snapshot->frame_version;
    out->topology_version = snapshot->topology_version;
    out->logical_clock = snapshot->logical_clock;
    out->winding = (int)llround(winding_total / (2.0 * M_PI));
    out->maximum_edge_delta = maximum_delta;
    memcpy(out->snapshot_digest, snapshot->snapshot_digest,
           CDC_DIGEST_SIZE);
    cdc_digest_final(&boundary, out->oriented_boundary_digest);

    cdc_digest_init(&sector);
    cdc_digest_update(&sector, SECTOR_DOMAIN, sizeof(SECTOR_DOMAIN));
    cdc_digest_update(&sector, out->oriented_boundary_digest,
                      CDC_DIGEST_SIZE);
    digest_u64(&sector, out->topology_version);
    digest_u64(&sector, (uint64_t)(int64_t)out->winding);
    cdc_digest_final(&sector, out->sector_digest);
    return CDC_TOPOLOGY_OK;
}

cdc_topology_status
cdc_topology_verify_transition(const cdc_topology_state *previous,
                               const cdc_topology_state *next,
                               const cdc_frame_snapshot *next_snapshot,
                               const cdc_topology_witness *witness) {
    int delta;
    if (!previous || !next || !next_snapshot ||
        previous->topology_version != next->topology_version ||
        next->logical_clock <= previous->logical_clock ||
        memcmp(next->snapshot_digest, next_snapshot->snapshot_digest,
               CDC_DIGEST_SIZE) != 0) {
        return CDC_TOPOLOGY_ECLOCK;
    }
    if (memcmp(previous->oriented_boundary_digest,
               next->oriented_boundary_digest, CDC_DIGEST_SIZE) != 0) {
        return CDC_TOPOLOGY_EBOUNDARY;
    }
    delta = next->winding - previous->winding;
    if (delta == 0) {
        if (!witness || witness->kind == CDC_TOPOLOGY_EVENT_NONE) {
            return CDC_TOPOLOGY_OK;
        }
        if (witness->kind != CDC_TOPOLOGY_EVENT_LOCAL_DEFORMATION ||
            digest_is_zero(witness->evidence_digest) ||
            witness->logical_clock <= previous->logical_clock ||
            witness->logical_clock > next->logical_clock ||
            !edge_exists(next_snapshot, witness->from_member,
                         witness->to_member)) {
            return CDC_TOPOLOGY_EWITNESS;
        }
        return CDC_TOPOLOGY_OK;
    }
    if (abs(delta) != 1) {
        return CDC_TOPOLOGY_ESECTOR;
    }
    if (!witness || witness->kind != CDC_TOPOLOGY_EVENT_PHASE_SLIP ||
        digest_is_zero(witness->evidence_digest) ||
        witness->logical_clock <= previous->logical_clock ||
        witness->logical_clock > next->logical_clock ||
        !edge_exists(next_snapshot, witness->from_member,
                     witness->to_member)) {
        return CDC_TOPOLOGY_EWITNESS;
    }
    return CDC_TOPOLOGY_OK;
}

const char *cdc_topology_status_name(cdc_topology_status status) {
    static const char *const NAMES[] = {
        "ok", "argument", "boundary", "sector", "witness", "clock",
    };
    if ((size_t)status >= sizeof(NAMES) / sizeof(NAMES[0])) {
        return "unknown";
    }
    return NAMES[status];
}
