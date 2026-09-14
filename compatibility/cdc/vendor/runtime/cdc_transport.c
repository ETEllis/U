#include "cdc_transport.h"

#include "cdc_authority.h"
#include "cdc_blake3.h"

#include <stdlib.h>
#include <string.h>

static const uint8_t MAC_DOMAIN[] = "CDC-RFTC-WIRE-MAC-V1";
static const uint8_t ID_DOMAIN[] = "CDC-RFTC-WIRE-ID-V1";
static const uint8_t WIRE_MAGIC[4] = {'R', 'F', '3', 'W'};

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

static int id_valid(const char *id) {
    size_t n = bounded_length(id, CDC_TRANSPORT_ID_MAX);
    return n > 0 && n <= CDC_TRANSPORT_ID_MAX;
}

static int bytes_zero(const uint8_t *bytes, size_t length) {
    uint8_t value = 0;
    size_t i;
    for (i = 0; i < length; i++) {
        value |= bytes[i];
    }
    return value == 0;
}

static int constant_equal(const uint8_t *a, const uint8_t *b, size_t length) {
    uint8_t difference = 0;
    size_t i;
    for (i = 0; i < length; i++) {
        difference |= (uint8_t)(a[i] ^ b[i]);
    }
    return difference == 0;
}

static int action_matches_message(uint32_t message_type, uint32_t action) {
    if (message_type == CDC_TRANSPORT_OBSERVATION ||
        message_type == CDC_TRANSPORT_RECEIPT) {
        return action == CDC_AUTH_OBSERVE;
    }
    if (message_type == CDC_TRANSPORT_PROPOSAL) {
        return action == CDC_AUTH_PROPOSE;
    }
    if (message_type == CDC_TRANSPORT_DECISION) {
        return action == CDC_AUTH_COMMIT || action == CDC_AUTH_ENACT ||
               action == CDC_AUTH_BRIDGE || action == CDC_AUTH_REVOKE;
    }
    return 0;
}

static void update_u16(cdc_blake3_hasher *hasher, uint16_t value) {
    uint8_t bytes[2] = {(uint8_t)(value >> 8), (uint8_t)value};
    cdc_blake3_update(hasher, bytes, sizeof(bytes));
}

static void update_u32(cdc_blake3_hasher *hasher, uint32_t value) {
    uint8_t bytes[4] = {(uint8_t)(value >> 24), (uint8_t)(value >> 16),
                        (uint8_t)(value >> 8), (uint8_t)value};
    cdc_blake3_update(hasher, bytes, sizeof(bytes));
}

static void update_u64(cdc_blake3_hasher *hasher, uint64_t value) {
    uint8_t bytes[8];
    size_t i;
    for (i = 0; i < 8; i++) {
        bytes[7 - i] = (uint8_t)(value >> (i * 8));
    }
    cdc_blake3_update(hasher, bytes, sizeof(bytes));
}

static int update_string(cdc_blake3_hasher *hasher, const char *text) {
    size_t length = bounded_length(text, CDC_TRANSPORT_ID_MAX);
    if (length == 0 || length > CDC_TRANSPORT_ID_MAX) {
        return 0;
    }
    update_u16(hasher, (uint16_t)length);
    cdc_blake3_update(hasher, text, length);
    return 1;
}

static int envelope_structurally_valid(const cdc_transport_envelope *envelope) {
    return envelope && envelope->schema_version > 0 &&
           envelope->message_type >= CDC_TRANSPORT_OBSERVATION &&
           envelope->message_type <= CDC_TRANSPORT_RECEIPT &&
           id_valid(envelope->sender) && id_valid(envelope->recipient) &&
           id_valid(envelope->frame) && id_valid(envelope->key_id) &&
           id_valid(envelope->lease_id) && envelope->authority_action > 0 &&
           (envelope->authority_action &
            (envelope->authority_action - 1)) == 0 &&
           envelope->authority_action <= (1u << 5) &&
           action_matches_message(envelope->message_type,
                                  envelope->authority_action) &&
           envelope->sequence > 0 &&
           !bytes_zero(envelope->nonce, CDC_TRANSPORT_NONCE_SIZE) &&
           (envelope->payload_length == 0 || envelope->payload != NULL);
}

static int canonical_update(cdc_blake3_hasher *hasher,
                            const cdc_transport_envelope *envelope,
                            int include_mac) {
    if (!envelope_structurally_valid(envelope)) {
        return 0;
    }
    update_u32(hasher, envelope->schema_version);
    update_u32(hasher, envelope->message_type);
    if (!update_string(hasher, envelope->sender) ||
        !update_string(hasher, envelope->recipient) ||
        !update_string(hasher, envelope->frame) ||
        !update_string(hasher, envelope->key_id) ||
        !update_string(hasher, envelope->lease_id)) {
        return 0;
    }
    update_u32(hasher, envelope->authority_action);
    update_u64(hasher, envelope->horizon);
    update_u64(hasher, envelope->sequence);
    update_u64(hasher, envelope->logical_clock);
    cdc_blake3_update(hasher, envelope->nonce, CDC_TRANSPORT_NONCE_SIZE);
    cdc_blake3_update(hasher, envelope->causal_parent,
                      CDC_TRANSPORT_TAG_SIZE);
    update_u64(hasher, (uint64_t)envelope->payload_length);
    cdc_blake3_update(hasher, envelope->payload_digest,
                      CDC_TRANSPORT_TAG_SIZE);
    if (envelope->payload_length > 0) {
        cdc_blake3_update(hasher, envelope->payload,
                          envelope->payload_length);
    }
    if (include_mac) {
        cdc_blake3_update(hasher, envelope->mac, CDC_TRANSPORT_TAG_SIZE);
    }
    return 1;
}

static void payload_digest(const cdc_transport_envelope *envelope,
                           uint8_t out[CDC_TRANSPORT_TAG_SIZE]) {
    cdc_blake3_hasher hasher;
    cdc_blake3_init(&hasher);
    if (envelope->payload_length > 0) {
        cdc_blake3_update(&hasher, envelope->payload,
                          envelope->payload_length);
    }
    cdc_blake3_final(&hasher, out);
}

static int compute_mac(const cdc_transport_envelope *envelope,
                       const uint8_t key[CDC_TRANSPORT_TAG_SIZE],
                       uint8_t out[CDC_TRANSPORT_TAG_SIZE]) {
    cdc_blake3_hasher hasher;
    cdc_blake3_init_keyed(&hasher, key);
    cdc_blake3_update(&hasher, MAC_DOMAIN, sizeof(MAC_DOMAIN));
    if (!canonical_update(&hasher, envelope, 0)) {
        return 0;
    }
    cdc_blake3_final(&hasher, out);
    return 1;
}

static int compute_identity(const cdc_transport_envelope *envelope,
                            uint8_t out[CDC_TRANSPORT_TAG_SIZE]) {
    cdc_blake3_hasher hasher;
    cdc_blake3_init(&hasher);
    cdc_blake3_update(&hasher, ID_DOMAIN, sizeof(ID_DOMAIN));
    if (!canonical_update(&hasher, envelope, 1)) {
        return 0;
    }
    cdc_blake3_final(&hasher, out);
    return 1;
}

void cdc_transport_envelope_init(cdc_transport_envelope *envelope) {
    if (envelope) {
        memset(envelope, 0, sizeof(*envelope));
    }
}

void cdc_transport_envelope_free(cdc_transport_envelope *envelope) {
    if (!envelope) {
        return;
    }
    free(envelope->payload);
    memset(envelope, 0, sizeof(*envelope));
}

int cdc_transport_envelope_set_payload(cdc_transport_envelope *envelope,
                                       const void *payload, size_t length) {
    uint8_t *copy = NULL;
    if (!envelope || (length > 0 && !payload)) {
        return 0;
    }
    if (length > 0) {
        copy = malloc(length);
        if (!copy) {
            return 0;
        }
        memcpy(copy, payload, length);
    }
    free(envelope->payload);
    envelope->payload = copy;
    envelope->payload_length = length;
    return 1;
}

int cdc_transport_envelope_sign(
    cdc_transport_envelope *envelope,
    const uint8_t key[CDC_TRANSPORT_TAG_SIZE]) {
    if (!key || !envelope_structurally_valid(envelope)) {
        return 0;
    }
    payload_digest(envelope, envelope->payload_digest);
    if (!compute_mac(envelope, key, envelope->mac)) {
        return 0;
    }
    return compute_identity(envelope, envelope->envelope_digest);
}

cdc_transport_verdict
cdc_transport_envelope_verify_auth(
    const cdc_transport_envelope *envelope,
    const uint8_t key[CDC_TRANSPORT_TAG_SIZE]) {
    uint8_t digest[CDC_TRANSPORT_TAG_SIZE];
    uint8_t mac[CDC_TRANSPORT_TAG_SIZE];
    uint8_t identity[CDC_TRANSPORT_TAG_SIZE];
    if (!envelope || !key || !envelope_structurally_valid(envelope)) {
        return CDC_TRANSPORT_REJECT_ARGUMENT;
    }
    payload_digest(envelope, digest);
    if (!constant_equal(digest, envelope->payload_digest, sizeof(digest))) {
        return CDC_TRANSPORT_REJECT_PAYLOAD;
    }
    if (!compute_mac(envelope, key, mac) ||
        !constant_equal(mac, envelope->mac, sizeof(mac))) {
        return CDC_TRANSPORT_REJECT_AUTH;
    }
    if (!compute_identity(envelope, identity) ||
        !constant_equal(identity, envelope->envelope_digest,
                        sizeof(identity))) {
        return CDC_TRANSPORT_REJECT_IDENTITY;
    }
    return CDC_TRANSPORT_ACCEPT;
}

static int size_add(size_t *total, size_t amount) {
    if (amount > SIZE_MAX - *total) {
        return 0;
    }
    *total += amount;
    return 1;
}

static void put_u16(uint8_t **cursor, uint16_t value) {
    (*cursor)[0] = (uint8_t)(value >> 8);
    (*cursor)[1] = (uint8_t)value;
    *cursor += 2;
}

static void put_u32(uint8_t **cursor, uint32_t value) {
    (*cursor)[0] = (uint8_t)(value >> 24);
    (*cursor)[1] = (uint8_t)(value >> 16);
    (*cursor)[2] = (uint8_t)(value >> 8);
    (*cursor)[3] = (uint8_t)value;
    *cursor += 4;
}

static void put_u64(uint8_t **cursor, uint64_t value) {
    size_t i;
    for (i = 0; i < 8; i++) {
        (*cursor)[7 - i] = (uint8_t)(value >> (i * 8));
    }
    *cursor += 8;
}

static void put_bytes(uint8_t **cursor, const void *bytes, size_t length) {
    if (length > 0) {
        memcpy(*cursor, bytes, length);
        *cursor += length;
    }
}

static void put_string(uint8_t **cursor, const char *text) {
    size_t length = strlen(text);
    put_u16(cursor, (uint16_t)length);
    put_bytes(cursor, text, length);
}

int cdc_transport_envelope_encode(const cdc_transport_envelope *envelope,
                                  uint8_t **out_bytes, size_t *out_length) {
    const char *strings[5];
    size_t total = 4 + 4 + 4 + 4 + 8 + 8 + 8 + CDC_TRANSPORT_NONCE_SIZE +
                   CDC_TRANSPORT_TAG_SIZE + 8 + CDC_TRANSPORT_TAG_SIZE +
                   CDC_TRANSPORT_TAG_SIZE + CDC_TRANSPORT_TAG_SIZE;
    size_t i;
    uint8_t *bytes;
    uint8_t *cursor;
    if (!out_bytes || !out_length) {
        return 0;
    }
    *out_bytes = NULL;
    *out_length = 0;
    if (!envelope_structurally_valid(envelope) ||
        bytes_zero(envelope->mac, CDC_TRANSPORT_TAG_SIZE) ||
        bytes_zero(envelope->envelope_digest, CDC_TRANSPORT_TAG_SIZE)) {
        return 0;
    }
    strings[0] = envelope->sender;
    strings[1] = envelope->recipient;
    strings[2] = envelope->frame;
    strings[3] = envelope->key_id;
    strings[4] = envelope->lease_id;
    for (i = 0; i < 5; i++) {
        if (!size_add(&total, 2 + strlen(strings[i]))) {
            return 0;
        }
    }
    if (!size_add(&total, envelope->payload_length)) {
        return 0;
    }
    bytes = malloc(total);
    if (!bytes) {
        return 0;
    }
    cursor = bytes;
    put_bytes(&cursor, WIRE_MAGIC, sizeof(WIRE_MAGIC));
    put_u32(&cursor, envelope->schema_version);
    put_u32(&cursor, envelope->message_type);
    for (i = 0; i < 5; i++) {
        put_string(&cursor, strings[i]);
    }
    put_u32(&cursor, envelope->authority_action);
    put_u64(&cursor, envelope->horizon);
    put_u64(&cursor, envelope->sequence);
    put_u64(&cursor, envelope->logical_clock);
    put_bytes(&cursor, envelope->nonce, CDC_TRANSPORT_NONCE_SIZE);
    put_bytes(&cursor, envelope->causal_parent, CDC_TRANSPORT_TAG_SIZE);
    put_u64(&cursor, (uint64_t)envelope->payload_length);
    put_bytes(&cursor, envelope->payload_digest, CDC_TRANSPORT_TAG_SIZE);
    put_bytes(&cursor, envelope->payload, envelope->payload_length);
    put_bytes(&cursor, envelope->mac, CDC_TRANSPORT_TAG_SIZE);
    put_bytes(&cursor, envelope->envelope_digest, CDC_TRANSPORT_TAG_SIZE);
    if ((size_t)(cursor - bytes) != total) {
        free(bytes);
        return 0;
    }
    *out_bytes = bytes;
    *out_length = total;
    return 1;
}

typedef struct {
    const uint8_t *cursor;
    size_t remaining;
} wire_reader;

static int take_bytes(wire_reader *reader, void *out, size_t length) {
    if (length > reader->remaining) {
        return 0;
    }
    if (length > 0 && out) {
        memcpy(out, reader->cursor, length);
    }
    reader->cursor += length;
    reader->remaining -= length;
    return 1;
}

static int take_u16(wire_reader *reader, uint16_t *out) {
    uint8_t bytes[2];
    if (!take_bytes(reader, bytes, sizeof(bytes))) {
        return 0;
    }
    *out = (uint16_t)(((uint16_t)bytes[0] << 8) |
                      (uint16_t)bytes[1]);
    return 1;
}

static int take_u32(wire_reader *reader, uint32_t *out) {
    uint8_t bytes[4];
    if (!take_bytes(reader, bytes, sizeof(bytes))) {
        return 0;
    }
    *out = ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) | bytes[3];
    return 1;
}

static int take_u64(wire_reader *reader, uint64_t *out) {
    uint8_t bytes[8];
    size_t i;
    uint64_t value = 0;
    if (!take_bytes(reader, bytes, sizeof(bytes))) {
        return 0;
    }
    for (i = 0; i < 8; i++) {
        value = (value << 8) | bytes[i];
    }
    *out = value;
    return 1;
}

static int take_string(wire_reader *reader,
                       char out[CDC_TRANSPORT_ID_MAX + 1]) {
    uint16_t length;
    if (!take_u16(reader, &length) || length == 0 ||
        length > CDC_TRANSPORT_ID_MAX || length > reader->remaining ||
        memchr(reader->cursor, '\0', length) != NULL) {
        return 0;
    }
    memcpy(out, reader->cursor, length);
    out[length] = '\0';
    reader->cursor += length;
    reader->remaining -= length;
    return 1;
}

int cdc_transport_envelope_decode(const uint8_t *bytes, size_t length,
                                  size_t payload_limit,
                                  cdc_transport_envelope *out) {
    wire_reader reader;
    uint8_t magic[4];
    uint64_t payload_length;
    if (!bytes || !out || payload_limit == 0 || out->payload != NULL) {
        return 0;
    }
    cdc_transport_envelope_init(out);
    reader.cursor = bytes;
    reader.remaining = length;
    if (!take_bytes(&reader, magic, sizeof(magic)) ||
        memcmp(magic, WIRE_MAGIC, sizeof(magic)) != 0 ||
        !take_u32(&reader, &out->schema_version) ||
        !take_u32(&reader, &out->message_type) ||
        !take_string(&reader, out->sender) ||
        !take_string(&reader, out->recipient) ||
        !take_string(&reader, out->frame) ||
        !take_string(&reader, out->key_id) ||
        !take_string(&reader, out->lease_id) ||
        !take_u32(&reader, &out->authority_action) ||
        !take_u64(&reader, &out->horizon) ||
        !take_u64(&reader, &out->sequence) ||
        !take_u64(&reader, &out->logical_clock) ||
        !take_bytes(&reader, out->nonce, CDC_TRANSPORT_NONCE_SIZE) ||
        !take_bytes(&reader, out->causal_parent, CDC_TRANSPORT_TAG_SIZE) ||
        !take_u64(&reader, &payload_length) ||
        payload_length > payload_limit || payload_length > SIZE_MAX ||
        !take_bytes(&reader, out->payload_digest, CDC_TRANSPORT_TAG_SIZE) ||
        payload_length > reader.remaining ||
        !cdc_transport_envelope_set_payload(
            out, reader.cursor, (size_t)payload_length)) {
        cdc_transport_envelope_free(out);
        return 0;
    }
    reader.cursor += (size_t)payload_length;
    reader.remaining -= (size_t)payload_length;
    if (!take_bytes(&reader, out->mac, CDC_TRANSPORT_TAG_SIZE) ||
        !take_bytes(&reader, out->envelope_digest, CDC_TRANSPORT_TAG_SIZE) ||
        reader.remaining != 0 || !envelope_structurally_valid(out)) {
        cdc_transport_envelope_free(out);
        return 0;
    }
    return 1;
}

int cdc_transport_peer_init(cdc_transport_peer *peer, uint32_t schema_version,
                            const char *local_id, const char *key_id,
                            size_t payload_limit, size_t stream_limit) {
    if (!peer || schema_version == 0 || !id_valid(local_id) ||
        !id_valid(key_id) ||
        payload_limit == 0 || stream_limit == 0) {
        return 0;
    }
    memset(peer, 0, sizeof(*peer));
    peer->schema_version = schema_version;
    memcpy(peer->local_id, local_id, strlen(local_id) + 1);
    memcpy(peer->key_id, key_id, strlen(key_id) + 1);
    peer->payload_limit = payload_limit;
    peer->stream_limit = stream_limit;
    return 1;
}

void cdc_transport_peer_free(cdc_transport_peer *peer) {
    if (!peer) {
        return;
    }
    free(peer->streams);
    memset(peer, 0, sizeof(*peer));
}

void cdc_transport_peer_set_partitioned(cdc_transport_peer *peer,
                                        int partitioned) {
    if (peer) {
        peer->partitioned = partitioned != 0;
    }
}

static size_t find_stream(const cdc_transport_peer *peer, const char *sender,
                          const char *frame) {
    size_t i;
    for (i = 0; i < peer->stream_count; i++) {
        if (strcmp(peer->streams[i].sender, sender) == 0 &&
            strcmp(peer->streams[i].frame, frame) == 0) {
            return i;
        }
    }
    return SIZE_MAX;
}

cdc_transport_verdict
cdc_transport_inspect(const cdc_transport_peer *peer,
                      const cdc_transport_envelope *envelope,
                      const uint8_t key[CDC_TRANSPORT_TAG_SIZE],
                      cdc_transport_ticket *ticket) {
    uint8_t digest[CDC_TRANSPORT_TAG_SIZE];
    uint8_t mac[CDC_TRANSPORT_TAG_SIZE];
    uint8_t identity[CDC_TRANSPORT_TAG_SIZE];
    size_t index;
    if (!peer || !envelope || !key || !ticket ||
        !envelope_structurally_valid(envelope)) {
        return CDC_TRANSPORT_REJECT_ARGUMENT;
    }
    if (envelope->schema_version != peer->schema_version) {
        return CDC_TRANSPORT_REJECT_SCHEMA;
    }
    if (strcmp(envelope->key_id, peer->key_id) != 0) {
        return CDC_TRANSPORT_REJECT_KEY;
    }
    if (strcmp(envelope->recipient, peer->local_id) != 0) {
        return CDC_TRANSPORT_REJECT_RECIPIENT;
    }
    if (envelope->payload_length > peer->payload_limit) {
        return CDC_TRANSPORT_HOLD_LIMIT;
    }
    payload_digest(envelope, digest);
    if (!constant_equal(digest, envelope->payload_digest, sizeof(digest))) {
        return CDC_TRANSPORT_REJECT_PAYLOAD;
    }
    if (!compute_mac(envelope, key, mac) ||
        !constant_equal(mac, envelope->mac, sizeof(mac))) {
        return CDC_TRANSPORT_REJECT_AUTH;
    }
    if (!compute_identity(envelope, identity) ||
        !constant_equal(identity, envelope->envelope_digest,
                        sizeof(identity))) {
        return CDC_TRANSPORT_REJECT_IDENTITY;
    }
    if (peer->partitioned) {
        return CDC_TRANSPORT_HOLD_PARTITION;
    }

    index = find_stream(peer, envelope->sender, envelope->frame);
    memset(ticket, 0, sizeof(*ticket));
    memcpy(ticket->sender, envelope->sender, strlen(envelope->sender) + 1);
    memcpy(ticket->frame, envelope->frame, strlen(envelope->frame) + 1);
    ticket->sequence = envelope->sequence;
    ticket->logical_clock = envelope->logical_clock;
    memcpy(ticket->envelope_digest, envelope->envelope_digest,
           CDC_TRANSPORT_TAG_SIZE);
    if (index == SIZE_MAX) {
        if (envelope->sequence != 1) {
            return CDC_TRANSPORT_HOLD_CAUSAL_GAP;
        }
        if (!bytes_zero(envelope->causal_parent, CDC_TRANSPORT_TAG_SIZE)) {
            return CDC_TRANSPORT_HOLD_CAUSAL_PARENT;
        }
        ticket->stream_existed = 0;
        return CDC_TRANSPORT_ACCEPT;
    }

    ticket->stream_existed = 1;
    ticket->prior_sequence = peer->streams[index].last_sequence;
    ticket->prior_logical_clock = peer->streams[index].last_logical_clock;
    memcpy(ticket->prior_digest, peer->streams[index].last_digest,
           CDC_TRANSPORT_TAG_SIZE);
    if (envelope->sequence == peer->streams[index].last_sequence &&
        constant_equal(envelope->envelope_digest,
                       peer->streams[index].last_digest,
                       CDC_TRANSPORT_TAG_SIZE)) {
        return CDC_TRANSPORT_HOLD_DUPLICATE;
    }
    if (envelope->sequence <= peer->streams[index].last_sequence) {
        return CDC_TRANSPORT_REJECT_REPLAY;
    }
    if (envelope->sequence != peer->streams[index].last_sequence + 1) {
        return CDC_TRANSPORT_HOLD_CAUSAL_GAP;
    }
    if (envelope->logical_clock <=
        peer->streams[index].last_logical_clock) {
        return CDC_TRANSPORT_HOLD_CAUSAL_CLOCK;
    }
    if (!constant_equal(envelope->causal_parent,
                        peer->streams[index].last_digest,
                        CDC_TRANSPORT_TAG_SIZE)) {
        return CDC_TRANSPORT_HOLD_CAUSAL_PARENT;
    }
    return CDC_TRANSPORT_ACCEPT;
}

static int grow_streams(cdc_transport_peer *peer) {
    size_t next;
    void *grown;
    if (peer->stream_count < peer->stream_capacity) {
        return 1;
    }
    next = peer->stream_capacity ? peer->stream_capacity * 2 : 4;
    if (next > peer->stream_limit) {
        next = peer->stream_limit;
    }
    if (next <= peer->stream_capacity) {
        return 0;
    }
    grown = realloc(peer->streams, next * sizeof(*peer->streams));
    if (!grown) {
        return 0;
    }
    peer->streams = grown;
    peer->stream_capacity = next;
    return 1;
}

cdc_transport_verdict
cdc_transport_prepare_accept(cdc_transport_peer *peer,
                             const cdc_transport_ticket *ticket) {
    size_t index;
    if (!peer || !ticket || !id_valid(ticket->sender) ||
        !id_valid(ticket->frame) || ticket->sequence == 0 ||
        bytes_zero(ticket->envelope_digest, CDC_TRANSPORT_TAG_SIZE)) {
        return CDC_TRANSPORT_REJECT_ARGUMENT;
    }
    index = find_stream(peer, ticket->sender, ticket->frame);
    if (ticket->stream_existed) {
        if (index == SIZE_MAX ||
            peer->streams[index].last_sequence != ticket->prior_sequence ||
            peer->streams[index].last_logical_clock !=
                ticket->prior_logical_clock ||
            !constant_equal(peer->streams[index].last_digest,
                            ticket->prior_digest, CDC_TRANSPORT_TAG_SIZE)) {
            return CDC_TRANSPORT_REJECT_STALE_TICKET;
        }
    } else if (index != SIZE_MAX) {
        return CDC_TRANSPORT_REJECT_STALE_TICKET;
    }

    if (index == SIZE_MAX) {
        if (peer->stream_count >= peer->stream_limit || !grow_streams(peer)) {
            return CDC_TRANSPORT_HOLD_LIMIT;
        }
    }
    return CDC_TRANSPORT_ACCEPT;
}

cdc_transport_verdict
cdc_transport_accept(cdc_transport_peer *peer,
                     const cdc_transport_ticket *ticket) {
    size_t index;
    cdc_transport_verdict verdict =
        cdc_transport_prepare_accept(peer, ticket);
    if (verdict != CDC_TRANSPORT_ACCEPT) {
        return verdict;
    }
    index = find_stream(peer, ticket->sender, ticket->frame);
    if (index == SIZE_MAX) {
        index = peer->stream_count++;
        memset(&peer->streams[index], 0, sizeof(peer->streams[index]));
        memcpy(peer->streams[index].sender, ticket->sender,
               strlen(ticket->sender) + 1);
        memcpy(peer->streams[index].frame, ticket->frame,
               strlen(ticket->frame) + 1);
    }
    peer->streams[index].last_sequence = ticket->sequence;
    peer->streams[index].last_logical_clock = ticket->logical_clock;
    memcpy(peer->streams[index].last_digest, ticket->envelope_digest,
           CDC_TRANSPORT_TAG_SIZE);
    return CDC_TRANSPORT_ACCEPT;
}

const char *cdc_transport_verdict_name(cdc_transport_verdict verdict) {
    static const char *const NAMES[] = {
        "accept", "hold-partition", "hold-duplicate", "hold-causal-gap",
        "hold-causal-parent", "hold-causal-clock", "hold-limit", "reject-argument",
        "reject-schema", "reject-key", "reject-recipient", "reject-payload",
        "reject-auth", "reject-identity", "reject-replay",
        "reject-stale-ticket",
    };
    if ((size_t)verdict >= sizeof(NAMES) / sizeof(NAMES[0])) {
        return "reject-unknown";
    }
    return NAMES[verdict];
}
