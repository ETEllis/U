#ifndef CDC_SCHEDULER_JOURNAL_H
#define CDC_SCHEDULER_JOURNAL_H

#include "cdc_scheduler_wire.h"
#include "cdc_store.h"
#include "cdc_supervisor.h"

typedef enum {
    CDC_SCHEDULER_JOURNAL_OK = 0,
    CDC_SCHEDULER_JOURNAL_HOLD_LIMIT,
    CDC_SCHEDULER_JOURNAL_HOLD_INCOMPLETE,
    CDC_SCHEDULER_JOURNAL_EARG,
    CDC_SCHEDULER_JOURNAL_ESTORE,
    CDC_SCHEDULER_JOURNAL_EWIRE,
    CDC_SCHEDULER_JOURNAL_EAUTH,
    CDC_SCHEDULER_JOURNAL_EPOLICY,
    CDC_SCHEDULER_JOURNAL_ESCHEDULER,
    CDC_SCHEDULER_JOURNAL_EUNSUPPORTED
} cdc_scheduler_journal_status;

typedef enum {
    CDC_SCHEDULER_JOURNAL_OUTCOME_NONE = 0,
    CDC_SCHEDULER_JOURNAL_OUTCOME_ACCEPT = 1,
    CDC_SCHEDULER_JOURNAL_OUTCOME_REJECT = 2
} cdc_scheduler_journal_outcome;

typedef struct {
    cdc_supervisor_receipt supervisor;
    cdc_scheduler_wire_status wire_status;
    cdc_store_status store_status;
    cdc_scheduler_payload_kind payload_kind;
    cdc_scheduler_journal_outcome outcome;
    int application_invoked;
    int durable;
} cdc_scheduler_journal_admission_receipt;

typedef struct {
    uint32_t schema_version;
    const char *local_id;
    const char *key_id;
    size_t stream_limit;
} cdc_scheduler_journal_replay_config;

typedef struct {
    size_t records_verified;
    size_t events_recovered;
    size_t terminal_rejections;
    size_t duplicate_events;
    size_t leaf_updates;
    size_t recursive_updates;
    size_t frames_held;
    size_t queued_remaining;
    cdc_store_status store_status;
    cdc_transport_verdict transport_verdict;
    cdc_scheduler_wire_status wire_status;
    cdc_scheduler_status scheduler_status;
    uint8_t configuration_digest[CDC_DIGEST_SIZE];
    uint8_t execution_digest[CDC_DIGEST_SIZE];
} cdc_scheduler_journal_replay_report;

/*
 * Durable ingress. The callback validates the canonical command and its
 * envelope scope, then seals the complete authenticated envelope and its
 * application ACCEPT/REJECT outcome as one store transaction. Terminal
 * application rejections are durable because they consume a causal position.
 * The store is owned exclusively by this journal while an admission is active;
 * callers must not interleave direct stage/commit calls on the same handle.
 * Supervisor serialization makes concurrent journal admissions safe. A store
 * error is retryable and does not consume supervisor authority/causal state.
 * At-least-once append after an ambiguous crash is safe because transport and
 * scheduler replay are idempotent for identical commands.
 */
cdc_supervisor_verdict
cdc_scheduler_journal_admit(
    cdc_supervisor *supervisor, cdc_store *store,
    const cdc_transport_envelope *envelope, uint64_t now,
    uint32_t verified_approvals,
    cdc_scheduler_journal_admission_receipt *receipt);

/*
 * Reconstructs a FRESH, already-sealed scheduler from the complete exact
 * event history. The store is fully verified and every envelope is checked
 * against the trusted replay peer configuration, complete causal stream, and
 * authenticated application outcome before the scheduler is mutated.
 * Compacted history is refused because a commitment is not an executable event
 * stream.
 */
cdc_scheduler_journal_status
cdc_scheduler_journal_replay(
    cdc_store *store, cdc_scheduler *fresh_scheduler,
    const uint8_t key[CDC_TRANSPORT_TAG_SIZE],
    const cdc_scheduler_journal_replay_config *config,
    size_t maximum_events,
    cdc_scheduler_journal_replay_report *report);

const char *cdc_scheduler_journal_status_name(
    cdc_scheduler_journal_status status);

#endif
