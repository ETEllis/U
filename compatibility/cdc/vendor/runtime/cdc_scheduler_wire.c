#include "cdc_scheduler_wire.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum {
    HEADER_SIZE = 12,
    OBSERVATION_FIXED_SIZE = 96,
    WITNESS_FIXED_SIZE = 64,
    FLAG_HAS_WITNESS = 1
};

static const uint8_t MAGIC[4] = {'C', 'D', 'C', 'P'};
static const double PHASE_SCALE = 1000000000000.0;
static const int64_t PI_QUANTIZED = INT64_C(3141592653590);

static size_t bounded_length(const char *text, size_t limit) {
    size_t length = 0;
    if (!text) {
        return limit + 1;
    }
    while (length <= limit && text[length] != '\0') {
        length++;
    }
    return length;
}

static int digest_is_zero(const uint8_t digest[CDC_DIGEST_SIZE]) {
    uint8_t combined = 0;
    size_t i;
    for (i = 0; i < CDC_DIGEST_SIZE; i++) {
        combined |= digest[i];
    }
    return combined == 0;
}

static double wrap_pi(double value) {
    double wrapped = fmod(value, 2.0 * M_PI);
    if (wrapped <= -M_PI) {
        wrapped += 2.0 * M_PI;
    } else if (wrapped > M_PI) {
        wrapped -= 2.0 * M_PI;
    }
    return wrapped;
}

static void put_u16(uint8_t *out, uint16_t value) {
    out[0] = (uint8_t)(value >> 8);
    out[1] = (uint8_t)value;
}

static void put_u64(uint8_t *out, uint64_t value) {
    size_t i;
    for (i = 0; i < 8; i++) {
        out[7 - i] = (uint8_t)(value >> (i * 8));
    }
}

static uint16_t get_u16(const uint8_t *bytes) {
    return (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}

static uint64_t get_u64(const uint8_t *bytes) {
    uint64_t value = 0;
    size_t i;
    for (i = 0; i < 8; i++) {
        value = (value << 8) | bytes[i];
    }
    return value;
}

static int64_t decode_i64(uint64_t raw) {
    if (raw <= (uint64_t)INT64_MAX) {
        return (int64_t)raw;
    }
    return -INT64_C(1) - (int64_t)(~raw);
}

static int witness_valid(const cdc_topology_witness *witness,
                         uint64_t target) {
    return witness &&
           (witness->kind == CDC_TOPOLOGY_EVENT_LOCAL_DEFORMATION ||
            witness->kind == CDC_TOPOLOGY_EVENT_PHASE_SLIP ||
            witness->kind == CDC_TOPOLOGY_EVENT_FRAME_CHANGE) &&
           witness->from_member != 0 && witness->to_member != 0 &&
           witness->logical_clock != 0 &&
           witness->logical_clock <= target &&
           !digest_is_zero(witness->evidence_digest);
}

static int observation_valid(
    const cdc_scheduler_observation *observation) {
    size_t cell_length;
    if (!observation) {
        return 0;
    }
    cell_length = bounded_length(observation->cell_id, CDC_CELL_ID_MAX);
    return cell_length > 0 && cell_length <= CDC_CELL_ID_MAX &&
           (observation->has_witness == 0 ||
            observation->has_witness == 1) &&
           observation->member_id != 0 && isfinite(observation->phase) &&
           observation->logical_clock != 0 &&
           observation->frame_version != 0 &&
           observation->window_start <= observation->window_end &&
           observation->observed_at >= observation->window_start &&
           observation->observed_at <= observation->window_end &&
           observation->seal_time >= observation->window_end &&
           !digest_is_zero(observation->source_digest) &&
           (!observation->has_witness ||
            witness_valid(&observation->witness,
                          observation->logical_clock));
}

static void encode_header(uint8_t *out, cdc_scheduler_payload_kind kind,
                          uint8_t flags, size_t total_length,
                          size_t cell_length) {
    memcpy(out, MAGIC, sizeof(MAGIC));
    put_u16(out + 4, CDC_SCHEDULER_WIRE_VERSION);
    out[6] = (uint8_t)kind;
    out[7] = flags;
    put_u16(out + 8, (uint16_t)total_length);
    out[10] = (uint8_t)cell_length;
    out[11] = 0;
}

static size_t encode_witness_body(uint8_t *out,
                                  const cdc_topology_witness *witness) {
    out[0] = (uint8_t)witness->kind;
    memset(out + 1, 0, 7);
    put_u64(out + 8, witness->from_member);
    put_u64(out + 16, witness->to_member);
    put_u64(out + 24, witness->logical_clock);
    memcpy(out + 32, witness->evidence_digest, CDC_DIGEST_SIZE);
    return WITNESS_FIXED_SIZE;
}

cdc_scheduler_wire_status
cdc_scheduler_payload_encode_observation(
    const cdc_scheduler_observation *observation,
    uint8_t out[CDC_SCHEDULER_PAYLOAD_MAX], size_t *out_length) {
    size_t cell_length;
    size_t length;
    size_t offset;
    int64_t phase;
    if (!out || !out_length || !observation_valid(observation)) {
        return CDC_SCHEDULER_WIRE_EARG;
    }
    cell_length = bounded_length(observation->cell_id, CDC_CELL_ID_MAX);
    if (cell_length == 0 || cell_length > CDC_CELL_ID_MAX) {
        return CDC_SCHEDULER_WIRE_EARG;
    }
    length = HEADER_SIZE + cell_length + OBSERVATION_FIXED_SIZE +
             (observation->has_witness ? WITNESS_FIXED_SIZE : 0);
    if (length > CDC_SCHEDULER_PAYLOAD_MAX || length > UINT16_MAX) {
        return CDC_SCHEDULER_WIRE_ELIMIT;
    }
    memset(out, 0, CDC_SCHEDULER_PAYLOAD_MAX);
    encode_header(out, CDC_SCHEDULER_PAYLOAD_OBSERVATION,
                  observation->has_witness ? FLAG_HAS_WITNESS : 0, length,
                  cell_length);
    offset = HEADER_SIZE;
    memcpy(out + offset, observation->cell_id, cell_length);
    offset += cell_length;
    put_u64(out + offset, observation->member_id);
    offset += 8;
    phase = (int64_t)llround(wrap_pi(observation->phase) * PHASE_SCALE);
    if (phase <= -PI_QUANTIZED) {
        phase = PI_QUANTIZED;
    }
    put_u64(out + offset, (uint64_t)phase);
    offset += 8;
    put_u64(out + offset, observation->observed_at);
    offset += 8;
    put_u64(out + offset, observation->logical_clock);
    offset += 8;
    put_u64(out + offset, observation->frame_version);
    offset += 8;
    put_u64(out + offset, observation->window_start);
    offset += 8;
    put_u64(out + offset, observation->window_end);
    offset += 8;
    put_u64(out + offset, observation->seal_time);
    offset += 8;
    memcpy(out + offset, observation->source_digest, CDC_DIGEST_SIZE);
    offset += CDC_DIGEST_SIZE;
    if (observation->has_witness) {
        offset += encode_witness_body(out + offset,
                                      &observation->witness);
    }
    if (offset != length) {
        return CDC_SCHEDULER_WIRE_ECANONICAL;
    }
    *out_length = length;
    return CDC_SCHEDULER_WIRE_OK;
}

cdc_scheduler_wire_status
cdc_scheduler_payload_encode_witness(
    const char *cell_id, uint64_t target_logical_clock,
    const cdc_topology_witness *witness,
    uint8_t out[CDC_SCHEDULER_PAYLOAD_MAX], size_t *out_length) {
    size_t cell_length = bounded_length(cell_id, CDC_CELL_ID_MAX);
    size_t length;
    size_t offset;
    if (!out || !out_length || target_logical_clock == 0 ||
        cell_length == 0 || cell_length > CDC_CELL_ID_MAX ||
        !witness_valid(witness, target_logical_clock)) {
        return CDC_SCHEDULER_WIRE_EARG;
    }
    length = HEADER_SIZE + cell_length + 8 + WITNESS_FIXED_SIZE;
    if (length > CDC_SCHEDULER_PAYLOAD_MAX || length > UINT16_MAX) {
        return CDC_SCHEDULER_WIRE_ELIMIT;
    }
    memset(out, 0, CDC_SCHEDULER_PAYLOAD_MAX);
    encode_header(out, CDC_SCHEDULER_PAYLOAD_WITNESS, 0, length,
                  cell_length);
    offset = HEADER_SIZE;
    memcpy(out + offset, cell_id, cell_length);
    offset += cell_length;
    put_u64(out + offset, target_logical_clock);
    offset += 8;
    offset += encode_witness_body(out + offset, witness);
    if (offset != length) {
        return CDC_SCHEDULER_WIRE_ECANONICAL;
    }
    *out_length = length;
    return CDC_SCHEDULER_WIRE_OK;
}

static int decode_witness_body(const uint8_t *bytes,
                               cdc_topology_witness *out) {
    size_t i;
    memset(out, 0, sizeof(*out));
    out->kind = (cdc_topology_event_kind)bytes[0];
    for (i = 1; i < 8; i++) {
        if (bytes[i] != 0) {
            return 0;
        }
    }
    out->from_member = get_u64(bytes + 8);
    out->to_member = get_u64(bytes + 16);
    out->logical_clock = get_u64(bytes + 24);
    memcpy(out->evidence_digest, bytes + 32, CDC_DIGEST_SIZE);
    return 1;
}

cdc_scheduler_wire_status
cdc_scheduler_payload_decode(const uint8_t *bytes, size_t length,
                             cdc_scheduler_payload *out) {
    cdc_scheduler_payload decoded;
    cdc_scheduler_payload_kind kind;
    uint8_t flags;
    size_t cell_length;
    size_t expected;
    size_t offset;
    int64_t phase;
    uint8_t canonical[CDC_SCHEDULER_PAYLOAD_MAX];
    size_t canonical_length = 0;
    if (!bytes || !out || length < HEADER_SIZE ||
        length > CDC_SCHEDULER_PAYLOAD_MAX) {
        return CDC_SCHEDULER_WIRE_EARG;
    }
    if (memcmp(bytes, MAGIC, sizeof(MAGIC)) != 0 ||
        get_u16(bytes + 4) != CDC_SCHEDULER_WIRE_VERSION) {
        return CDC_SCHEDULER_WIRE_ESCHEMA;
    }
    kind = (cdc_scheduler_payload_kind)bytes[6];
    flags = bytes[7];
    cell_length = bytes[10];
    if (get_u16(bytes + 8) != length || bytes[11] != 0 ||
        cell_length == 0 || cell_length > CDC_CELL_ID_MAX ||
        (kind != CDC_SCHEDULER_PAYLOAD_OBSERVATION &&
         kind != CDC_SCHEDULER_PAYLOAD_WITNESS)) {
        return CDC_SCHEDULER_WIRE_ECANONICAL;
    }
    if ((kind == CDC_SCHEDULER_PAYLOAD_OBSERVATION &&
         (flags & (uint8_t)~FLAG_HAS_WITNESS) != 0) ||
        (kind == CDC_SCHEDULER_PAYLOAD_WITNESS && flags != 0)) {
        return CDC_SCHEDULER_WIRE_ECANONICAL;
    }
    expected = HEADER_SIZE + cell_length;
    expected += kind == CDC_SCHEDULER_PAYLOAD_OBSERVATION
                    ? OBSERVATION_FIXED_SIZE +
                          ((flags & FLAG_HAS_WITNESS)
                               ? WITNESS_FIXED_SIZE
                               : 0)
                    : 8 + WITNESS_FIXED_SIZE;
    if (expected != length) {
        return CDC_SCHEDULER_WIRE_ECANONICAL;
    }
    memset(&decoded, 0, sizeof(decoded));
    decoded.kind = kind;
    offset = HEADER_SIZE;
    memcpy(decoded.cell_id, bytes + offset, cell_length);
    decoded.cell_id[cell_length] = '\0';
    offset += cell_length;
    if (kind == CDC_SCHEDULER_PAYLOAD_OBSERVATION) {
        memcpy(decoded.observation.cell_id, decoded.cell_id,
               cell_length + 1);
        decoded.observation.member_id = get_u64(bytes + offset);
        offset += 8;
        phase = decode_i64(get_u64(bytes + offset));
        offset += 8;
        if (phase <= -PI_QUANTIZED || phase > PI_QUANTIZED) {
            return CDC_SCHEDULER_WIRE_ECANONICAL;
        }
        decoded.observation.phase = (double)phase / PHASE_SCALE;
        decoded.observation.observed_at = get_u64(bytes + offset);
        offset += 8;
        decoded.observation.logical_clock = get_u64(bytes + offset);
        offset += 8;
        decoded.observation.frame_version = get_u64(bytes + offset);
        offset += 8;
        decoded.observation.window_start = get_u64(bytes + offset);
        offset += 8;
        decoded.observation.window_end = get_u64(bytes + offset);
        offset += 8;
        decoded.observation.seal_time = get_u64(bytes + offset);
        offset += 8;
        memcpy(decoded.observation.source_digest, bytes + offset,
               CDC_DIGEST_SIZE);
        offset += CDC_DIGEST_SIZE;
        decoded.observation.has_witness =
            (flags & FLAG_HAS_WITNESS) != 0;
        if (decoded.observation.has_witness) {
            if (!decode_witness_body(
                    bytes + offset, &decoded.observation.witness)) {
                return CDC_SCHEDULER_WIRE_ECANONICAL;
            }
            offset += WITNESS_FIXED_SIZE;
        }
        if (cdc_scheduler_payload_encode_observation(
                &decoded.observation, canonical, &canonical_length) !=
                CDC_SCHEDULER_WIRE_OK ||
            canonical_length != length ||
            memcmp(canonical, bytes, length) != 0) {
            return CDC_SCHEDULER_WIRE_ECANONICAL;
        }
    } else {
        decoded.target_logical_clock = get_u64(bytes + offset);
        offset += 8;
        if (!decode_witness_body(bytes + offset, &decoded.witness)) {
            return CDC_SCHEDULER_WIRE_ECANONICAL;
        }
        offset += WITNESS_FIXED_SIZE;
        if (!witness_valid(&decoded.witness,
                           decoded.target_logical_clock)) {
            return CDC_SCHEDULER_WIRE_ECANONICAL;
        }
        if (cdc_scheduler_payload_encode_witness(
                decoded.cell_id, decoded.target_logical_clock,
                &decoded.witness, canonical, &canonical_length) !=
                CDC_SCHEDULER_WIRE_OK ||
            canonical_length != length ||
            memcmp(canonical, bytes, length) != 0) {
            return CDC_SCHEDULER_WIRE_ECANONICAL;
        }
    }
    if (offset != length) {
        return CDC_SCHEDULER_WIRE_ECANONICAL;
    }
    *out = decoded;
    return CDC_SCHEDULER_WIRE_OK;
}

const char *
cdc_scheduler_wire_status_name(cdc_scheduler_wire_status status) {
    static const char *const NAMES[] = {
        "ok", "argument", "limit", "schema", "canonical",
    };
    if ((size_t)status >= sizeof(NAMES) / sizeof(NAMES[0])) {
        return "unknown";
    }
    return NAMES[status];
}
