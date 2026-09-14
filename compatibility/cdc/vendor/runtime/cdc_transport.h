#ifndef CDC_TRANSPORT_H
#define CDC_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

enum {
    CDC_TRANSPORT_ID_MAX = 63,
    CDC_TRANSPORT_TAG_SIZE = 32,
    CDC_TRANSPORT_NONCE_SIZE = 16
};

typedef enum {
    CDC_TRANSPORT_OBSERVATION = 1,
    CDC_TRANSPORT_PROPOSAL = 2,
    CDC_TRANSPORT_DECISION = 3,
    CDC_TRANSPORT_RECEIPT = 4
} cdc_transport_message_type;

typedef enum {
    CDC_TRANSPORT_ACCEPT = 0,
    CDC_TRANSPORT_HOLD_PARTITION,
    CDC_TRANSPORT_HOLD_DUPLICATE,
    CDC_TRANSPORT_HOLD_CAUSAL_GAP,
    CDC_TRANSPORT_HOLD_CAUSAL_PARENT,
    CDC_TRANSPORT_HOLD_CAUSAL_CLOCK,
    CDC_TRANSPORT_HOLD_LIMIT,
    CDC_TRANSPORT_REJECT_ARGUMENT,
    CDC_TRANSPORT_REJECT_SCHEMA,
    CDC_TRANSPORT_REJECT_KEY,
    CDC_TRANSPORT_REJECT_RECIPIENT,
    CDC_TRANSPORT_REJECT_PAYLOAD,
    CDC_TRANSPORT_REJECT_AUTH,
    CDC_TRANSPORT_REJECT_IDENTITY,
    CDC_TRANSPORT_REJECT_REPLAY,
    CDC_TRANSPORT_REJECT_STALE_TICKET
} cdc_transport_verdict;

typedef struct {
    uint32_t schema_version;
    uint32_t message_type;
    char sender[CDC_TRANSPORT_ID_MAX + 1];
    char recipient[CDC_TRANSPORT_ID_MAX + 1];
    char frame[CDC_TRANSPORT_ID_MAX + 1];
    char key_id[CDC_TRANSPORT_ID_MAX + 1];
    char lease_id[CDC_TRANSPORT_ID_MAX + 1];
    uint32_t authority_action;
    uint64_t horizon;
    uint64_t sequence;
    uint64_t logical_clock;
    uint8_t nonce[CDC_TRANSPORT_NONCE_SIZE];
    uint8_t causal_parent[CDC_TRANSPORT_TAG_SIZE];
    uint8_t payload_digest[CDC_TRANSPORT_TAG_SIZE];
    uint8_t *payload;
    size_t payload_length;
    uint8_t mac[CDC_TRANSPORT_TAG_SIZE];
    uint8_t envelope_digest[CDC_TRANSPORT_TAG_SIZE];
} cdc_transport_envelope;

typedef struct {
    char sender[CDC_TRANSPORT_ID_MAX + 1];
    char frame[CDC_TRANSPORT_ID_MAX + 1];
    uint64_t last_sequence;
    uint64_t last_logical_clock;
    uint8_t last_digest[CDC_TRANSPORT_TAG_SIZE];
} cdc_transport_stream;

typedef struct {
    uint32_t schema_version;
    char local_id[CDC_TRANSPORT_ID_MAX + 1];
    char key_id[CDC_TRANSPORT_ID_MAX + 1];
    size_t payload_limit;
    size_t stream_limit;
    int partitioned;
    cdc_transport_stream *streams;
    size_t stream_count;
    size_t stream_capacity;
} cdc_transport_peer;

typedef struct {
    char sender[CDC_TRANSPORT_ID_MAX + 1];
    char frame[CDC_TRANSPORT_ID_MAX + 1];
    uint64_t sequence;
    uint64_t logical_clock;
    uint64_t prior_sequence;
    uint64_t prior_logical_clock;
    uint8_t prior_digest[CDC_TRANSPORT_TAG_SIZE];
    uint8_t envelope_digest[CDC_TRANSPORT_TAG_SIZE];
    int stream_existed;
} cdc_transport_ticket;

void cdc_transport_envelope_init(cdc_transport_envelope *envelope);
void cdc_transport_envelope_free(cdc_transport_envelope *envelope);
int cdc_transport_envelope_set_payload(cdc_transport_envelope *envelope,
                                       const void *payload, size_t length);
int cdc_transport_envelope_sign(
    cdc_transport_envelope *envelope,
    const uint8_t key[CDC_TRANSPORT_TAG_SIZE]);

/* Stateless payload/MAC/identity verification for durable envelope replay.
 * This authenticates one canonical envelope but does not apply peer schema,
 * recipient, partition, sequence, parent, or logical-clock policy. */
cdc_transport_verdict
cdc_transport_envelope_verify_auth(
    const cdc_transport_envelope *envelope,
    const uint8_t key[CDC_TRANSPORT_TAG_SIZE]);

int cdc_transport_envelope_encode(const cdc_transport_envelope *envelope,
                                  uint8_t **out_bytes, size_t *out_length);
/* `out` must be initialized and empty. Decode owns any payload it installs. */
int cdc_transport_envelope_decode(const uint8_t *bytes, size_t length,
                                  size_t payload_limit,
                                  cdc_transport_envelope *out);

int cdc_transport_peer_init(cdc_transport_peer *peer, uint32_t schema_version,
                            const char *local_id, const char *key_id,
                            size_t payload_limit, size_t stream_limit);
void cdc_transport_peer_free(cdc_transport_peer *peer);
void cdc_transport_peer_set_partitioned(cdc_transport_peer *peer,
                                        int partitioned);

/* Inspect authenticates and checks causal admission without mutating peer
 * state. Accept consumes the ticket only after authority and policy agree. */
cdc_transport_verdict
cdc_transport_inspect(const cdc_transport_peer *peer,
                      const cdc_transport_envelope *envelope,
                      const uint8_t key[CDC_TRANSPORT_TAG_SIZE],
                      cdc_transport_ticket *ticket);
/* Reserve any storage needed by accept without advancing causal state. */
cdc_transport_verdict
cdc_transport_prepare_accept(cdc_transport_peer *peer,
                             const cdc_transport_ticket *ticket);
cdc_transport_verdict
cdc_transport_accept(cdc_transport_peer *peer,
                     const cdc_transport_ticket *ticket);

const char *cdc_transport_verdict_name(cdc_transport_verdict verdict);

#endif
