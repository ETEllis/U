#include "cdc_cell.h"

#include "cdc_rftc.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const uint8_t CELL_DOMAIN[] = "CDC-RFTC-CELL-V1";
static const uint8_t HIDDEN_DOMAIN[] = "CDC-RFTC-HIDDEN-CLASS-V1";

static size_t bounded_length(const char *text, size_t limit) {
    size_t n = 0;
    if (!text) {
        return limit + 1;
    }
    while (n <= limit && text[n] != '\0') {
        n++;
    }
    return n;
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
        bytes[sizeof(bytes) - 1 - i] = (uint8_t)(value >> (i * 8));
    }
    cdc_digest_update(ctx, bytes, sizeof(bytes));
}

static void digest_double(cdc_digest_ctx *ctx, double value) {
    int64_t quantized = (int64_t)llround(value * 1000000000000.0);
    digest_u64(ctx, (uint64_t)quantized);
}

static void hidden_class_digest(const cdc_cell_state *state,
                                uint8_t out[CDC_DIGEST_SIZE]) {
    cdc_digest_ctx digest;
    cdc_digest_init(&digest);
    cdc_digest_update(&digest, HIDDEN_DOMAIN, sizeof(HIDDEN_DOMAIN));
    cdc_digest_update(&digest, state->frame_snapshot_digest,
                      CDC_DIGEST_SIZE);
    digest_u64(&digest, (uint64_t)state->transition_kind);
    digest_u64(&digest, state->transition_clock);
    digest_u64(&digest, state->transition_from_member);
    digest_u64(&digest, state->transition_to_member);
    cdc_digest_update(&digest, state->transition_evidence_digest,
                      CDC_DIGEST_SIZE);
    cdc_digest_final(&digest, out);
}

static void cell_state_digest(const cdc_cell_state *state,
                              uint8_t out[CDC_DIGEST_SIZE]) {
    cdc_digest_ctx digest;
    cdc_digest_init(&digest);
    cdc_digest_update(&digest, CELL_DOMAIN, sizeof(CELL_DOMAIN));
    digest_u64(&digest, (uint64_t)strlen(state->cell_id));
    cdc_digest_update(&digest, state->cell_id, strlen(state->cell_id));
    digest_u64(&digest, state->generation);
    digest_u64(&digest, state->frame_version);
    digest_u64(&digest, state->reducer_version);
    digest_u64(&digest, state->topology_version);
    digest_u64(&digest, state->logical_clock);
    digest_u64(&digest, state->causal_horizon);
    digest_u64(&digest, state->window_start);
    digest_u64(&digest, state->window_end);
    digest_u64(&digest, state->freshness);
    digest_double(&digest, state->amplitude);
    digest_double(&digest, state->mean_phase);
    digest_double(&digest, state->dispersion);
    digest_u64(&digest, (uint64_t)(int64_t)state->winding);
    digest_u64(&digest, (uint64_t)state->member_count);
    digest_u64(&digest, (uint64_t)state->transition_kind);
    digest_u64(&digest, state->transition_clock);
    digest_u64(&digest, state->transition_from_member);
    digest_u64(&digest, state->transition_to_member);
    cdc_digest_update(&digest, state->frame_snapshot_digest,
                      CDC_DIGEST_SIZE);
    cdc_digest_update(&digest, state->oriented_boundary_digest,
                      CDC_DIGEST_SIZE);
    cdc_digest_update(&digest, state->sector_digest, CDC_DIGEST_SIZE);
    cdc_digest_update(&digest, state->transition_evidence_digest,
                      CDC_DIGEST_SIZE);
    cdc_digest_update(&digest, state->hidden_class_digest,
                      CDC_DIGEST_SIZE);
    cdc_digest_update(&digest, state->previous_state_digest,
                      CDC_DIGEST_SIZE);
    cdc_digest_final(&digest, out);
}

cdc_cell_status cdc_cell_state_validate(const cdc_cell_state *state) {
    uint8_t expected_hidden[CDC_DIGEST_SIZE];
    uint8_t expected_state[CDC_DIGEST_SIZE];
    int has_transition;
    if (!state ||
        bounded_length(state->cell_id, CDC_CELL_ID_MAX) == 0 ||
        bounded_length(state->cell_id, CDC_CELL_ID_MAX) >
            CDC_CELL_ID_MAX ||
        state->generation == 0 || state->frame_version == 0 ||
        state->reducer_version == 0 || state->topology_version == 0 ||
        state->logical_clock == 0 ||
        state->causal_horizon < state->logical_clock ||
        state->window_end <= state->window_start ||
        state->member_count < 2 || !isfinite(state->amplitude) ||
        !isfinite(state->mean_phase) || !isfinite(state->dispersion) ||
        state->amplitude < 0.0 || state->amplitude > 1.0 + 1e-12 ||
        state->dispersion < -1e-12 ||
        state->dispersion > 1.0 + 1e-12 ||
        fabs((state->amplitude + state->dispersion) - 1.0) > 1e-9 ||
        state->mean_phase < -M_PI - 1e-12 ||
        state->mean_phase > M_PI + 1e-12 ||
        state->transition_kind < CDC_TOPOLOGY_EVENT_NONE ||
        state->transition_kind > CDC_TOPOLOGY_EVENT_FRAME_CHANGE ||
        digest_is_zero(state->frame_snapshot_digest) ||
        digest_is_zero(state->oriented_boundary_digest) ||
        digest_is_zero(state->sector_digest) ||
        digest_is_zero(state->hidden_class_digest) ||
        digest_is_zero(state->state_digest)) {
        return CDC_CELL_EARG;
    }
    has_transition =
        state->transition_kind != CDC_TOPOLOGY_EVENT_NONE;
    if ((!has_transition &&
         (state->transition_clock != 0 ||
          state->transition_from_member != 0 ||
          state->transition_to_member != 0 ||
          !digest_is_zero(state->transition_evidence_digest))) ||
        (has_transition &&
         (state->transition_clock == 0 ||
          digest_is_zero(state->transition_evidence_digest))) ||
        (state->generation == 1 &&
         !digest_is_zero(state->previous_state_digest)) ||
        (state->generation > 1 &&
         digest_is_zero(state->previous_state_digest))) {
        return CDC_CELL_EARG;
    }
    hidden_class_digest(state, expected_hidden);
    if (memcmp(expected_hidden, state->hidden_class_digest,
               CDC_DIGEST_SIZE) != 0) {
        return CDC_CELL_EARG;
    }
    cell_state_digest(state, expected_state);
    return memcmp(expected_state, state->state_digest, CDC_DIGEST_SIZE) == 0
               ? CDC_CELL_OK
               : CDC_CELL_EARG;
}

static int frame_change_valid(const cdc_cell_state *previous,
                              const cdc_frame_snapshot *snapshot,
                              const cdc_topology_witness *witness) {
    return snapshot->frame_version == previous->frame_version + 1 &&
           snapshot->reducer_version >= previous->reducer_version &&
           snapshot->topology_version >= previous->topology_version &&
           witness && witness->kind == CDC_TOPOLOGY_EVENT_FRAME_CHANGE &&
           !digest_is_zero(witness->evidence_digest) &&
           witness->logical_clock > previous->logical_clock &&
           witness->logical_clock <= snapshot->logical_clock;
}

cdc_cell_status
cdc_cell_reduce(const char *cell_id, const cdc_frame_snapshot *snapshot,
                uint64_t causal_horizon,
                const cdc_cell_state *previous,
                const cdc_topology_witness *witness,
                cdc_cell_state *out) {
    cdc_topology_state topology;
    cdc_rftc_frame frame;
    cdc_rftc_state macro;
    double *phases = NULL;
    uint64_t *member_ids = NULL;
    size_t i;

    if (!cell_id || !snapshot || !out || !snapshot->observations ||
        bounded_length(cell_id, CDC_CELL_ID_MAX) == 0 ||
        bounded_length(cell_id, CDC_CELL_ID_MAX) > CDC_CELL_ID_MAX ||
        snapshot->member_count < 2 ||
        causal_horizon < snapshot->logical_clock ||
        digest_is_zero(snapshot->snapshot_digest)) {
        return CDC_CELL_EARG;
    }
    if (cdc_topology_classify(snapshot, &topology) != CDC_TOPOLOGY_OK) {
        return CDC_CELL_ETOPOLOGY;
    }
    if (previous) {
        if (cdc_cell_state_validate(previous) != CDC_CELL_OK ||
            strcmp(previous->cell_id, cell_id) != 0 ||
            previous->logical_clock >= snapshot->logical_clock ||
            causal_horizon < previous->causal_horizon ||
            snapshot->window_start < previous->window_start ||
            snapshot->window_end <= previous->window_end ||
            digest_is_zero(previous->state_digest)) {
            return CDC_CELL_ECLOCK;
        }
        if (snapshot->frame_version == previous->frame_version) {
            cdc_topology_state prior_topology;
            if (snapshot->reducer_version != previous->reducer_version ||
                snapshot->topology_version != previous->topology_version) {
                return CDC_CELL_EFRAME;
            }
            memset(&prior_topology, 0, sizeof(prior_topology));
            prior_topology.frame_version = previous->frame_version;
            prior_topology.topology_version = previous->topology_version;
            prior_topology.logical_clock = previous->logical_clock;
            prior_topology.winding = previous->winding;
            memcpy(prior_topology.oriented_boundary_digest,
                   previous->oriented_boundary_digest, CDC_DIGEST_SIZE);
            memcpy(prior_topology.sector_digest, previous->sector_digest,
                   CDC_DIGEST_SIZE);
            if (cdc_topology_verify_transition(
                    &prior_topology, &topology, snapshot, witness) !=
                CDC_TOPOLOGY_OK) {
                return CDC_CELL_ETRANSITION;
            }
        } else if (!frame_change_valid(previous, snapshot, witness)) {
            return CDC_CELL_EFRAME;
        }
    } else if (witness && witness->kind != CDC_TOPOLOGY_EVENT_NONE) {
        return CDC_CELL_ETRANSITION;
    }

    phases = malloc(snapshot->member_count * sizeof(*phases));
    member_ids = malloc(snapshot->member_count * sizeof(*member_ids));
    if (!phases || !member_ids) {
        free(phases);
        free(member_ids);
        return CDC_CELL_ELIMIT;
    }
    for (i = 0; i < snapshot->member_count; i++) {
        phases[i] = snapshot->observations[i].phase;
        member_ids[i] = snapshot->observations[i].member_id;
    }
    memset(&frame, 0, sizeof(frame));
    frame.phases = phases;
    frame.member_ids = member_ids;
    frame.member_count = snapshot->member_count;
    frame.frame_version = snapshot->frame_version;
    frame.topology_version = snapshot->topology_version;
    frame.logical_clock = snapshot->logical_clock;
    if (cdc_rftc_reduce(&frame, &macro) != CDC_RFTC_OK) {
        free(phases);
        free(member_ids);
        return CDC_CELL_EARG;
    }
    free(phases);
    free(member_ids);

    memset(out, 0, sizeof(*out));
    memcpy(out->cell_id, cell_id, strlen(cell_id) + 1);
    out->generation = previous ? previous->generation + 1 : 1;
    out->frame_version = snapshot->frame_version;
    out->reducer_version = snapshot->reducer_version;
    out->topology_version = snapshot->topology_version;
    out->logical_clock = snapshot->logical_clock;
    out->causal_horizon = causal_horizon;
    out->window_start = snapshot->window_start;
    out->window_end = snapshot->window_end;
    out->freshness = snapshot->freshness;
    out->amplitude = macro.amplitude;
    out->mean_phase = macro.mean_phase;
    out->dispersion = macro.dispersion;
    out->winding = topology.winding;
    out->member_count = snapshot->member_count;
    if (witness && witness->kind != CDC_TOPOLOGY_EVENT_NONE) {
        out->transition_kind = witness->kind;
        out->transition_clock = witness->logical_clock;
        out->transition_from_member = witness->from_member;
        out->transition_to_member = witness->to_member;
        memcpy(out->transition_evidence_digest, witness->evidence_digest,
               CDC_DIGEST_SIZE);
    }
    memcpy(out->frame_snapshot_digest, snapshot->snapshot_digest,
           CDC_DIGEST_SIZE);
    memcpy(out->oriented_boundary_digest,
           topology.oriented_boundary_digest, CDC_DIGEST_SIZE);
    memcpy(out->sector_digest, topology.sector_digest, CDC_DIGEST_SIZE);
    hidden_class_digest(out, out->hidden_class_digest);
    if (previous) {
        memcpy(out->previous_state_digest, previous->state_digest,
               CDC_DIGEST_SIZE);
    }

    cell_state_digest(out, out->state_digest);
    return CDC_CELL_OK;
}

const char *cdc_cell_status_name(cdc_cell_status status) {
    static const char *const NAMES[] = {
        "ok",       "argument", "limit", "frame",
        "topology", "transition", "clock",
    };
    if ((size_t)status >= sizeof(NAMES) / sizeof(NAMES[0])) {
        return "unknown";
    }
    return NAMES[status];
}
