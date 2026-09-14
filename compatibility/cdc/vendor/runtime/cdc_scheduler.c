#include "cdc_scheduler.h"

#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t TRACE_DOMAIN[] = "CDC-RFTC-SCHEDULER-TRACE-V1";
static const uint8_t CONFIG_DOMAIN[] = "CDC-RFTC-SCHEDULER-CONFIG-V2";
static const uint8_t CELL_SPEC_DOMAIN[] =
    "CDC-RFTC-SCHEDULER-CELL-SPEC-V1";

typedef enum {
    SCHEDULER_EVENT_OBSERVATION = 1,
    SCHEDULER_EVENT_WITNESS = 2
} scheduler_event_kind;

typedef struct {
    int present;
    cdc_scheduler_observation observation;
} pending_slot;

typedef struct {
    char cell_id[CDC_CELL_ID_MAX + 1];
    cdc_scheduler_cell_kind kind;
    uint64_t *member_ids;
    uint64_t *successor_ids;
    size_t *children;
    size_t member_count;
    size_t parent;
    size_t depth;
    uint64_t frame_version;
    uint64_t reducer_version;
    uint64_t topology_version;
    uint64_t stale_after;
    uint64_t causal_horizon;
    pending_slot *pending;
    pending_slot *committed;
    size_t pending_count;
    int batch_active;
    uint64_t batch_clock;
    uint64_t batch_window_start;
    uint64_t batch_window_end;
    uint64_t batch_seal_time;
    int has_witness;
    uint64_t witness_target_clock;
    cdc_topology_witness witness;
    int has_committed_witness;
    uint64_t committed_witness_target_clock;
    cdc_topology_witness committed_witness;
    int has_state;
    int has_imported_state;
    uint8_t imported_structure_digest[CDC_DIGEST_SIZE];
    cdc_cell_state state;
} cell_entry;

typedef struct {
    scheduler_event_kind kind;
    cdc_scheduler_observation observation;
    char cell_id[CDC_CELL_ID_MAX + 1];
    uint64_t logical_clock;
    cdc_topology_witness witness;
    uint64_t sequence;
} scheduler_event;

struct cdc_scheduler {
    pthread_mutex_t mutex;
    int mutex_ready;
    cell_entry *cells;
    size_t cell_count;
    size_t cell_limit;
    scheduler_event *events;
    size_t event_count;
    size_t event_limit;
    size_t maximum_depth;
    uint64_t next_sequence;
    int sealed;
    int has_predecessor_configuration;
    uint8_t predecessor_configuration_digest[CDC_DIGEST_SIZE];
    uint8_t configuration_digest[CDC_DIGEST_SIZE];
    uint8_t execution_digest[CDC_DIGEST_SIZE];
};

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
        bytes[sizeof(bytes) - 1 - i] = (uint8_t)(value >> (8 * i));
    }
    cdc_digest_update(ctx, bytes, sizeof(bytes));
}

static void digest_string(cdc_digest_ctx *ctx, const char *text) {
    digest_u64(ctx, (uint64_t)strlen(text));
    cdc_digest_update(ctx, text, strlen(text));
}

static size_t find_cell(const cdc_scheduler *scheduler, const char *cell_id) {
    size_t i;
    for (i = 0; i < scheduler->cell_count; i++) {
        if (strcmp(scheduler->cells[i].cell_id, cell_id) == 0) {
            return i;
        }
    }
    return SIZE_MAX;
}

static size_t find_member(const cell_entry *cell, uint64_t member_id) {
    size_t i;
    for (i = 0; i < cell->member_count; i++) {
        if (cell->member_ids[i] == member_id) {
            return i;
        }
    }
    return SIZE_MAX;
}

static int ring_valid(const uint64_t *members, const uint64_t *successors,
                      size_t count) {
    size_t anchor = 0;
    size_t current;
    size_t i;
    if (!members || !successors || count < 2) {
        return 0;
    }
    for (i = 0; i < count; i++) {
        size_t j;
        size_t successor_matches = 0;
        if (members[i] == 0 || successors[i] == 0) {
            return 0;
        }
        if (members[i] < members[anchor]) {
            anchor = i;
        }
        for (j = i + 1; j < count; j++) {
            if (members[i] == members[j]) {
                return 0;
            }
        }
        for (j = 0; j < count; j++) {
            if (members[j] == successors[i]) {
                successor_matches++;
            }
            if (i != j && successors[i] == successors[j]) {
                return 0;
            }
        }
        if (successor_matches != 1) {
            return 0;
        }
    }
    current = anchor;
    for (i = 0; i < count; i++) {
        size_t next = SIZE_MAX;
        size_t j;
        if (i > 0 && current == anchor) {
            return 0;
        }
        for (j = 0; j < count; j++) {
            if (members[j] == successors[current]) {
                next = j;
                break;
            }
        }
        if (next == SIZE_MAX) {
            return 0;
        }
        current = next;
    }
    return current == anchor;
}

static int witness_equal(const cdc_topology_witness *a,
                         const cdc_topology_witness *b) {
    return a->kind == b->kind && a->from_member == b->from_member &&
           a->to_member == b->to_member &&
           a->logical_clock == b->logical_clock &&
           memcmp(a->evidence_digest, b->evidence_digest,
                  CDC_DIGEST_SIZE) == 0;
}

static int observation_core_equal(const cdc_scheduler_observation *a,
                                  const cdc_scheduler_observation *b) {
    return strcmp(a->cell_id, b->cell_id) == 0 &&
           a->member_id == b->member_id && a->phase == b->phase &&
           a->observed_at == b->observed_at &&
           a->logical_clock == b->logical_clock &&
           a->frame_version == b->frame_version &&
           a->window_start == b->window_start &&
           a->window_end == b->window_end &&
           a->seal_time == b->seal_time &&
           memcmp(a->source_digest, b->source_digest,
                  CDC_DIGEST_SIZE) == 0;
}

static void trace_state(cdc_scheduler *scheduler,
                        const cdc_cell_state *state) {
    cdc_digest_ctx digest;
    uint8_t next[CDC_DIGEST_SIZE];
    cdc_digest_init(&digest);
    cdc_digest_update(&digest, TRACE_DOMAIN, sizeof(TRACE_DOMAIN));
    cdc_digest_update(&digest, scheduler->execution_digest,
                      CDC_DIGEST_SIZE);
    cdc_digest_update(&digest, state->state_digest, CDC_DIGEST_SIZE);
    cdc_digest_final(&digest, next);
    memcpy(scheduler->execution_digest, next, CDC_DIGEST_SIZE);
}

static const char *event_cell_id(const scheduler_event *event) {
    return event->kind == SCHEDULER_EVENT_OBSERVATION
               ? event->observation.cell_id
               : event->cell_id;
}

static uint64_t event_clock(const scheduler_event *event) {
    return event->kind == SCHEDULER_EVENT_OBSERVATION
               ? event->observation.logical_clock
               : event->logical_clock;
}

static uint64_t event_member(const scheduler_event *event) {
    return event->kind == SCHEDULER_EVENT_OBSERVATION
               ? event->observation.member_id
               : 0;
}

static int event_before(const scheduler_event *a,
                        const scheduler_event *b) {
    int id_order;
    if (event_clock(a) != event_clock(b)) {
        return event_clock(a) < event_clock(b);
    }
    if (a->kind != b->kind) {
        return a->kind < b->kind;
    }
    id_order = strcmp(event_cell_id(a), event_cell_id(b));
    if (id_order != 0) {
        return id_order < 0;
    }
    if (event_member(a) != event_member(b)) {
        return event_member(a) < event_member(b);
    }
    return a->sequence < b->sequence;
}

static size_t minimum_event(const cdc_scheduler *scheduler) {
    size_t minimum = 0;
    size_t i;
    for (i = 1; i < scheduler->event_count; i++) {
        if (event_before(&scheduler->events[i],
                         &scheduler->events[minimum])) {
            minimum = i;
        }
    }
    return minimum;
}

static void remove_event(cdc_scheduler *scheduler, size_t index) {
    if (index + 1 < scheduler->event_count) {
        memmove(&scheduler->events[index], &scheduler->events[index + 1],
                (scheduler->event_count - index - 1) *
                    sizeof(*scheduler->events));
    }
    scheduler->event_count--;
}

static int cell_id_before(const cdc_scheduler *scheduler, size_t a, size_t b) {
    return strcmp(scheduler->cells[a].cell_id,
                  scheduler->cells[b].cell_id) < 0;
}

static int compute_cell_structure_digest(
    const cdc_scheduler *scheduler, size_t index,
    uint8_t out[CDC_DIGEST_SIZE]) {
    const cell_entry *cell;
    cdc_digest_ctx digest;
    size_t member;
    size_t current = 0;
    if (!scheduler || index >= scheduler->cell_count || !out) {
        return 0;
    }
    cell = &scheduler->cells[index];
    cdc_digest_init(&digest);
    cdc_digest_update(&digest, CELL_SPEC_DOMAIN,
                      sizeof(CELL_SPEC_DOMAIN));
    digest_string(&digest, cell->cell_id);
    digest_u64(&digest, (uint64_t)cell->kind);
    digest_u64(&digest, (uint64_t)cell->member_count);
    digest_u64(&digest, (uint64_t)cell->depth);
    digest_u64(&digest, cell->frame_version);
    digest_u64(&digest, cell->reducer_version);
    digest_u64(&digest, cell->topology_version);
    digest_u64(&digest, cell->stale_after);
    digest_u64(&digest, cell->causal_horizon);
    for (member = 1; member < cell->member_count; member++) {
        if (cell->member_ids[member] < cell->member_ids[current]) {
            current = member;
        }
    }
    for (member = 0; member < cell->member_count; member++) {
        size_t next = SIZE_MAX;
        size_t candidate;
        digest_u64(&digest, cell->member_ids[current]);
        digest_u64(&digest, cell->successor_ids[current]);
        if (cell->kind == CDC_SCHEDULER_COMPOSITE) {
            digest_string(
                &digest,
                scheduler->cells[cell->children[current]].cell_id);
        }
        for (candidate = 0; candidate < cell->member_count;
             candidate++) {
            if (cell->member_ids[candidate] ==
                cell->successor_ids[current]) {
                next = candidate;
                break;
            }
        }
        if (next == SIZE_MAX) {
            return 0;
        }
        current = next;
    }
    cdc_digest_final(&digest, out);
    return 1;
}

static int compute_configuration_digest(cdc_scheduler *scheduler) {
    size_t *order;
    cdc_digest_ctx digest;
    size_t i;
    order = malloc(scheduler->cell_count * sizeof(*order));
    if (!order) {
        return 0;
    }
    for (i = 0; i < scheduler->cell_count; i++) {
        size_t position = i;
        order[i] = i;
        while (position > 0 &&
               cell_id_before(scheduler, order[position],
                              order[position - 1])) {
            size_t temporary = order[position - 1];
            order[position - 1] = order[position];
            order[position] = temporary;
            position--;
        }
    }
    cdc_digest_init(&digest);
    cdc_digest_update(&digest, CONFIG_DOMAIN, sizeof(CONFIG_DOMAIN));
    digest_u64(&digest, (uint64_t)scheduler->cell_count);
    digest_u64(&digest, (uint64_t)scheduler->cell_limit);
    digest_u64(&digest, (uint64_t)scheduler->event_limit);
    digest_u64(&digest, (uint64_t)scheduler->maximum_depth);
    digest_u64(&digest,
               (uint64_t)scheduler->has_predecessor_configuration);
    if (scheduler->has_predecessor_configuration) {
        cdc_digest_update(
            &digest, scheduler->predecessor_configuration_digest,
            CDC_DIGEST_SIZE);
    }
    for (i = 0; i < scheduler->cell_count; i++) {
        const cell_entry *cell = &scheduler->cells[order[i]];
        uint8_t structure_digest[CDC_DIGEST_SIZE];
        if (!compute_cell_structure_digest(
                scheduler, order[i], structure_digest)) {
            free(order);
            return 0;
        }
        cdc_digest_update(&digest, structure_digest, CDC_DIGEST_SIZE);
        digest_u64(&digest, (uint64_t)cell->has_imported_state);
        if (cell->has_imported_state) {
            cdc_digest_update(&digest, cell->imported_structure_digest,
                              CDC_DIGEST_SIZE);
            cdc_digest_update(&digest, cell->state.state_digest,
                              CDC_DIGEST_SIZE);
        }
    }
    cdc_digest_final(&digest, scheduler->configuration_digest);
    free(order);
    return 1;
}

static cdc_scheduler_status
reduce_composite(cdc_scheduler *scheduler, size_t index,
                 cdc_scheduler_run_report *report);

static cdc_scheduler_status
propagate_parent(cdc_scheduler *scheduler, size_t child,
                 cdc_scheduler_run_report *report) {
    size_t parent = scheduler->cells[child].parent;
    if (parent != SIZE_MAX) {
        cdc_scheduler_status status =
            reduce_composite(scheduler, parent, report);
        if (status == CDC_SCHEDULER_HOLD_LIMIT ||
            status >= CDC_SCHEDULER_EARG) {
            return status;
        }
    }
    return CDC_SCHEDULER_OK;
}

static cdc_scheduler_status
reduce_leaf(cdc_scheduler *scheduler, size_t index,
            cdc_scheduler_run_report *report) {
    cell_entry *cell = &scheduler->cells[index];
    cdc_frame_observation *observations;
    cdc_frame_snapshot snapshot;
    cdc_frame_config config;
    cdc_cell_state next;
    cdc_cell_status status;
    cdc_frame_status frame_status;
    const cdc_topology_witness *witness = NULL;
    cdc_scheduler_status parent_status;
    size_t i;

    if (cell->pending_count != cell->member_count) {
        return CDC_SCHEDULER_HOLD_INCOMPLETE;
    }
    observations = calloc(cell->member_count, sizeof(*observations));
    if (!observations) {
        return CDC_SCHEDULER_HOLD_LIMIT;
    }
    for (i = 0; i < cell->member_count; i++) {
        observations[i].member_id = cell->member_ids[i];
        observations[i].successor_id = cell->successor_ids[i];
        observations[i].phase = cell->pending[i].observation.phase;
        observations[i].observed_at =
            cell->pending[i].observation.observed_at;
        observations[i].logical_clock =
            cell->pending[i].observation.logical_clock;
        memcpy(observations[i].source_digest,
               cell->pending[i].observation.source_digest,
               CDC_DIGEST_SIZE);
    }
    memset(&config, 0, sizeof(config));
    config.frame_id = cell->cell_id;
    config.frame_version = cell->frame_version;
    config.reducer_version = cell->reducer_version;
    config.topology_version = cell->topology_version;
    config.window_start = cell->batch_window_start;
    config.window_end = cell->batch_window_end;
    config.stale_after = cell->stale_after;
    config.minimum_members = cell->member_count;
    config.maximum_members = cell->member_count;
    cdc_frame_snapshot_init(&snapshot);
    frame_status =
        cdc_frame_seal(&config, observations, cell->member_count,
                       cell->batch_seal_time, &snapshot);
    if (frame_status != CDC_FRAME_OK) {
        free(observations);
        if (frame_status == CDC_FRAME_ELIMIT) {
            return CDC_SCHEDULER_HOLD_LIMIT;
        }
        if (frame_status == CDC_FRAME_HOLD_STALE) {
            return CDC_SCHEDULER_HOLD_STALE;
        }
        return CDC_SCHEDULER_EFRAME;
    }
    free(observations);
    if (cell->has_witness &&
        cell->witness_target_clock == cell->batch_clock) {
        witness = &cell->witness;
    }
    status = cdc_cell_reduce(
        cell->cell_id, &snapshot, cell->causal_horizon,
        cell->has_state ? &cell->state : NULL,
        witness, &next);
    cdc_frame_snapshot_free(&snapshot);
    if (status == CDC_CELL_ETRANSITION || status == CDC_CELL_EFRAME) {
        report->frames_held++;
        if (witness) {
            cell->has_witness = 0;
            cell->witness_target_clock = 0;
            memset(&cell->witness, 0, sizeof(cell->witness));
            return CDC_SCHEDULER_ETRANSITION;
        }
        return CDC_SCHEDULER_HOLD_INCOMPLETE;
    }
    if (status == CDC_CELL_ELIMIT) {
        return CDC_SCHEDULER_HOLD_LIMIT;
    }
    if (status != CDC_CELL_OK) {
        return CDC_SCHEDULER_ETRANSITION;
    }
    cell->state = next;
    cell->has_state = 1;
    cell->has_imported_state = 0;
    memcpy(cell->committed, cell->pending,
           cell->member_count * sizeof(*cell->committed));
    cell->has_committed_witness = witness != NULL;
    cell->committed_witness_target_clock =
        witness ? cell->batch_clock : 0;
    if (witness) {
        cell->committed_witness = *witness;
    } else {
        memset(&cell->committed_witness, 0,
               sizeof(cell->committed_witness));
    }
    memset(cell->pending, 0,
           cell->member_count * sizeof(*cell->pending));
    cell->pending_count = 0;
    cell->batch_active = 0;
    if (witness) {
        cell->has_witness = 0;
        cell->witness_target_clock = 0;
        memset(&cell->witness, 0, sizeof(cell->witness));
    }
    report->leaf_updates++;
    trace_state(scheduler, &cell->state);
    parent_status = propagate_parent(scheduler, index, report);
    if (parent_status != CDC_SCHEDULER_OK) {
        return parent_status;
    }
    return CDC_SCHEDULER_OK;
}

static cdc_scheduler_status
reduce_composite(cdc_scheduler *scheduler, size_t index,
                 cdc_scheduler_run_report *report) {
    cell_entry *cell = &scheduler->cells[index];
    cdc_frame_observation *observations;
    cdc_frame_snapshot snapshot;
    cdc_frame_config config;
    cdc_cell_state next;
    uint64_t logical_clock;
    uint64_t window_start;
    uint64_t window_end;
    uint64_t horizon;
    size_t i;
    cdc_cell_status status;
    cdc_frame_status frame_status;
    const cdc_topology_witness *witness = NULL;
    cdc_scheduler_status parent_status;

    if (cell->kind != CDC_SCHEDULER_COMPOSITE ||
        cell->member_count < 2) {
        return CDC_SCHEDULER_EARG;
    }
    for (i = 0; i < cell->member_count; i++) {
        if (!scheduler->cells[cell->children[i]].has_state) {
            return CDC_SCHEDULER_HOLD_INCOMPLETE;
        }
    }
    logical_clock =
        scheduler->cells[cell->children[0]].state.logical_clock;
    window_start =
        scheduler->cells[cell->children[0]].state.window_start;
    window_end = scheduler->cells[cell->children[0]].state.window_end;
    horizon = scheduler->cells[cell->children[0]].state.causal_horizon;
    for (i = 1; i < cell->member_count; i++) {
        const cdc_cell_state *child =
            &scheduler->cells[cell->children[i]].state;
        if (child->logical_clock != logical_clock) {
            return CDC_SCHEDULER_HOLD_INCOMPLETE;
        }
        if (child->window_start < window_start) {
            window_start = child->window_start;
        }
        if (child->window_end > window_end) {
            window_end = child->window_end;
        }
        if (child->causal_horizon < horizon) {
            horizon = child->causal_horizon;
        }
    }
    if (cell->has_state && logical_clock <= cell->state.logical_clock) {
        return CDC_SCHEDULER_HOLD_DUPLICATE;
    }

    observations = calloc(cell->member_count, sizeof(*observations));
    if (!observations) {
        return CDC_SCHEDULER_HOLD_LIMIT;
    }
    for (i = 0; i < cell->member_count; i++) {
        const cdc_cell_state *child =
            &scheduler->cells[cell->children[i]].state;
        observations[i].member_id = cell->member_ids[i];
        observations[i].successor_id = cell->successor_ids[i];
        observations[i].phase = child->mean_phase;
        observations[i].observed_at = child->window_end;
        observations[i].logical_clock = child->logical_clock;
        memcpy(observations[i].source_digest, child->state_digest,
               CDC_DIGEST_SIZE);
    }
    memset(&config, 0, sizeof(config));
    config.frame_id = cell->cell_id;
    config.frame_version = cell->frame_version;
    config.reducer_version = cell->reducer_version;
    config.topology_version = cell->topology_version;
    config.window_start = window_start;
    config.window_end = window_end;
    config.stale_after = cell->stale_after;
    config.minimum_members = cell->member_count;
    config.maximum_members = cell->member_count;
    cdc_frame_snapshot_init(&snapshot);
    frame_status = cdc_frame_seal(&config, observations, cell->member_count,
                                  window_end, &snapshot);
    if (frame_status != CDC_FRAME_OK) {
        free(observations);
        report->frames_held++;
        if (frame_status == CDC_FRAME_ELIMIT) {
            return CDC_SCHEDULER_HOLD_LIMIT;
        }
        return CDC_SCHEDULER_HOLD_INCOMPLETE;
    }
    free(observations);
    if (cell->has_witness &&
        cell->witness_target_clock == logical_clock) {
        witness = &cell->witness;
    }
    status = cdc_cell_reduce(
        cell->cell_id, &snapshot,
        horizon < cell->causal_horizon ? horizon : cell->causal_horizon,
        cell->has_state ? &cell->state : NULL, witness, &next);
    cdc_frame_snapshot_free(&snapshot);
    if (status == CDC_CELL_ETRANSITION || status == CDC_CELL_EFRAME) {
        report->frames_held++;
        if (witness) {
            cell->has_witness = 0;
            cell->witness_target_clock = 0;
            memset(&cell->witness, 0, sizeof(cell->witness));
            return CDC_SCHEDULER_ETRANSITION;
        }
        return CDC_SCHEDULER_HOLD_INCOMPLETE;
    }
    if (status == CDC_CELL_ELIMIT) {
        return CDC_SCHEDULER_HOLD_LIMIT;
    }
    if (status != CDC_CELL_OK) {
        return CDC_SCHEDULER_ETRANSITION;
    }
    cell->state = next;
    cell->has_state = 1;
    cell->has_imported_state = 0;
    cell->has_committed_witness = witness != NULL;
    cell->committed_witness_target_clock =
        witness ? logical_clock : 0;
    if (witness) {
        cell->committed_witness = *witness;
    } else {
        memset(&cell->committed_witness, 0,
               sizeof(cell->committed_witness));
    }
    if (witness) {
        cell->has_witness = 0;
        cell->witness_target_clock = 0;
        memset(&cell->witness, 0, sizeof(cell->witness));
    }
    report->recursive_updates++;
    trace_state(scheduler, &cell->state);
    parent_status = propagate_parent(scheduler, index, report);
    if (parent_status != CDC_SCHEDULER_OK) {
        return parent_status;
    }
    return CDC_SCHEDULER_OK;
}

static cdc_scheduler_status
apply_observation_event(cdc_scheduler *scheduler,
                        const scheduler_event *event,
                        cdc_scheduler_run_report *report, int *consumed) {
    size_t cell_index = find_cell(scheduler, event->observation.cell_id);
    cell_entry *cell;
    size_t member_index;
    int duplicate = 0;
    *consumed = 0;
    if (cell_index == SIZE_MAX) {
        return CDC_SCHEDULER_ENOTFOUND;
    }
    cell = &scheduler->cells[cell_index];
    if (cell->kind != CDC_SCHEDULER_LEAF) {
        return CDC_SCHEDULER_EARG;
    }
    member_index = find_member(cell, event->observation.member_id);
    if (member_index == SIZE_MAX) {
        return CDC_SCHEDULER_ENOTFOUND;
    }
    if (cell->has_state &&
        event->observation.logical_clock <= cell->state.logical_clock) {
        *consumed = 1;
        if (event->observation.logical_clock == cell->state.logical_clock &&
            cell->committed[member_index].present &&
            observation_core_equal(
                &cell->committed[member_index].observation,
                &event->observation)) {
            return CDC_SCHEDULER_HOLD_DUPLICATE;
        }
        return CDC_SCHEDULER_EFRAME;
    }
    if (!cell->batch_active) {
        cell->batch_active = 1;
        cell->batch_clock = event->observation.logical_clock;
        cell->batch_window_start = event->observation.window_start;
        cell->batch_window_end = event->observation.window_end;
        cell->batch_seal_time = event->observation.seal_time;
    } else if (event->observation.logical_clock != cell->batch_clock ||
               event->observation.window_start !=
                   cell->batch_window_start ||
               event->observation.window_end != cell->batch_window_end ||
               event->observation.seal_time != cell->batch_seal_time) {
        if (event->observation.logical_clock > cell->batch_clock) {
            return CDC_SCHEDULER_HOLD_INCOMPLETE;
        }
        *consumed = 1;
        return CDC_SCHEDULER_EFRAME;
    }

    if (cell->pending[member_index].present) {
        if (!observation_core_equal(
                &cell->pending[member_index].observation,
                &event->observation)) {
            *consumed = 1;
            return CDC_SCHEDULER_ECONFLICT;
        }
        duplicate = 1;
    }
    if (event->observation.has_witness) {
        if (cell->has_witness &&
            (cell->witness_target_clock !=
                       event->observation.logical_clock ||
             !witness_equal(&cell->witness,
                            &event->observation.witness))) {
            *consumed = 1;
            return CDC_SCHEDULER_ECONFLICT;
        }
    }
    if (!cell->pending[member_index].present) {
        cell->pending[member_index].present = 1;
        cell->pending[member_index].observation = event->observation;
        cell->pending_count++;
    }
    if (event->observation.has_witness && !cell->has_witness) {
        cell->has_witness = 1;
        cell->witness_target_clock = event->observation.logical_clock;
        cell->witness = event->observation.witness;
        duplicate = 0;
    }
    *consumed = 1;
    if (cell->pending_count == cell->member_count) {
        cdc_scheduler_status status =
            reduce_leaf(scheduler, cell_index, report);
        if (status != CDC_SCHEDULER_OK &&
            status != CDC_SCHEDULER_HOLD_INCOMPLETE) {
            return status;
        }
    }
    return duplicate ? CDC_SCHEDULER_HOLD_DUPLICATE : CDC_SCHEDULER_OK;
}

static cdc_scheduler_status
apply_witness_event(cdc_scheduler *scheduler, const scheduler_event *event,
                    cdc_scheduler_run_report *report, int *consumed) {
    size_t cell_index = find_cell(scheduler, event->cell_id);
    cell_entry *cell;
    cdc_scheduler_status status;
    *consumed = 0;
    if (cell_index == SIZE_MAX) {
        return CDC_SCHEDULER_ENOTFOUND;
    }
    cell = &scheduler->cells[cell_index];
    if (cell->has_state && event->logical_clock <= cell->state.logical_clock) {
        *consumed = 1;
        if (event->logical_clock == cell->state.logical_clock &&
            cell->has_committed_witness &&
            cell->committed_witness_target_clock == event->logical_clock &&
            witness_equal(&cell->committed_witness, &event->witness)) {
            return CDC_SCHEDULER_HOLD_DUPLICATE;
        }
        return CDC_SCHEDULER_ECONFLICT;
    }
    if (cell->kind == CDC_SCHEDULER_LEAF) {
        if (!cell->batch_active ||
            cell->batch_clock < event->logical_clock) {
            return CDC_SCHEDULER_HOLD_INCOMPLETE;
        }
        if (cell->batch_clock > event->logical_clock) {
            *consumed = 1;
            return CDC_SCHEDULER_EFRAME;
        }
    }
    if (cell->has_witness) {
        *consumed = 1;
        if (cell->witness_target_clock == event->logical_clock &&
            witness_equal(&cell->witness, &event->witness)) {
            return CDC_SCHEDULER_HOLD_DUPLICATE;
        }
        return CDC_SCHEDULER_ECONFLICT;
    }
    cell->has_witness = 1;
    cell->witness_target_clock = event->logical_clock;
    cell->witness = event->witness;
    *consumed = 1;
    report->witness_events++;
    if (cell->kind == CDC_SCHEDULER_LEAF) {
        if (cell->pending_count != cell->member_count) {
            return CDC_SCHEDULER_OK;
        }
        status = reduce_leaf(scheduler, cell_index, report);
    } else {
        status = reduce_composite(scheduler, cell_index, report);
    }
    if (status == CDC_SCHEDULER_HOLD_INCOMPLETE ||
        status == CDC_SCHEDULER_HOLD_DUPLICATE) {
        return CDC_SCHEDULER_OK;
    }
    return status;
}

static cdc_scheduler_status
apply_event(cdc_scheduler *scheduler, const scheduler_event *event,
            cdc_scheduler_run_report *report, int *consumed) {
    if (event->kind == SCHEDULER_EVENT_OBSERVATION) {
        return apply_observation_event(scheduler, event, report, consumed);
    }
    if (event->kind == SCHEDULER_EVENT_WITNESS) {
        return apply_witness_event(scheduler, event, report, consumed);
    }
    *consumed = 1;
    return CDC_SCHEDULER_EARG;
}

cdc_scheduler *cdc_scheduler_create(const cdc_scheduler_config *config) {
    cdc_scheduler *scheduler;
    if (!config || config->cell_limit == 0 || config->event_limit == 0 ||
        config->maximum_depth == 0) {
        return NULL;
    }
    scheduler = calloc(1, sizeof(*scheduler));
    if (!scheduler) {
        return NULL;
    }
    scheduler->cells = calloc(config->cell_limit, sizeof(*scheduler->cells));
    scheduler->events =
        calloc(config->event_limit, sizeof(*scheduler->events));
    if (!scheduler->cells || !scheduler->events ||
        pthread_mutex_init(&scheduler->mutex, NULL) != 0) {
        free(scheduler->cells);
        free(scheduler->events);
        free(scheduler);
        return NULL;
    }
    scheduler->mutex_ready = 1;
    scheduler->cell_limit = config->cell_limit;
    scheduler->event_limit = config->event_limit;
    scheduler->maximum_depth = config->maximum_depth;
    scheduler->next_sequence = 1;
    return scheduler;
}

void cdc_scheduler_destroy(cdc_scheduler *scheduler) {
    size_t i;
    if (!scheduler) {
        return;
    }
    for (i = 0; i < scheduler->cell_count; i++) {
        free(scheduler->cells[i].member_ids);
        free(scheduler->cells[i].successor_ids);
        free(scheduler->cells[i].children);
        free(scheduler->cells[i].pending);
        free(scheduler->cells[i].committed);
    }
    free(scheduler->cells);
    free(scheduler->events);
    if (scheduler->mutex_ready) {
        pthread_mutex_destroy(&scheduler->mutex);
    }
    free(scheduler);
}

cdc_scheduler_status
cdc_scheduler_add_cell(cdc_scheduler *scheduler,
                       const cdc_scheduler_cell_spec *spec) {
    uint64_t *members = NULL;
    uint64_t *successors = NULL;
    size_t *children = NULL;
    pending_slot *pending = NULL;
    pending_slot *committed = NULL;
    size_t depth = 0;
    size_t i;
    cell_entry *cell;
    cdc_scheduler_status status = CDC_SCHEDULER_OK;

    if (!scheduler || !spec ||
        bounded_length(spec->cell_id, CDC_CELL_ID_MAX) == 0 ||
        bounded_length(spec->cell_id, CDC_CELL_ID_MAX) > CDC_CELL_ID_MAX ||
        (spec->kind != CDC_SCHEDULER_LEAF &&
         spec->kind != CDC_SCHEDULER_COMPOSITE) ||
        spec->member_count > scheduler->event_limit ||
        !ring_valid(spec->member_ids, spec->successor_ids,
                    spec->member_count) ||
        spec->frame_version == 0 || spec->reducer_version == 0 ||
        spec->topology_version == 0 || spec->causal_horizon == 0 ||
        (spec->kind == CDC_SCHEDULER_COMPOSITE &&
         !spec->child_cell_ids)) {
        return CDC_SCHEDULER_EARG;
    }
    pthread_mutex_lock(&scheduler->mutex);
    if (scheduler->sealed) {
        status = CDC_SCHEDULER_ECONFLICT;
        goto done;
    }
    if (scheduler->cell_count >= scheduler->cell_limit) {
        status = CDC_SCHEDULER_HOLD_LIMIT;
        goto done;
    }
    if (find_cell(scheduler, spec->cell_id) != SIZE_MAX) {
        status = CDC_SCHEDULER_ECONFLICT;
        goto done;
    }
    members = malloc(spec->member_count * sizeof(*members));
    successors = malloc(spec->member_count * sizeof(*successors));
    children = spec->kind == CDC_SCHEDULER_COMPOSITE
                   ? malloc(spec->member_count * sizeof(*children))
                   : NULL;
    pending = spec->kind == CDC_SCHEDULER_LEAF
                  ? calloc(spec->member_count, sizeof(*pending))
                  : NULL;
    committed = spec->kind == CDC_SCHEDULER_LEAF
                    ? calloc(spec->member_count, sizeof(*committed))
                    : NULL;
    if (!members || !successors ||
        (spec->kind == CDC_SCHEDULER_COMPOSITE && !children) ||
        (spec->kind == CDC_SCHEDULER_LEAF &&
         (!pending || !committed))) {
        status = CDC_SCHEDULER_HOLD_LIMIT;
        goto done;
    }
    memcpy(members, spec->member_ids,
           spec->member_count * sizeof(*members));
    memcpy(successors, spec->successor_ids,
           spec->member_count * sizeof(*successors));
    if (spec->kind == CDC_SCHEDULER_COMPOSITE) {
        for (i = 0; i < spec->member_count; i++) {
            size_t child;
            size_t prior;
            if (!spec->child_cell_ids[i] ||
                bounded_length(spec->child_cell_ids[i], CDC_CELL_ID_MAX) ==
                    0 ||
                bounded_length(spec->child_cell_ids[i], CDC_CELL_ID_MAX) >
                    CDC_CELL_ID_MAX) {
                status = CDC_SCHEDULER_EARG;
                goto done;
            }
            child = find_cell(scheduler, spec->child_cell_ids[i]);
            if (child == SIZE_MAX) {
                status = CDC_SCHEDULER_ENOTFOUND;
                goto done;
            }
            if (scheduler->cells[child].parent != SIZE_MAX) {
                status = CDC_SCHEDULER_ECYCLE;
                goto done;
            }
            for (prior = 0; prior < i; prior++) {
                if (children[prior] == child) {
                    status = CDC_SCHEDULER_ECONFLICT;
                    goto done;
                }
            }
            children[i] = child;
            if (scheduler->cells[child].depth + 1 > depth) {
                depth = scheduler->cells[child].depth + 1;
            }
        }
        if (depth > scheduler->maximum_depth) {
            status = CDC_SCHEDULER_ECYCLE;
            goto done;
        }
    }

    cell = &scheduler->cells[scheduler->cell_count];
    memset(cell, 0, sizeof(*cell));
    memcpy(cell->cell_id, spec->cell_id, strlen(spec->cell_id) + 1);
    cell->kind = spec->kind;
    cell->member_ids = members;
    cell->successor_ids = successors;
    cell->children = children;
    cell->pending = pending;
    cell->committed = committed;
    cell->member_count = spec->member_count;
    cell->parent = SIZE_MAX;
    cell->depth = depth;
    cell->frame_version = spec->frame_version;
    cell->reducer_version = spec->reducer_version;
    cell->topology_version = spec->topology_version;
    cell->stale_after = spec->stale_after;
    cell->causal_horizon = spec->causal_horizon;
    members = NULL;
    successors = NULL;
    children = NULL;
    pending = NULL;
    committed = NULL;
    if (cell->kind == CDC_SCHEDULER_COMPOSITE) {
        for (i = 0; i < cell->member_count; i++) {
            scheduler->cells[cell->children[i]].parent =
                scheduler->cell_count;
        }
    }
    scheduler->cell_count++;

done:
    free(members);
    free(successors);
    free(children);
    free(pending);
    free(committed);
    pthread_mutex_unlock(&scheduler->mutex);
    return status;
}

cdc_scheduler_status
cdc_scheduler_import_previous_states(
    cdc_scheduler *scheduler,
    const cdc_scheduler_epoch_state *imports, size_t import_count,
    const uint8_t predecessor_configuration_digest[CDC_DIGEST_SIZE]) {
    size_t *indices = NULL;
    size_t i;
    cdc_scheduler_status status = CDC_SCHEDULER_OK;
    if (!scheduler || !imports || import_count == 0 ||
        !predecessor_configuration_digest ||
        digest_is_zero(predecessor_configuration_digest)) {
        return CDC_SCHEDULER_EARG;
    }
    indices = malloc(import_count * sizeof(*indices));
    if (!indices) {
        return CDC_SCHEDULER_HOLD_LIMIT;
    }
    pthread_mutex_lock(&scheduler->mutex);
    if (scheduler->sealed || scheduler->has_predecessor_configuration) {
        status = CDC_SCHEDULER_ECONFLICT;
        goto done;
    }
    if (import_count > scheduler->cell_count) {
        status = CDC_SCHEDULER_EARG;
        goto done;
    }
    for (i = 0; i < import_count; i++) {
        const cdc_scheduler_epoch_state *item = &imports[i];
        const cdc_cell_state *previous = &item->state;
        cell_entry *cell;
        size_t prior;
        int same_frame;
        if (digest_is_zero(item->source_configuration_digest)) {
            status = CDC_SCHEDULER_EARG;
            goto done;
        }
        if (memcmp(item->source_configuration_digest,
                   predecessor_configuration_digest,
                   CDC_DIGEST_SIZE) != 0) {
            status = CDC_SCHEDULER_EFRAME;
            goto done;
        }
        if (bounded_length(item->cell_id, CDC_CELL_ID_MAX) == 0 ||
            bounded_length(item->cell_id, CDC_CELL_ID_MAX) >
                CDC_CELL_ID_MAX ||
            (item->kind != CDC_SCHEDULER_LEAF &&
             item->kind != CDC_SCHEDULER_COMPOSITE) ||
            strcmp(previous->cell_id, item->cell_id) != 0 ||
            previous->generation == 0 || previous->frame_version == 0 ||
            previous->reducer_version == 0 ||
            previous->topology_version == 0 ||
            previous->logical_clock == 0 ||
            previous->causal_horizon < previous->logical_clock ||
            previous->member_count < 2 ||
            digest_is_zero(previous->state_digest) ||
            digest_is_zero(item->structure_digest) ||
            cdc_cell_state_validate(previous) != CDC_CELL_OK) {
            status = CDC_SCHEDULER_EARG;
            goto done;
        }
        indices[i] = find_cell(scheduler, item->cell_id);
        if (indices[i] == SIZE_MAX) {
            status = CDC_SCHEDULER_ENOTFOUND;
            goto done;
        }
        for (prior = 0; prior < i; prior++) {
            if (indices[prior] == indices[i]) {
                status = CDC_SCHEDULER_ECONFLICT;
                goto done;
            }
        }
        cell = &scheduler->cells[indices[i]];
        if (cell->kind != item->kind || cell->has_state ||
            cell->pending_count != 0 || cell->batch_active ||
            cell->has_witness ||
            cell->causal_horizon < previous->causal_horizon ||
            cell->causal_horizon <= previous->logical_clock) {
            status = CDC_SCHEDULER_EFRAME;
            goto done;
        }
        same_frame = cell->frame_version == previous->frame_version;
        if (same_frame) {
            uint8_t current_structure[CDC_DIGEST_SIZE];
            if (!compute_cell_structure_digest(
                    scheduler, indices[i], current_structure) ||
                memcmp(current_structure, item->structure_digest,
                       CDC_DIGEST_SIZE) != 0) {
                status = CDC_SCHEDULER_EFRAME;
                goto done;
            }
        } else if (
            previous->frame_version == UINT64_MAX ||
            cell->frame_version != previous->frame_version + 1 ||
            cell->reducer_version < previous->reducer_version ||
            cell->topology_version < previous->topology_version) {
            status = CDC_SCHEDULER_EFRAME;
            goto done;
        }
    }
    for (i = 0; i < import_count; i++) {
        cell_entry *cell = &scheduler->cells[indices[i]];
        cell->state = imports[i].state;
        cell->has_state = 1;
        cell->has_imported_state = 1;
        memcpy(cell->imported_structure_digest,
               imports[i].structure_digest, CDC_DIGEST_SIZE);
    }
    scheduler->has_predecessor_configuration = 1;
    memcpy(scheduler->predecessor_configuration_digest,
           predecessor_configuration_digest, CDC_DIGEST_SIZE);

done:
    pthread_mutex_unlock(&scheduler->mutex);
    free(indices);
    return status;
}

cdc_scheduler_status
cdc_scheduler_seal(cdc_scheduler *scheduler,
                   uint8_t configuration_digest[CDC_DIGEST_SIZE]) {
    cdc_scheduler_status status = CDC_SCHEDULER_OK;
    if (!scheduler || !configuration_digest) {
        return CDC_SCHEDULER_EARG;
    }
    pthread_mutex_lock(&scheduler->mutex);
    if (scheduler->cell_count == 0) {
        status = CDC_SCHEDULER_EARG;
    } else if (!scheduler->sealed) {
        if (!compute_configuration_digest(scheduler)) {
            status = CDC_SCHEDULER_HOLD_LIMIT;
        } else {
            scheduler->sealed = 1;
        }
    }
    if (status == CDC_SCHEDULER_OK) {
        memcpy(configuration_digest, scheduler->configuration_digest,
               CDC_DIGEST_SIZE);
    }
    pthread_mutex_unlock(&scheduler->mutex);
    return status;
}

cdc_scheduler_status
cdc_scheduler_publish(cdc_scheduler *scheduler,
                      const cdc_scheduler_observation *observation,
                      uint64_t now) {
    size_t index;
    size_t member_index;
    size_t event_index;
    cell_entry *cell;
    cdc_scheduler_status status = CDC_SCHEDULER_OK;
    if (!scheduler || !observation ||
        bounded_length(observation->cell_id, CDC_CELL_ID_MAX) == 0 ||
        bounded_length(observation->cell_id, CDC_CELL_ID_MAX) >
            CDC_CELL_ID_MAX ||
        observation->member_id == 0 || !isfinite(observation->phase) ||
        observation->logical_clock == 0 ||
        observation->window_start > observation->window_end ||
        observation->observed_at < observation->window_start ||
        observation->observed_at > observation->window_end ||
        observation->seal_time < observation->window_end ||
        now < observation->seal_time ||
        digest_is_zero(observation->source_digest) ||
        (observation->has_witness != 0 &&
         observation->has_witness != 1) ||
        (observation->has_witness &&
         ((observation->witness.kind !=
               CDC_TOPOLOGY_EVENT_LOCAL_DEFORMATION &&
           observation->witness.kind !=
               CDC_TOPOLOGY_EVENT_PHASE_SLIP &&
           observation->witness.kind !=
               CDC_TOPOLOGY_EVENT_FRAME_CHANGE) ||
          observation->witness.logical_clock == 0 ||
          observation->witness.logical_clock >
              observation->logical_clock ||
          observation->witness.from_member == 0 ||
          observation->witness.to_member == 0 ||
          digest_is_zero(observation->witness.evidence_digest)))) {
        return CDC_SCHEDULER_EARG;
    }
    pthread_mutex_lock(&scheduler->mutex);
    if (!scheduler->sealed) {
        status = CDC_SCHEDULER_ECONFLICT;
        goto done;
    }
    index = find_cell(scheduler, observation->cell_id);
    if (index == SIZE_MAX) {
        status = CDC_SCHEDULER_ENOTFOUND;
        goto done;
    }
    cell = &scheduler->cells[index];
    member_index = find_member(cell, observation->member_id);
    if (cell->kind != CDC_SCHEDULER_LEAF ||
        member_index == SIZE_MAX) {
        status = CDC_SCHEDULER_ENOTFOUND;
        goto done;
    }
    if (observation->frame_version != cell->frame_version) {
        status = CDC_SCHEDULER_EFRAME;
        goto done;
    }
    if (observation->has_witness &&
        observation->witness.kind == CDC_TOPOLOGY_EVENT_FRAME_CHANGE &&
        (!cell->has_imported_state ||
         cell->frame_version != cell->state.frame_version + 1)) {
        status = CDC_SCHEDULER_ETRANSITION;
        goto done;
    }
    if (observation->logical_clock > cell->causal_horizon) {
        status = CDC_SCHEDULER_EFRAME;
        goto done;
    }
    if (now - observation->seal_time > cell->stale_after) {
        status = CDC_SCHEDULER_HOLD_STALE;
        goto done;
    }
    if (observation->seal_time - observation->observed_at >
        cell->stale_after) {
        status = CDC_SCHEDULER_HOLD_STALE;
        goto done;
    }
    if (cell->has_state &&
        observation->logical_clock <= cell->state.logical_clock) {
        if (observation->logical_clock == cell->state.logical_clock &&
            cell->committed[member_index].present &&
            observation_core_equal(
                &cell->committed[member_index].observation,
                observation) &&
            (!observation->has_witness ||
             (cell->has_committed_witness &&
              cell->committed_witness_target_clock ==
                  observation->logical_clock &&
              witness_equal(&cell->committed_witness,
                            &observation->witness)))) {
            status = CDC_SCHEDULER_HOLD_DUPLICATE;
        } else {
            status = CDC_SCHEDULER_EFRAME;
        }
        goto done;
    }
    if (cell->batch_active &&
        observation->logical_clock == cell->batch_clock &&
        (observation->window_start != cell->batch_window_start ||
         observation->window_end != cell->batch_window_end ||
         observation->seal_time != cell->batch_seal_time)) {
        status = CDC_SCHEDULER_EFRAME;
        goto done;
    }
    if (cell->pending[member_index].present &&
        cell->batch_clock == observation->logical_clock) {
        if (!observation_core_equal(
                &cell->pending[member_index].observation, observation)) {
            status = CDC_SCHEDULER_ECONFLICT;
            goto done;
        }
        if (!observation->has_witness) {
            status = CDC_SCHEDULER_HOLD_DUPLICATE;
            goto done;
        }
    }
    if (observation->has_witness && cell->has_witness &&
        (cell->witness_target_clock != observation->logical_clock ||
         !witness_equal(&cell->witness, &observation->witness))) {
        status = CDC_SCHEDULER_ECONFLICT;
        goto done;
    }
    for (event_index = 0; event_index < scheduler->event_count;
         event_index++) {
        scheduler_event *queued = &scheduler->events[event_index];
        if (queued->kind != SCHEDULER_EVENT_OBSERVATION ||
            strcmp(queued->observation.cell_id, observation->cell_id) != 0 ||
            queued->observation.logical_clock !=
                observation->logical_clock) {
            continue;
        }
        if (queued->observation.window_start !=
                observation->window_start ||
            queued->observation.window_end != observation->window_end ||
            queued->observation.seal_time != observation->seal_time ||
            queued->observation.frame_version !=
                observation->frame_version) {
            status = CDC_SCHEDULER_EFRAME;
            goto done;
        }
        if (observation->has_witness &&
            queued->observation.has_witness &&
            !witness_equal(&queued->observation.witness,
                           &observation->witness)) {
            status = CDC_SCHEDULER_ECONFLICT;
            goto done;
        }
        if (queued->observation.member_id != observation->member_id) {
            continue;
        }
        if (!observation_core_equal(&queued->observation, observation)) {
            status = CDC_SCHEDULER_ECONFLICT;
            goto done;
        }
        if (!queued->observation.has_witness &&
            observation->has_witness) {
            queued->observation.has_witness = 1;
            queued->observation.witness = observation->witness;
            goto done;
        }
        if (queued->observation.has_witness &&
            observation->has_witness &&
            !witness_equal(&queued->observation.witness,
                           &observation->witness)) {
            status = CDC_SCHEDULER_ECONFLICT;
        } else {
            status = CDC_SCHEDULER_HOLD_DUPLICATE;
        }
        goto done;
    }
    if (scheduler->event_count >= scheduler->event_limit ||
        scheduler->next_sequence == UINT64_MAX) {
        status = CDC_SCHEDULER_HOLD_LIMIT;
        goto done;
    }
    scheduler->events[scheduler->event_count].kind =
        SCHEDULER_EVENT_OBSERVATION;
    scheduler->events[scheduler->event_count].observation = *observation;
    scheduler->events[scheduler->event_count].sequence =
        scheduler->next_sequence++;
    scheduler->event_count++;

done:
    pthread_mutex_unlock(&scheduler->mutex);
    return status;
}

cdc_scheduler_status
cdc_scheduler_publish_witness(cdc_scheduler *scheduler, const char *cell_id,
                              uint64_t target_logical_clock,
                              const cdc_topology_witness *witness) {
    size_t index;
    size_t event_index;
    cell_entry *cell;
    cdc_scheduler_status status = CDC_SCHEDULER_OK;
    if (!scheduler || !cell_id || !witness ||
        bounded_length(cell_id, CDC_CELL_ID_MAX) == 0 ||
        bounded_length(cell_id, CDC_CELL_ID_MAX) > CDC_CELL_ID_MAX ||
        target_logical_clock == 0 ||
        (witness->kind != CDC_TOPOLOGY_EVENT_LOCAL_DEFORMATION &&
         witness->kind != CDC_TOPOLOGY_EVENT_PHASE_SLIP &&
         witness->kind != CDC_TOPOLOGY_EVENT_FRAME_CHANGE) ||
        witness->logical_clock == 0 ||
        witness->logical_clock > target_logical_clock ||
        witness->from_member == 0 || witness->to_member == 0 ||
        digest_is_zero(witness->evidence_digest)) {
        return CDC_SCHEDULER_EARG;
    }
    pthread_mutex_lock(&scheduler->mutex);
    if (!scheduler->sealed) {
        status = CDC_SCHEDULER_ECONFLICT;
        goto done;
    }
    index = find_cell(scheduler, cell_id);
    if (index == SIZE_MAX) {
        status = CDC_SCHEDULER_ENOTFOUND;
        goto done;
    }
    cell = &scheduler->cells[index];
    if (witness->kind == CDC_TOPOLOGY_EVENT_FRAME_CHANGE &&
        (!cell->has_imported_state ||
         cell->frame_version != cell->state.frame_version + 1)) {
        status = CDC_SCHEDULER_ETRANSITION;
        goto done;
    }
    if (target_logical_clock > cell->causal_horizon) {
        status = CDC_SCHEDULER_EFRAME;
        goto done;
    }
    if (cell->has_state &&
        target_logical_clock <= cell->state.logical_clock) {
        if (target_logical_clock == cell->state.logical_clock &&
            cell->has_committed_witness &&
            cell->committed_witness_target_clock ==
                target_logical_clock &&
            witness_equal(&cell->committed_witness, witness)) {
            status = CDC_SCHEDULER_HOLD_DUPLICATE;
        } else {
            status = CDC_SCHEDULER_ECONFLICT;
        }
        goto done;
    }
    if (cell->has_witness) {
        if (cell->witness_target_clock == target_logical_clock &&
            witness_equal(&cell->witness, witness)) {
            status = CDC_SCHEDULER_HOLD_DUPLICATE;
        } else {
            status = CDC_SCHEDULER_ECONFLICT;
        }
        goto done;
    }
    for (event_index = 0; event_index < scheduler->event_count;
         event_index++) {
        scheduler_event *queued = &scheduler->events[event_index];
        if (queued->kind == SCHEDULER_EVENT_OBSERVATION &&
            strcmp(queued->observation.cell_id, cell_id) == 0 &&
            queued->observation.logical_clock == target_logical_clock &&
            queued->observation.has_witness) {
            status = witness_equal(&queued->observation.witness, witness)
                         ? CDC_SCHEDULER_HOLD_DUPLICATE
                         : CDC_SCHEDULER_ECONFLICT;
            goto done;
        }
        if (queued->kind != SCHEDULER_EVENT_WITNESS ||
            strcmp(queued->cell_id, cell_id) != 0 ||
            queued->logical_clock != target_logical_clock) {
            continue;
        }
        status = witness_equal(&queued->witness, witness)
                     ? CDC_SCHEDULER_HOLD_DUPLICATE
                     : CDC_SCHEDULER_ECONFLICT;
        goto done;
    }
    if (scheduler->event_count >= scheduler->event_limit ||
        scheduler->next_sequence == UINT64_MAX) {
        status = CDC_SCHEDULER_HOLD_LIMIT;
        goto done;
    }
    scheduler->events[scheduler->event_count].kind =
        SCHEDULER_EVENT_WITNESS;
    memcpy(scheduler->events[scheduler->event_count].cell_id, cell_id,
           strlen(cell_id) + 1);
    scheduler->events[scheduler->event_count].logical_clock =
        target_logical_clock;
    scheduler->events[scheduler->event_count].witness = *witness;
    scheduler->events[scheduler->event_count].sequence =
        scheduler->next_sequence++;
    scheduler->event_count++;

done:
    pthread_mutex_unlock(&scheduler->mutex);
    return status;
}

static cdc_scheduler_status
retry_ready_cells(cdc_scheduler *scheduler,
                  cdc_scheduler_run_report *report) {
    const char *after = NULL;
    size_t visited;
    for (visited = 0; visited < scheduler->cell_count; visited++) {
        size_t i;
        size_t selected = SIZE_MAX;
        cdc_scheduler_status status;
        cell_entry *cell;
        for (i = 0; i < scheduler->cell_count; i++) {
            if ((after &&
                 strcmp(scheduler->cells[i].cell_id, after) <= 0) ||
                (selected != SIZE_MAX &&
                 strcmp(scheduler->cells[i].cell_id,
                        scheduler->cells[selected].cell_id) >= 0)) {
                continue;
            }
            selected = i;
        }
        if (selected == SIZE_MAX) {
            return CDC_SCHEDULER_ECONFLICT;
        }
        cell = &scheduler->cells[selected];
        after = cell->cell_id;
        if (cell->kind == CDC_SCHEDULER_LEAF) {
            if (cell->pending_count != cell->member_count) {
                continue;
            }
            status = reduce_leaf(scheduler, selected, report);
        } else {
            status = reduce_composite(scheduler, selected, report);
        }
        if (status == CDC_SCHEDULER_HOLD_LIMIT) {
            return status;
        }
        if (status >= CDC_SCHEDULER_EARG) {
            return status;
        }
    }
    return CDC_SCHEDULER_OK;
}

cdc_scheduler_status
cdc_scheduler_drain(cdc_scheduler *scheduler, size_t maximum_events,
                    cdc_scheduler_run_report *report) {
    cdc_scheduler_status status = CDC_SCHEDULER_OK;
    if (!scheduler || !report || maximum_events == 0) {
        return CDC_SCHEDULER_EARG;
    }
    pthread_mutex_lock(&scheduler->mutex);
    memset(report, 0, sizeof(*report));
    if (!scheduler->sealed) {
        pthread_mutex_unlock(&scheduler->mutex);
        return CDC_SCHEDULER_ECONFLICT;
    }
    status = retry_ready_cells(scheduler, report);
    if (status != CDC_SCHEDULER_OK) {
        goto finish;
    }
    while (scheduler->event_count > 0 &&
           report->events_processed < maximum_events) {
        size_t index = minimum_event(scheduler);
        int consumed = 0;
        status = apply_event(scheduler, &scheduler->events[index], report,
                             &consumed);
        if (consumed) {
            remove_event(scheduler, index);
            report->events_processed++;
            if (status == CDC_SCHEDULER_HOLD_DUPLICATE) {
                report->duplicate_events++;
                status = CDC_SCHEDULER_OK;
            } else if (status >= CDC_SCHEDULER_EARG) {
                report->rejected_events++;
            }
        }
        if (status != CDC_SCHEDULER_OK) {
            break;
        }
    }
    if (status == CDC_SCHEDULER_OK) {
        status = retry_ready_cells(scheduler, report);
    }
finish:
    report->queued_remaining = scheduler->event_count;
    memcpy(report->configuration_digest, scheduler->configuration_digest,
           CDC_DIGEST_SIZE);
    memcpy(report->execution_digest, scheduler->execution_digest,
           CDC_DIGEST_SIZE);
    pthread_mutex_unlock(&scheduler->mutex);
    return status;
}

cdc_scheduler_status
cdc_scheduler_get_state(cdc_scheduler *scheduler, const char *cell_id,
                        cdc_cell_state *out) {
    size_t index;
    cdc_scheduler_status status = CDC_SCHEDULER_OK;
    if (!scheduler || !cell_id || !out) {
        return CDC_SCHEDULER_EARG;
    }
    pthread_mutex_lock(&scheduler->mutex);
    index = find_cell(scheduler, cell_id);
    if (index == SIZE_MAX) {
        status = CDC_SCHEDULER_ENOTFOUND;
    } else if (!scheduler->cells[index].has_state) {
        status = CDC_SCHEDULER_HOLD_INCOMPLETE;
    } else {
        *out = scheduler->cells[index].state;
    }
    pthread_mutex_unlock(&scheduler->mutex);
    return status;
}

cdc_scheduler_status
cdc_scheduler_get_epoch_state(cdc_scheduler *scheduler, const char *cell_id,
                              cdc_scheduler_epoch_state *out) {
    size_t index;
    cdc_scheduler_status status = CDC_SCHEDULER_OK;
    if (!scheduler || !cell_id || !out) {
        return CDC_SCHEDULER_EARG;
    }
    pthread_mutex_lock(&scheduler->mutex);
    index = find_cell(scheduler, cell_id);
    if (index == SIZE_MAX) {
        status = CDC_SCHEDULER_ENOTFOUND;
    } else if (!scheduler->sealed ||
               !scheduler->cells[index].has_state) {
        status = CDC_SCHEDULER_HOLD_INCOMPLETE;
    } else {
        memset(out, 0, sizeof(*out));
        memcpy(out->cell_id, scheduler->cells[index].cell_id,
               strlen(scheduler->cells[index].cell_id) + 1);
        out->kind = scheduler->cells[index].kind;
        out->state = scheduler->cells[index].state;
        if (!compute_cell_structure_digest(
                scheduler, index, out->structure_digest)) {
            status = CDC_SCHEDULER_HOLD_LIMIT;
        } else {
            memcpy(out->source_configuration_digest,
                   scheduler->configuration_digest,
                   CDC_DIGEST_SIZE);
        }
    }
    pthread_mutex_unlock(&scheduler->mutex);
    return status;
}

size_t cdc_scheduler_cell_count(cdc_scheduler *scheduler) {
    size_t count = 0;
    if (!scheduler) {
        return 0;
    }
    pthread_mutex_lock(&scheduler->mutex);
    count = scheduler->cell_count;
    pthread_mutex_unlock(&scheduler->mutex);
    return count;
}

size_t cdc_scheduler_queued_count(cdc_scheduler *scheduler) {
    size_t count = 0;
    if (!scheduler) {
        return 0;
    }
    pthread_mutex_lock(&scheduler->mutex);
    count = scheduler->event_count;
    pthread_mutex_unlock(&scheduler->mutex);
    return count;
}

int cdc_scheduler_is_pristine(cdc_scheduler *scheduler) {
    int pristine = 1;
    size_t i;
    if (!scheduler) {
        return 0;
    }
    pthread_mutex_lock(&scheduler->mutex);
    if (!scheduler->sealed || scheduler->event_count != 0 ||
        !digest_is_zero(scheduler->execution_digest)) {
        pristine = 0;
    }
    for (i = 0; pristine && i < scheduler->cell_count; i++) {
        const cell_entry *cell = &scheduler->cells[i];
        if ((cell->has_state && !cell->has_imported_state) ||
            cell->pending_count != 0 ||
            cell->batch_active || cell->has_witness) {
            pristine = 0;
        }
    }
    pthread_mutex_unlock(&scheduler->mutex);
    return pristine;
}

const char *cdc_scheduler_status_name(cdc_scheduler_status status) {
    static const char *const NAMES[] = {
        "ok",          "hold-incomplete", "hold-stale", "hold-limit",
        "hold-duplicate", "argument",       "not-found",  "conflict",
        "cycle",       "frame",           "transition",
    };
    if ((size_t)status >= sizeof(NAMES) / sizeof(NAMES[0])) {
        return "unknown";
    }
    return NAMES[status];
}
