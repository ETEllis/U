#include "cdc_scheduler_journal.h"

#include <stdlib.h>
#include <string.h>

enum {
    JOURNAL_RECORD_VERSION = 1,
    JOURNAL_RECORD_HEADER_SIZE = 16
};

static const uint8_t JOURNAL_RECORD_MAGIC[4] = {'C', 'D', 'J', 'R'};

typedef struct {
    cdc_store *store;
    cdc_scheduler_journal_admission_receipt *receipt;
} journal_apply_context;

typedef struct {
    cdc_scheduler_payload payload;
    uint64_t event_sequence;
} replay_event;

typedef struct {
    replay_event *events;
    size_t capacity;
    size_t count;
    size_t records_verified;
    size_t terminal_rejections;
    const uint8_t *key;
    cdc_transport_peer peer;
    cdc_transport_verdict transport_verdict;
    cdc_scheduler_wire_status wire_status;
} replay_collector;

static void put_u64(uint8_t *out, uint64_t value) {
    size_t i;
    for (i = 0; i < 8; i++) {
        out[7 - i] = (uint8_t)(value >> (i * 8));
    }
}

static uint64_t get_u64(const uint8_t *in) {
    uint64_t value = 0;
    size_t i;
    for (i = 0; i < 8; i++) {
        value = (value << 8) | in[i];
    }
    return value;
}

static int encode_journal_record(
    cdc_scheduler_journal_outcome outcome,
    const cdc_transport_envelope *envelope,
    uint8_t **out, size_t *out_length) {
    uint8_t *envelope_wire = NULL;
    size_t envelope_length = 0;
    uint8_t *record;
    size_t record_length;
    if (!out || !out_length ||
        (outcome != CDC_SCHEDULER_JOURNAL_OUTCOME_ACCEPT &&
         outcome != CDC_SCHEDULER_JOURNAL_OUTCOME_REJECT) ||
        !cdc_transport_envelope_encode(envelope, &envelope_wire,
                                       &envelope_length) ||
        envelope_length > SIZE_MAX - JOURNAL_RECORD_HEADER_SIZE) {
        free(envelope_wire);
        return 0;
    }
    record_length = JOURNAL_RECORD_HEADER_SIZE + envelope_length;
    record = malloc(record_length);
    if (!record) {
        free(envelope_wire);
        return 0;
    }
    memcpy(record, JOURNAL_RECORD_MAGIC, sizeof(JOURNAL_RECORD_MAGIC));
    record[4] = JOURNAL_RECORD_VERSION;
    record[5] = (uint8_t)outcome;
    record[6] = 0;
    record[7] = 0;
    put_u64(record + 8, (uint64_t)envelope_length);
    memcpy(record + JOURNAL_RECORD_HEADER_SIZE, envelope_wire,
           envelope_length);
    free(envelope_wire);
    *out = record;
    *out_length = record_length;
    return 1;
}

static int decode_journal_record(
    const uint8_t *record, size_t record_length,
    cdc_scheduler_journal_outcome *outcome,
    const uint8_t **envelope_wire, size_t *envelope_length) {
    uint64_t encoded_length;
    if (!record || !outcome || !envelope_wire || !envelope_length ||
        record_length < JOURNAL_RECORD_HEADER_SIZE ||
        memcmp(record, JOURNAL_RECORD_MAGIC,
               sizeof(JOURNAL_RECORD_MAGIC)) != 0 ||
        record[4] != JOURNAL_RECORD_VERSION ||
        (record[5] != CDC_SCHEDULER_JOURNAL_OUTCOME_ACCEPT &&
         record[5] != CDC_SCHEDULER_JOURNAL_OUTCOME_REJECT) ||
        record[6] != 0 || record[7] != 0) {
        return 0;
    }
    encoded_length = get_u64(record + 8);
    if (encoded_length > SIZE_MAX ||
        (size_t)encoded_length !=
            record_length - JOURNAL_RECORD_HEADER_SIZE) {
        return 0;
    }
    *outcome = (cdc_scheduler_journal_outcome)record[5];
    *envelope_wire = record + JOURNAL_RECORD_HEADER_SIZE;
    *envelope_length = (size_t)encoded_length;
    return 1;
}

static int envelope_matches_payload(
    const cdc_transport_envelope *envelope,
    const cdc_scheduler_payload *payload) {
    if (strcmp(payload->cell_id, envelope->frame) != 0) {
        return 0;
    }
    if (payload->kind == CDC_SCHEDULER_PAYLOAD_OBSERVATION) {
        return envelope->message_type == CDC_TRANSPORT_OBSERVATION &&
               envelope->authority_action == CDC_AUTH_OBSERVE &&
               payload->observation.logical_clock <= envelope->horizon;
    }
    if (payload->kind == CDC_SCHEDULER_PAYLOAD_WITNESS) {
        return envelope->message_type == CDC_TRANSPORT_DECISION &&
               envelope->authority_action == CDC_AUTH_COMMIT &&
               payload->target_logical_clock <= envelope->horizon;
    }
    return 0;
}

static cdc_supervisor_application_verdict
append_scheduler_payload(const cdc_transport_envelope *envelope,
                         const cdc_supervisor_receipt *supervisor_receipt,
                         void *opaque) {
    journal_apply_context *context = opaque;
    cdc_scheduler_journal_admission_receipt *receipt = context->receipt;
    cdc_scheduler_payload payload;
    uint8_t *wire = NULL;
    size_t wire_length = 0;
    int semantic_valid;

    receipt->application_invoked = 1;
    receipt->wire_status =
        cdc_scheduler_payload_decode(envelope->payload,
                                     envelope->payload_length, &payload);
    semantic_valid =
        receipt->wire_status == CDC_SCHEDULER_WIRE_OK &&
        envelope_matches_payload(envelope, &payload) &&
        envelope->horizon == supervisor_receipt->horizon;
    receipt->outcome =
        semantic_valid ? CDC_SCHEDULER_JOURNAL_OUTCOME_ACCEPT
                       : CDC_SCHEDULER_JOURNAL_OUTCOME_REJECT;
    if (semantic_valid) {
        receipt->payload_kind = payload.kind;
    }
    if (!encode_journal_record(receipt->outcome, envelope, &wire,
                               &wire_length)) {
        receipt->store_status = CDC_STORE_EMEM;
        return CDC_SUPERVISOR_APPLICATION_HOLD;
    }
    receipt->store_status =
        cdc_store_stage(context->store, wire, wire_length);
    free(wire);
    if (receipt->store_status != CDC_STORE_OK) {
        (void)cdc_store_rollback(context->store);
        return CDC_SUPERVISOR_APPLICATION_HOLD;
    }
    receipt->store_status = cdc_store_commit(context->store);
    if (receipt->store_status != CDC_STORE_OK) {
        (void)cdc_store_rollback(context->store);
        return CDC_SUPERVISOR_APPLICATION_HOLD;
    }
    receipt->durable = 1;
    return semantic_valid ? CDC_SUPERVISOR_APPLICATION_ACCEPT
                          : CDC_SUPERVISOR_APPLICATION_REJECT;
}

cdc_supervisor_verdict
cdc_scheduler_journal_admit(
    cdc_supervisor *supervisor, cdc_store *store,
    const cdc_transport_envelope *envelope, uint64_t now,
    uint32_t verified_approvals,
    cdc_scheduler_journal_admission_receipt *receipt) {
    journal_apply_context context;
    if (!receipt) {
        return CDC_SUPERVISOR_REJECT_INTERNAL;
    }
    memset(receipt, 0, sizeof(*receipt));
    receipt->wire_status = CDC_SCHEDULER_WIRE_EARG;
    receipt->store_status = CDC_STORE_EARG;
    if (!supervisor || !store || !envelope) {
        receipt->supervisor.verdict = CDC_SUPERVISOR_REJECT_INTERNAL;
        return CDC_SUPERVISOR_REJECT_INTERNAL;
    }
    context.store = store;
    context.receipt = receipt;
    return cdc_supervisor_admit_ex(
        supervisor, envelope, now, verified_approvals,
        append_scheduler_payload, &context, &receipt->supervisor);
}

static cdc_store_status collect_event(
    void *opaque, uint64_t event_sequence, uint64_t transaction_sequence,
    const void *payload, size_t payload_size) {
    replay_collector *collector = opaque;
    replay_event *event;
    cdc_transport_envelope envelope;
    cdc_transport_ticket ticket;
    cdc_scheduler_payload decoded;
    cdc_scheduler_wire_status decoded_status;
    cdc_scheduler_journal_outcome outcome;
    const uint8_t *envelope_wire = NULL;
    size_t envelope_length = 0;
    cdc_transport_verdict verdict;
    int semantic_valid;
    (void)transaction_sequence;
    if (collector->records_verified >= collector->capacity ||
        !decode_journal_record(payload, payload_size, &outcome,
                               &envelope_wire, &envelope_length)) {
        collector->wire_status = CDC_SCHEDULER_WIRE_ECANONICAL;
        return CDC_STORE_ESTATE;
    }
    cdc_transport_envelope_init(&envelope);
    if (!cdc_transport_envelope_decode(envelope_wire, envelope_length,
                                       CDC_SCHEDULER_PAYLOAD_MAX,
                                       &envelope)) {
        collector->wire_status = CDC_SCHEDULER_WIRE_ECANONICAL;
        return CDC_STORE_ESTATE;
    }
    verdict = cdc_transport_inspect(&collector->peer, &envelope,
                                    collector->key, &ticket);
    collector->transport_verdict = verdict;
    if (verdict != CDC_TRANSPORT_ACCEPT &&
        verdict != CDC_TRANSPORT_HOLD_DUPLICATE) {
        cdc_transport_envelope_free(&envelope);
        return CDC_STORE_ESTATE;
    }
    decoded_status =
        cdc_scheduler_payload_decode(envelope.payload,
                                     envelope.payload_length,
                                     &decoded);
    semantic_valid =
        decoded_status == CDC_SCHEDULER_WIRE_OK &&
        envelope_matches_payload(&envelope, &decoded);
    if ((outcome == CDC_SCHEDULER_JOURNAL_OUTCOME_ACCEPT &&
         !semantic_valid) ||
        (outcome == CDC_SCHEDULER_JOURNAL_OUTCOME_REJECT &&
         semantic_valid)) {
        collector->wire_status = decoded_status;
        if (collector->wire_status == CDC_SCHEDULER_WIRE_OK) {
            collector->wire_status = CDC_SCHEDULER_WIRE_ECANONICAL;
        }
        cdc_transport_envelope_free(&envelope);
        return CDC_STORE_ESTATE;
    }
    if (verdict == CDC_TRANSPORT_ACCEPT) {
        verdict = cdc_transport_prepare_accept(&collector->peer, &ticket);
        if (verdict == CDC_TRANSPORT_ACCEPT) {
            verdict = cdc_transport_accept(&collector->peer, &ticket);
        }
        collector->transport_verdict = verdict;
        if (verdict != CDC_TRANSPORT_ACCEPT) {
            cdc_transport_envelope_free(&envelope);
            return CDC_STORE_ESTATE;
        }
    }
    collector->records_verified++;
    if (outcome == CDC_SCHEDULER_JOURNAL_OUTCOME_REJECT) {
        collector->terminal_rejections++;
        cdc_transport_envelope_free(&envelope);
        return CDC_STORE_OK;
    }
    event = &collector->events[collector->count];
    event->payload = decoded;
    cdc_transport_envelope_free(&envelope);
    event->event_sequence = event_sequence;
    collector->count++;
    return CDC_STORE_OK;
}

static uint64_t replay_clock(const replay_event *event) {
    return event->payload.kind == CDC_SCHEDULER_PAYLOAD_OBSERVATION
               ? event->payload.observation.logical_clock
               : event->payload.target_logical_clock;
}

static uint64_t replay_member(const replay_event *event) {
    return event->payload.kind == CDC_SCHEDULER_PAYLOAD_OBSERVATION
               ? event->payload.observation.member_id
               : 0;
}

static int replay_event_compare(const void *left, const void *right) {
    const replay_event *a = left;
    const replay_event *b = right;
    uint64_t a_clock = replay_clock(a);
    uint64_t b_clock = replay_clock(b);
    int cell_order;
    if (a_clock != b_clock) {
        return a_clock < b_clock ? -1 : 1;
    }
    if (a->payload.kind != b->payload.kind) {
        return a->payload.kind < b->payload.kind ? -1 : 1;
    }
    cell_order = strcmp(a->payload.cell_id, b->payload.cell_id);
    if (cell_order != 0) {
        return cell_order;
    }
    if (replay_member(a) != replay_member(b)) {
        return replay_member(a) < replay_member(b) ? -1 : 1;
    }
    if (a->event_sequence == b->event_sequence) {
        return 0;
    }
    return a->event_sequence < b->event_sequence ? -1 : 1;
}

static void add_run_report(cdc_scheduler_journal_replay_report *target,
                           const cdc_scheduler_run_report *source) {
    target->duplicate_events += source->duplicate_events;
    target->leaf_updates += source->leaf_updates;
    target->recursive_updates += source->recursive_updates;
    target->frames_held += source->frames_held;
    target->queued_remaining = source->queued_remaining;
    memcpy(target->configuration_digest, source->configuration_digest,
           CDC_DIGEST_SIZE);
    memcpy(target->execution_digest, source->execution_digest,
           CDC_DIGEST_SIZE);
}

static cdc_scheduler_journal_status
drain_replay(cdc_scheduler *scheduler,
             cdc_scheduler_journal_replay_report *report) {
    cdc_scheduler_run_report run;
    cdc_scheduler_status status =
        cdc_scheduler_drain(scheduler, SIZE_MAX, &run);
    add_run_report(report, &run);
    report->scheduler_status = status;
    if (status == CDC_SCHEDULER_OK) {
        return CDC_SCHEDULER_JOURNAL_OK;
    }
    if (status == CDC_SCHEDULER_HOLD_INCOMPLETE) {
        return CDC_SCHEDULER_JOURNAL_HOLD_INCOMPLETE;
    }
    if (status == CDC_SCHEDULER_HOLD_LIMIT) {
        return CDC_SCHEDULER_JOURNAL_HOLD_LIMIT;
    }
    return CDC_SCHEDULER_JOURNAL_ESCHEDULER;
}

cdc_scheduler_journal_status
cdc_scheduler_journal_replay(
    cdc_store *store, cdc_scheduler *fresh_scheduler,
    const uint8_t key[CDC_TRANSPORT_TAG_SIZE],
    const cdc_scheduler_journal_replay_config *config,
    size_t maximum_events,
    cdc_scheduler_journal_replay_report *report) {
    replay_collector collector;
    replay_event *events = NULL;
    cdc_store_status store_status;
    cdc_scheduler_journal_status result = CDC_SCHEDULER_JOURNAL_OK;
    uint64_t event_count;
    size_t i;

    if (!store || !fresh_scheduler || !key || !config || !report ||
        config->schema_version == 0 || !config->local_id ||
        !config->key_id || config->stream_limit == 0 ||
        maximum_events == 0 ||
        !cdc_scheduler_is_pristine(fresh_scheduler)) {
        return CDC_SCHEDULER_JOURNAL_EARG;
    }
    memset(report, 0, sizeof(*report));
    report->store_status = CDC_STORE_OK;
    report->transport_verdict = CDC_TRANSPORT_ACCEPT;
    report->wire_status = CDC_SCHEDULER_WIRE_OK;
    report->scheduler_status = CDC_SCHEDULER_OK;
    event_count = cdc_store_event_count(store);
    if (event_count > maximum_events || event_count > SIZE_MAX) {
        return CDC_SCHEDULER_JOURNAL_HOLD_LIMIT;
    }
    if (event_count > 0) {
        events = calloc((size_t)event_count, sizeof(*events));
        if (!events) {
            return CDC_SCHEDULER_JOURNAL_HOLD_LIMIT;
        }
    }
    memset(&collector, 0, sizeof(collector));
    collector.events = events;
    collector.capacity = (size_t)event_count;
    collector.key = key;
    collector.transport_verdict = CDC_TRANSPORT_ACCEPT;
    collector.wire_status = CDC_SCHEDULER_WIRE_OK;
    if (!cdc_transport_peer_init(
            &collector.peer, config->schema_version, config->local_id,
            config->key_id, CDC_SCHEDULER_PAYLOAD_MAX,
            config->stream_limit)) {
        free(events);
        return CDC_SCHEDULER_JOURNAL_HOLD_LIMIT;
    }
    store_status =
        cdc_store_visit_events(store, collect_event, &collector);
    report->store_status = store_status;
    report->transport_verdict = collector.transport_verdict;
    report->wire_status = collector.wire_status;
    if (store_status == CDC_STORE_EUNSUPPORTED) {
        result = CDC_SCHEDULER_JOURNAL_EUNSUPPORTED;
        goto done;
    }
    if (store_status != CDC_STORE_OK) {
        result = collector.transport_verdict != CDC_TRANSPORT_ACCEPT
                     ? (collector.transport_verdict ==
                                    CDC_TRANSPORT_REJECT_AUTH ||
                                collector.transport_verdict ==
                                    CDC_TRANSPORT_REJECT_IDENTITY ||
                                collector.transport_verdict ==
                                    CDC_TRANSPORT_REJECT_PAYLOAD
                            ? CDC_SCHEDULER_JOURNAL_EAUTH
                            : CDC_SCHEDULER_JOURNAL_EPOLICY)
                 : collector.wire_status != CDC_SCHEDULER_WIRE_OK
                     ? CDC_SCHEDULER_JOURNAL_EWIRE
                     : CDC_SCHEDULER_JOURNAL_ESTORE;
        goto done;
    }
    if (collector.records_verified != (size_t)event_count) {
        result = CDC_SCHEDULER_JOURNAL_ESTORE;
        goto done;
    }
    report->records_verified = collector.records_verified;
    report->terminal_rejections = collector.terminal_rejections;
    if (collector.count > 1) {
        qsort(events, collector.count, sizeof(*events),
              replay_event_compare);
    }
    for (i = 0; i < collector.count; i++) {
        cdc_scheduler_status status;
        if (events[i].payload.kind ==
            CDC_SCHEDULER_PAYLOAD_OBSERVATION) {
            status = cdc_scheduler_publish(
                fresh_scheduler, &events[i].payload.observation,
                events[i].payload.observation.seal_time);
        } else {
            status = cdc_scheduler_publish_witness(
                fresh_scheduler, events[i].payload.cell_id,
                events[i].payload.target_logical_clock,
                &events[i].payload.witness);
        }
        if (status == CDC_SCHEDULER_HOLD_LIMIT) {
            result = drain_replay(fresh_scheduler, report);
            if (result != CDC_SCHEDULER_JOURNAL_OK &&
                result != CDC_SCHEDULER_JOURNAL_HOLD_INCOMPLETE) {
                goto done;
            }
            if (events[i].payload.kind ==
                CDC_SCHEDULER_PAYLOAD_OBSERVATION) {
                status = cdc_scheduler_publish(
                    fresh_scheduler, &events[i].payload.observation,
                    events[i].payload.observation.seal_time);
            } else {
                status = cdc_scheduler_publish_witness(
                    fresh_scheduler, events[i].payload.cell_id,
                    events[i].payload.target_logical_clock,
                    &events[i].payload.witness);
            }
        }
        if (status == CDC_SCHEDULER_HOLD_DUPLICATE) {
            report->duplicate_events++;
        } else if (status != CDC_SCHEDULER_OK) {
            report->scheduler_status = status;
            result = status == CDC_SCHEDULER_HOLD_LIMIT
                         ? CDC_SCHEDULER_JOURNAL_HOLD_LIMIT
                         : CDC_SCHEDULER_JOURNAL_ESCHEDULER;
            goto done;
        }
        report->events_recovered++;
    }
    do {
        result = drain_replay(fresh_scheduler, report);
    } while (result == CDC_SCHEDULER_JOURNAL_OK &&
             cdc_scheduler_queued_count(fresh_scheduler) > 0);
    if (result == CDC_SCHEDULER_JOURNAL_OK &&
        cdc_scheduler_queued_count(fresh_scheduler) != 0) {
        result = CDC_SCHEDULER_JOURNAL_HOLD_INCOMPLETE;
    }

done:
    report->queued_remaining =
        cdc_scheduler_queued_count(fresh_scheduler);
    cdc_transport_peer_free(&collector.peer);
    free(events);
    return result;
}

const char *cdc_scheduler_journal_status_name(
    cdc_scheduler_journal_status status) {
    static const char *const NAMES[] = {
        "ok",          "hold-limit", "hold-incomplete", "argument",
        "store",       "wire",       "auth",            "policy",
        "scheduler",   "unsupported",
    };
    if ((size_t)status >= sizeof(NAMES) / sizeof(NAMES[0])) {
        return "unknown";
    }
    return NAMES[status];
}
