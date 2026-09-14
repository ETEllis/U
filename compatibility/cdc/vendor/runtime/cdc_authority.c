#include "cdc_authority.h"

#include <stdlib.h>
#include <string.h>

static int id_valid(const char *id) {
    size_t n = 0;
    if (!id) {
        return 0;
    }
    while (n <= CDC_AUTHORITY_ID_MAX && id[n] != '\0') {
        n++;
    }
    return n > 0 && n <= CDC_AUTHORITY_ID_MAX;
}

static int nonce_is_zero(const uint8_t nonce[CDC_AUTHORITY_NONCE_SIZE]) {
    uint8_t value = 0;
    size_t i;
    for (i = 0; i < CDC_AUTHORITY_NONCE_SIZE; i++) {
        value |= nonce[i];
    }
    return value == 0;
}

static int digest_is_zero(const uint8_t digest[32]) {
    uint8_t value = 0;
    size_t i;
    for (i = 0; i < 32; i++) {
        value |= digest[i];
    }
    return value == 0;
}

static size_t find_lease(const cdc_authority_guard *guard,
                         const char *lease_id) {
    size_t i;
    for (i = 0; i < guard->lease_count; i++) {
        if (strcmp(guard->leases[i].lease_id, lease_id) == 0) {
            return i;
        }
    }
    return SIZE_MAX;
}

static int nonce_seen(const cdc_authority_guard *guard, const char *lease_id,
                      const uint8_t nonce[CDC_AUTHORITY_NONCE_SIZE]) {
    size_t i;
    for (i = 0; i < guard->nonce_count; i++) {
        if (strcmp(guard->nonces[i].lease_id, lease_id) == 0 &&
            memcmp(guard->nonces[i].nonce, nonce,
                   CDC_AUTHORITY_NONCE_SIZE) == 0) {
            return 1;
        }
    }
    return 0;
}

static int grow_leases(cdc_authority_guard *guard) {
    size_t next;
    void *grown;
    if (guard->lease_count < guard->lease_capacity) {
        return 1;
    }
    next = guard->lease_capacity ? guard->lease_capacity * 2 : 4;
    if (next > guard->lease_limit) {
        next = guard->lease_limit;
    }
    if (next <= guard->lease_capacity) {
        return 0;
    }
    grown = realloc(guard->leases, next * sizeof(*guard->leases));
    if (!grown) {
        return 0;
    }
    guard->leases = grown;
    guard->lease_capacity = next;
    return 1;
}

static int grow_nonces(cdc_authority_guard *guard) {
    size_t next;
    void *grown;
    if (guard->nonce_count < guard->nonce_capacity) {
        return 1;
    }
    next = guard->nonce_capacity ? guard->nonce_capacity * 2 : 8;
    if (next > guard->nonce_limit) {
        next = guard->nonce_limit;
    }
    if (next <= guard->nonce_capacity) {
        return 0;
    }
    grown = realloc(guard->nonces, next * sizeof(*guard->nonces));
    if (!grown) {
        return 0;
    }
    guard->nonces = grown;
    guard->nonce_capacity = next;
    return 1;
}

int cdc_authority_guard_init(cdc_authority_guard *guard, size_t lease_limit,
                             size_t nonce_limit) {
    if (!guard || lease_limit == 0 || nonce_limit == 0) {
        return 0;
    }
    memset(guard, 0, sizeof(*guard));
    guard->lease_limit = lease_limit;
    guard->nonce_limit = nonce_limit;
    guard->revision = 1;
    return 1;
}

void cdc_authority_guard_free(cdc_authority_guard *guard) {
    if (!guard) {
        return;
    }
    free(guard->leases);
    free(guard->nonces);
    memset(guard, 0, sizeof(*guard));
}

cdc_authority_verdict
cdc_authority_add(cdc_authority_guard *guard,
                  const cdc_authority_lease *lease) {
    if (!guard || !lease || !id_valid(lease->lease_id) ||
        !id_valid(lease->subject) || !id_valid(lease->frame) ||
        lease->version != 1 || lease->actions == 0 ||
        (lease->actions & ~(uint32_t)(CDC_AUTH_OBSERVE | CDC_AUTH_PROPOSE |
                                      CDC_AUTH_COMMIT | CDC_AUTH_ENACT |
                                      CDC_AUTH_BRIDGE | CDC_AUTH_REVOKE)) != 0 ||
        lease->horizon_start > lease->horizon_end ||
        lease->not_before >= lease->expires_at ||
        lease->quorum_required == 0 ||
        lease->quorum_required > lease->quorum_total) {
        return CDC_AUTHORITY_REJECT_ARGUMENT;
    }
    if (find_lease(guard, lease->lease_id) != SIZE_MAX) {
        return CDC_AUTHORITY_REJECT_ARGUMENT;
    }
    if (guard->lease_count >= guard->lease_limit || !grow_leases(guard)) {
        return CDC_AUTHORITY_HOLD_LIMIT;
    }
    guard->leases[guard->lease_count++] = *lease;
    guard->revision++;
    return CDC_AUTHORITY_ACCEPT;
}

cdc_authority_verdict cdc_authority_revoke(cdc_authority_guard *guard,
                                           const char *lease_id) {
    size_t index;
    if (!guard || !id_valid(lease_id)) {
        return CDC_AUTHORITY_REJECT_ARGUMENT;
    }
    index = find_lease(guard, lease_id);
    if (index == SIZE_MAX) {
        return CDC_AUTHORITY_REJECT_MISSING;
    }
    if (!guard->leases[index].revoked) {
        guard->leases[index].revoked = 1;
        guard->revision++;
    }
    return CDC_AUTHORITY_ACCEPT;
}

cdc_authority_verdict
cdc_authority_check(const cdc_authority_guard *guard,
                    const cdc_authority_request *request,
                    cdc_authority_ticket *ticket) {
    size_t index;
    const cdc_authority_lease *lease;
    if (!guard || !request || !ticket || !id_valid(request->lease_id) ||
        !id_valid(request->subject) || !id_valid(request->frame) ||
        request->action == 0 ||
        (request->action & (request->action - 1)) != 0 ||
        request->action > CDC_AUTH_REVOKE || nonce_is_zero(request->nonce) ||
        digest_is_zero(request->proposal_digest)) {
        return CDC_AUTHORITY_REJECT_ARGUMENT;
    }
    index = find_lease(guard, request->lease_id);
    if (index == SIZE_MAX) {
        return CDC_AUTHORITY_REJECT_MISSING;
    }
    lease = &guard->leases[index];
    if (lease->revoked) {
        return CDC_AUTHORITY_REJECT_REVOKED;
    }
    if (request->now < lease->not_before) {
        return CDC_AUTHORITY_REJECT_NOT_YET_VALID;
    }
    if (request->now >= lease->expires_at) {
        return CDC_AUTHORITY_REJECT_EXPIRED;
    }
    if (strcmp(request->frame, lease->frame) != 0) {
        return CDC_AUTHORITY_REJECT_FRAME;
    }
    if (strcmp(request->subject, lease->subject) != 0) {
        return CDC_AUTHORITY_REJECT_SUBJECT;
    }
    if (request->horizon < lease->horizon_start ||
        request->horizon > lease->horizon_end) {
        return CDC_AUTHORITY_REJECT_HORIZON;
    }
    if ((lease->actions & request->action) == 0) {
        return CDC_AUTHORITY_REJECT_ACTION;
    }
    if (request->approvals < lease->quorum_required ||
        request->approvals > lease->quorum_total) {
        return CDC_AUTHORITY_REJECT_QUORUM;
    }
    if (nonce_seen(guard, lease->lease_id, request->nonce)) {
        return CDC_AUTHORITY_REJECT_REPLAY;
    }
    memset(ticket, 0, sizeof(*ticket));
    memcpy(ticket->lease_id, lease->lease_id, strlen(lease->lease_id) + 1);
    memcpy(ticket->subject, request->subject, strlen(request->subject) + 1);
    memcpy(ticket->frame, request->frame, strlen(request->frame) + 1);
    ticket->action = request->action;
    ticket->horizon = request->horizon;
    memcpy(ticket->nonce, request->nonce, CDC_AUTHORITY_NONCE_SIZE);
    memcpy(ticket->proposal_digest, request->proposal_digest,
           sizeof(ticket->proposal_digest));
    ticket->expires_at = lease->expires_at;
    ticket->revision = guard->revision;
    return CDC_AUTHORITY_ACCEPT;
}

cdc_authority_verdict
cdc_authority_prepare_consume(cdc_authority_guard *guard,
                              const cdc_authority_ticket *ticket,
                              uint64_t now) {
    size_t index;
    if (!guard || !ticket || !id_valid(ticket->lease_id) ||
        !id_valid(ticket->subject) || !id_valid(ticket->frame) ||
        ticket->action == 0 ||
        (ticket->action & (ticket->action - 1)) != 0 ||
        ticket->action > CDC_AUTH_REVOKE || nonce_is_zero(ticket->nonce) ||
        digest_is_zero(ticket->proposal_digest)) {
        return CDC_AUTHORITY_REJECT_ARGUMENT;
    }
    if (ticket->revision != guard->revision) {
        return CDC_AUTHORITY_REJECT_STALE_TICKET;
    }
    index = find_lease(guard, ticket->lease_id);
    if (index == SIZE_MAX) {
        return CDC_AUTHORITY_REJECT_MISSING;
    }
    if (guard->leases[index].revoked) {
        return CDC_AUTHORITY_REJECT_REVOKED;
    }
    if (strcmp(ticket->subject, guard->leases[index].subject) != 0) {
        return CDC_AUTHORITY_REJECT_SUBJECT;
    }
    if (strcmp(ticket->frame, guard->leases[index].frame) != 0) {
        return CDC_AUTHORITY_REJECT_FRAME;
    }
    if ((guard->leases[index].actions & ticket->action) == 0) {
        return CDC_AUTHORITY_REJECT_ACTION;
    }
    if (ticket->horizon < guard->leases[index].horizon_start ||
        ticket->horizon > guard->leases[index].horizon_end) {
        return CDC_AUTHORITY_REJECT_HORIZON;
    }
    if (ticket->expires_at != guard->leases[index].expires_at) {
        return CDC_AUTHORITY_REJECT_STALE_TICKET;
    }
    if (now >= ticket->expires_at) {
        return CDC_AUTHORITY_REJECT_EXPIRED;
    }
    if (nonce_seen(guard, ticket->lease_id, ticket->nonce)) {
        return CDC_AUTHORITY_REJECT_REPLAY;
    }
    if (guard->nonce_count >= guard->nonce_limit || !grow_nonces(guard)) {
        return CDC_AUTHORITY_HOLD_LIMIT;
    }
    return CDC_AUTHORITY_ACCEPT;
}

cdc_authority_verdict
cdc_authority_consume(cdc_authority_guard *guard,
                      const cdc_authority_ticket *ticket, uint64_t now) {
    cdc_authority_verdict verdict =
        cdc_authority_prepare_consume(guard, ticket, now);
    if (verdict != CDC_AUTHORITY_ACCEPT) {
        return verdict;
    }
    memcpy(guard->nonces[guard->nonce_count].lease_id, ticket->lease_id,
           strlen(ticket->lease_id) + 1);
    memcpy(guard->nonces[guard->nonce_count].nonce, ticket->nonce,
           CDC_AUTHORITY_NONCE_SIZE);
    guard->nonce_count++;
    return CDC_AUTHORITY_ACCEPT;
}

const char *cdc_authority_verdict_name(cdc_authority_verdict verdict) {
    static const char *const NAMES[] = {
        "accept", "hold-limit", "reject-argument", "reject-missing",
        "reject-not-yet-valid", "reject-expired", "reject-revoked",
        "reject-frame", "reject-subject", "reject-horizon", "reject-action",
        "reject-quorum", "reject-nonce", "reject-replay",
        "reject-stale-ticket",
    };
    if ((size_t)verdict >= sizeof(NAMES) / sizeof(NAMES[0])) {
        return "reject-unknown";
    }
    return NAMES[verdict];
}
