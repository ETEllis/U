#include "cdc_supervised_scheduler.h"

#include <string.h>

typedef struct {
    cdc_scheduler *scheduler;
    uint64_t now;
    cdc_supervised_scheduler_receipt *receipt;
} supervised_apply_context;

static cdc_supervisor_application_verdict
apply_scheduler_payload(const cdc_transport_envelope *envelope,
                        const cdc_supervisor_receipt *supervisor_receipt,
                        void *opaque) {
    supervised_apply_context *context = opaque;
    cdc_supervised_scheduler_receipt *receipt = context->receipt;
    cdc_scheduler_payload payload;
    cdc_scheduler_status status;

    receipt->application_invoked = 1;
    receipt->wire_status =
        cdc_scheduler_payload_decode(envelope->payload,
                                     envelope->payload_length, &payload);
    if (receipt->wire_status != CDC_SCHEDULER_WIRE_OK) {
        return CDC_SUPERVISOR_APPLICATION_REJECT;
    }
    receipt->payload_kind = payload.kind;
    if (strcmp(payload.cell_id, envelope->frame) != 0) {
        return CDC_SUPERVISOR_APPLICATION_REJECT;
    }
    if (payload.kind == CDC_SCHEDULER_PAYLOAD_OBSERVATION) {
        if (envelope->message_type != CDC_TRANSPORT_OBSERVATION ||
            envelope->authority_action != CDC_AUTH_OBSERVE ||
            payload.observation.logical_clock >
                supervisor_receipt->horizon) {
            return CDC_SUPERVISOR_APPLICATION_REJECT;
        }
        status = cdc_scheduler_publish(context->scheduler,
                                       &payload.observation, context->now);
    } else if (payload.kind == CDC_SCHEDULER_PAYLOAD_WITNESS) {
        if (envelope->message_type != CDC_TRANSPORT_DECISION ||
            envelope->authority_action != CDC_AUTH_COMMIT ||
            payload.target_logical_clock >
                supervisor_receipt->horizon) {
            return CDC_SUPERVISOR_APPLICATION_REJECT;
        }
        status = cdc_scheduler_publish_witness(
            context->scheduler, payload.cell_id,
            payload.target_logical_clock, &payload.witness);
    } else {
        return CDC_SUPERVISOR_APPLICATION_REJECT;
    }
    receipt->scheduler_status = status;
    if (status == CDC_SCHEDULER_OK) {
        receipt->application_mutated = 1;
        return CDC_SUPERVISOR_APPLICATION_ACCEPT;
    }
    if (status == CDC_SCHEDULER_HOLD_DUPLICATE) {
        return CDC_SUPERVISOR_APPLICATION_ACCEPT;
    }
    if (status == CDC_SCHEDULER_HOLD_LIMIT ||
        status == CDC_SCHEDULER_HOLD_INCOMPLETE ||
        status == CDC_SCHEDULER_HOLD_STALE) {
        return CDC_SUPERVISOR_APPLICATION_HOLD;
    }
    return CDC_SUPERVISOR_APPLICATION_REJECT;
}

cdc_supervisor_verdict
cdc_supervised_scheduler_admit(
    cdc_supervisor *supervisor, cdc_scheduler *scheduler,
    const cdc_transport_envelope *envelope, uint64_t now,
    uint32_t verified_approvals,
    cdc_supervised_scheduler_receipt *receipt) {
    supervised_apply_context context;
    cdc_supervisor_verdict verdict;
    if (!receipt) {
        return CDC_SUPERVISOR_REJECT_INTERNAL;
    }
    memset(receipt, 0, sizeof(*receipt));
    receipt->wire_status = CDC_SCHEDULER_WIRE_EARG;
    receipt->scheduler_status = CDC_SCHEDULER_EARG;
    if (!supervisor || !scheduler || !envelope) {
        receipt->supervisor.verdict = CDC_SUPERVISOR_REJECT_INTERNAL;
        return CDC_SUPERVISOR_REJECT_INTERNAL;
    }
    context.scheduler = scheduler;
    context.now = now;
    context.receipt = receipt;
    verdict = cdc_supervisor_admit_ex(
        supervisor, envelope, now, verified_approvals,
        apply_scheduler_payload, &context, &receipt->supervisor);
    return verdict;
}
