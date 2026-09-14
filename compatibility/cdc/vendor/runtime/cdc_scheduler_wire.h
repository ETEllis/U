#ifndef CDC_SCHEDULER_WIRE_H
#define CDC_SCHEDULER_WIRE_H

#include <stddef.h>
#include <stdint.h>

#include "cdc_scheduler.h"

enum {
    CDC_SCHEDULER_WIRE_VERSION = 1,
    CDC_SCHEDULER_PAYLOAD_MAX = 256
};

typedef enum {
    CDC_SCHEDULER_PAYLOAD_OBSERVATION = 1,
    CDC_SCHEDULER_PAYLOAD_WITNESS = 2
} cdc_scheduler_payload_kind;

typedef enum {
    CDC_SCHEDULER_WIRE_OK = 0,
    CDC_SCHEDULER_WIRE_EARG,
    CDC_SCHEDULER_WIRE_ELIMIT,
    CDC_SCHEDULER_WIRE_ESCHEMA,
    CDC_SCHEDULER_WIRE_ECANONICAL
} cdc_scheduler_wire_status;

typedef struct {
    cdc_scheduler_payload_kind kind;
    cdc_scheduler_observation observation;
    char cell_id[CDC_CELL_ID_MAX + 1];
    uint64_t target_logical_clock;
    cdc_topology_witness witness;
} cdc_scheduler_payload;

cdc_scheduler_wire_status
cdc_scheduler_payload_encode_observation(
    const cdc_scheduler_observation *observation,
    uint8_t out[CDC_SCHEDULER_PAYLOAD_MAX], size_t *out_length);

cdc_scheduler_wire_status
cdc_scheduler_payload_encode_witness(
    const char *cell_id, uint64_t target_logical_clock,
    const cdc_topology_witness *witness,
    uint8_t out[CDC_SCHEDULER_PAYLOAD_MAX], size_t *out_length);

cdc_scheduler_wire_status
cdc_scheduler_payload_decode(const uint8_t *bytes, size_t length,
                             cdc_scheduler_payload *out);

const char *
cdc_scheduler_wire_status_name(cdc_scheduler_wire_status status);

#endif
