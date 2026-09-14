#ifndef CDC_AUTHORITY_H
#define CDC_AUTHORITY_H

#include <stddef.h>
#include <stdint.h>

enum {
    CDC_AUTHORITY_ID_MAX = 63,
    CDC_AUTHORITY_NONCE_SIZE = 16
};

typedef enum {
    CDC_AUTH_OBSERVE = 1u << 0,
    CDC_AUTH_PROPOSE = 1u << 1,
    CDC_AUTH_COMMIT = 1u << 2,
    CDC_AUTH_ENACT = 1u << 3,
    CDC_AUTH_BRIDGE = 1u << 4,
    CDC_AUTH_REVOKE = 1u << 5
} cdc_authority_action;

typedef enum {
    CDC_AUTHORITY_ACCEPT = 0,
    CDC_AUTHORITY_HOLD_LIMIT,
    CDC_AUTHORITY_REJECT_ARGUMENT,
    CDC_AUTHORITY_REJECT_MISSING,
    CDC_AUTHORITY_REJECT_NOT_YET_VALID,
    CDC_AUTHORITY_REJECT_EXPIRED,
    CDC_AUTHORITY_REJECT_REVOKED,
    CDC_AUTHORITY_REJECT_FRAME,
    CDC_AUTHORITY_REJECT_SUBJECT,
    CDC_AUTHORITY_REJECT_HORIZON,
    CDC_AUTHORITY_REJECT_ACTION,
    CDC_AUTHORITY_REJECT_QUORUM,
    CDC_AUTHORITY_REJECT_NONCE,
    CDC_AUTHORITY_REJECT_REPLAY,
    CDC_AUTHORITY_REJECT_STALE_TICKET
} cdc_authority_verdict;

typedef struct {
    uint32_t version;
    char lease_id[CDC_AUTHORITY_ID_MAX + 1];
    char subject[CDC_AUTHORITY_ID_MAX + 1];
    char frame[CDC_AUTHORITY_ID_MAX + 1];
    uint32_t actions;
    uint64_t horizon_start;
    uint64_t horizon_end;
    uint64_t not_before;
    uint64_t expires_at;
    uint32_t quorum_required;
    uint32_t quorum_total;
    int revoked;
} cdc_authority_lease;

typedef struct {
    char lease_id[CDC_AUTHORITY_ID_MAX + 1];
    char subject[CDC_AUTHORITY_ID_MAX + 1];
    char frame[CDC_AUTHORITY_ID_MAX + 1];
    uint32_t action;
    uint64_t horizon;
    uint64_t now;
    uint32_t approvals;
    uint8_t nonce[CDC_AUTHORITY_NONCE_SIZE];
    uint8_t proposal_digest[32];
} cdc_authority_request;

typedef struct {
    char lease_id[CDC_AUTHORITY_ID_MAX + 1];
    uint8_t nonce[CDC_AUTHORITY_NONCE_SIZE];
} cdc_authority_nonce_record;

typedef struct {
    cdc_authority_lease *leases;
    size_t lease_count;
    size_t lease_capacity;
    size_t lease_limit;
    cdc_authority_nonce_record *nonces;
    size_t nonce_count;
    size_t nonce_capacity;
    size_t nonce_limit;
    uint64_t revision;
} cdc_authority_guard;

typedef struct {
    char lease_id[CDC_AUTHORITY_ID_MAX + 1];
    char subject[CDC_AUTHORITY_ID_MAX + 1];
    char frame[CDC_AUTHORITY_ID_MAX + 1];
    uint32_t action;
    uint64_t horizon;
    uint8_t nonce[CDC_AUTHORITY_NONCE_SIZE];
    uint8_t proposal_digest[32];
    uint64_t expires_at;
    uint64_t revision;
} cdc_authority_ticket;

int cdc_authority_guard_init(cdc_authority_guard *guard, size_t lease_limit,
                             size_t nonce_limit);
void cdc_authority_guard_free(cdc_authority_guard *guard);

cdc_authority_verdict
cdc_authority_add(cdc_authority_guard *guard,
                  const cdc_authority_lease *lease);
cdc_authority_verdict cdc_authority_revoke(cdc_authority_guard *guard,
                                           const char *lease_id);

/* Check is side-effect free. A successful ticket must be consumed before the
 * protected proposal mutates state. The guard is intentionally single-owner;
 * a supervisor serializes check/consume with transport accept. */
cdc_authority_verdict
cdc_authority_check(const cdc_authority_guard *guard,
                    const cdc_authority_request *request,
                    cdc_authority_ticket *ticket);
/* Reserve any storage needed by consume without consuming the ticket. */
cdc_authority_verdict
cdc_authority_prepare_consume(cdc_authority_guard *guard,
                              const cdc_authority_ticket *ticket,
                              uint64_t now);
cdc_authority_verdict
cdc_authority_consume(cdc_authority_guard *guard,
                      const cdc_authority_ticket *ticket, uint64_t now);

const char *cdc_authority_verdict_name(cdc_authority_verdict verdict);

#endif
