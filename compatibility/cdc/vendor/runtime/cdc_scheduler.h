#ifndef CDC_SCHEDULER_H
#define CDC_SCHEDULER_H

#include <stddef.h>
#include <stdint.h>

#include "cdc_cell.h"

typedef struct cdc_scheduler cdc_scheduler;

typedef enum {
    CDC_SCHEDULER_OK = 0,
    CDC_SCHEDULER_HOLD_INCOMPLETE,
    CDC_SCHEDULER_HOLD_STALE,
    CDC_SCHEDULER_HOLD_LIMIT,
    CDC_SCHEDULER_HOLD_DUPLICATE,
    CDC_SCHEDULER_EARG,
    CDC_SCHEDULER_ENOTFOUND,
    CDC_SCHEDULER_ECONFLICT,
    CDC_SCHEDULER_ECYCLE,
    CDC_SCHEDULER_EFRAME,
    CDC_SCHEDULER_ETRANSITION
} cdc_scheduler_status;

typedef enum {
    CDC_SCHEDULER_LEAF = 1,
    CDC_SCHEDULER_COMPOSITE = 2
} cdc_scheduler_cell_kind;

typedef struct {
    size_t cell_limit;
    size_t event_limit;
    size_t maximum_depth;
} cdc_scheduler_config;

typedef struct {
    const char *cell_id;
    cdc_scheduler_cell_kind kind;
    const uint64_t *member_ids;
    const uint64_t *successor_ids;
    const char *const *child_cell_ids; /* required only for COMPOSITE */
    size_t member_count;
    uint64_t frame_version;
    uint64_t reducer_version;
    uint64_t topology_version;
    uint64_t stale_after;
    uint64_t causal_horizon;
} cdc_scheduler_cell_spec;

typedef struct {
    char cell_id[CDC_CELL_ID_MAX + 1];
    cdc_scheduler_cell_kind kind;
    cdc_cell_state state;
    uint8_t structure_digest[CDC_DIGEST_SIZE];
    /*
     * Configuration digest of the sealed scheduler that exported this
     * receipt. Import validates every item against the declared predecessor;
     * callers cannot splice locally valid states from different epochs.
     */
    uint8_t source_configuration_digest[CDC_DIGEST_SIZE];
} cdc_scheduler_epoch_state;

typedef struct {
    char cell_id[CDC_CELL_ID_MAX + 1];
    uint64_t member_id;
    double phase;
    uint64_t observed_at;
    uint64_t logical_clock;
    uint64_t frame_version;
    uint64_t window_start;
    uint64_t window_end;
    uint64_t seal_time;
    uint8_t source_digest[CDC_DIGEST_SIZE];
    int has_witness;
    cdc_topology_witness witness;
} cdc_scheduler_observation;

typedef struct {
    size_t events_processed;
    size_t duplicate_events;
    size_t rejected_events;
    size_t witness_events;
    size_t leaf_updates;
    size_t recursive_updates;
    size_t frames_held;
    size_t queued_remaining;
    uint8_t configuration_digest[CDC_DIGEST_SIZE];
    uint8_t execution_digest[CDC_DIGEST_SIZE];
} cdc_scheduler_run_report;

cdc_scheduler *cdc_scheduler_create(const cdc_scheduler_config *config);
/* The caller must quiesce concurrent users before destroy. */
void cdc_scheduler_destroy(cdc_scheduler *scheduler);

/*
 * Cells are added bottom-up. A composite names already-added children and
 * becomes their sole parent. This produces a bounded forest and makes cycles
 * unrepresentable through the public construction path.
 */
cdc_scheduler_status
cdc_scheduler_add_cell(cdc_scheduler *scheduler,
                       const cdc_scheduler_cell_spec *spec);

/*
 * Atomically seeds cells in a new, still-unsealed scheduler epoch from one
 * coherent predecessor configuration. Unchanged cells may carry forward only
 * when kind, frame/reducer/topology versions, member count, and oriented ring
 * are unchanged. Changed cells must advance exactly one frame version and may
 * advance (never regress) reducer/topology versions. Every receipt must bind
 * the same source configuration digest supplied for the batch. That one
 * predecessor digest and every imported state digest are bound into the new
 * configuration digest. Changed cells still require FRAME_CHANGE evidence.
 */
cdc_scheduler_status
cdc_scheduler_import_previous_states(
    cdc_scheduler *scheduler,
    const cdc_scheduler_epoch_state *imports, size_t import_count,
    const uint8_t predecessor_configuration_digest[CDC_DIGEST_SIZE]);

/*
 * Freezes the cell forest and returns its canonical digest. Cell publication
 * is refused before sealing, and graph mutation is refused afterward.
 */
cdc_scheduler_status
cdc_scheduler_seal(cdc_scheduler *scheduler,
                   uint8_t configuration_digest[CDC_DIGEST_SIZE]);

/* `now` is local admission time. It gates staleness but is not placed in the
 * replay identity; the authenticated observation carries its deterministic
 * seal_time. */
cdc_scheduler_status
cdc_scheduler_publish(cdc_scheduler *scheduler,
                      const cdc_scheduler_observation *observation,
                      uint64_t now);

/*
 * Supplies evidence for a transition that could not commit without it.
 * The witness is sequenced at `target_logical_clock` after observations at
 * the same clock, so a held leaf or composite transition can be retried
 * deterministically. FRAME_CHANGE is accepted only for a cell seeded from the
 * immediately preceding scheduler epoch. Evidence submitted after a state
 * already committed is terminally late.
 */
cdc_scheduler_status
cdc_scheduler_publish_witness(cdc_scheduler *scheduler, const char *cell_id,
                              uint64_t target_logical_clock,
                              const cdc_topology_witness *witness);

/*
 * Processes the canonical minimum event key:
 * (logical_clock, event_kind, cell_id, member_id, local_sequence).
 * Observation events precede transition witnesses at the same clock. Parent
 * cells reduce only when every child has the same strictly newer clock.
 */
cdc_scheduler_status
cdc_scheduler_drain(cdc_scheduler *scheduler, size_t maximum_events,
                    cdc_scheduler_run_report *report);

cdc_scheduler_status
cdc_scheduler_get_state(cdc_scheduler *scheduler, const char *cell_id,
                        cdc_cell_state *out);

/*
 * Exports the committed state together with the exact canonical cell-spec
 * digest used by the sealed scheduler configuration. For composites this
 * binds each oriented member position to its child cell identifier.
 */
cdc_scheduler_status
cdc_scheduler_get_epoch_state(cdc_scheduler *scheduler, const char *cell_id,
                              cdc_scheduler_epoch_state *out);

size_t cdc_scheduler_cell_count(cdc_scheduler *scheduler);
size_t cdc_scheduler_queued_count(cdc_scheduler *scheduler);
int cdc_scheduler_is_pristine(cdc_scheduler *scheduler);

const char *cdc_scheduler_status_name(cdc_scheduler_status status);

#endif
