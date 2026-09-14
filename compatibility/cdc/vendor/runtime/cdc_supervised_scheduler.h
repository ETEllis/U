#ifndef CDC_SUPERVISED_SCHEDULER_H
#define CDC_SUPERVISED_SCHEDULER_H

#include "cdc_scheduler_wire.h"
#include "cdc_supervisor.h"

typedef struct {
    cdc_supervisor_receipt supervisor;
    cdc_scheduler_wire_status wire_status;
    cdc_scheduler_status scheduler_status;
    cdc_scheduler_payload_kind payload_kind;
    int application_invoked;
    int application_mutated;
} cdc_supervised_scheduler_receipt;

/*
 * Authenticates, causally orders, authorizes, canonically decodes, and then
 * publishes one scheduler command. Lock order is supervisor -> scheduler.
 * No scheduler API acquires a supervisor lock, so callers must preserve that
 * one-way composition boundary.
 */
cdc_supervisor_verdict
cdc_supervised_scheduler_admit(
    cdc_supervisor *supervisor, cdc_scheduler *scheduler,
    const cdc_transport_envelope *envelope, uint64_t now,
    uint32_t verified_approvals,
    cdc_supervised_scheduler_receipt *receipt);

#endif
