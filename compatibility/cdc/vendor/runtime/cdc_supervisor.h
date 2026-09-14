#ifndef CDC_SUPERVISOR_H
#define CDC_SUPERVISOR_H

#include "cdc_authority.h"
#include "cdc_transport.h"

#include <stddef.h>
#include <stdint.h>

typedef struct cdc_supervisor cdc_supervisor;

typedef enum {
    CDC_SUPERVISOR_ACCEPT = 0,
    CDC_SUPERVISOR_HOLD_TRANSPORT,
    CDC_SUPERVISOR_HOLD_AUTHORITY,
    CDC_SUPERVISOR_HOLD_COMMIT,
    CDC_SUPERVISOR_REJECT_TRANSPORT,
    CDC_SUPERVISOR_REJECT_AUTHORITY,
    CDC_SUPERVISOR_REJECT_INTERNAL,
    CDC_SUPERVISOR_REJECT_APPLICATION
} cdc_supervisor_verdict;

typedef enum {
    CDC_SUPERVISOR_APPLICATION_NOT_RUN = 0,
    CDC_SUPERVISOR_APPLICATION_ACCEPT,
    CDC_SUPERVISOR_APPLICATION_HOLD,
    CDC_SUPERVISOR_APPLICATION_REJECT
} cdc_supervisor_application_verdict;

typedef struct {
    uint32_t schema_version;
    const char *local_id;
    const char *key_id;
    const uint8_t *key;
    size_t payload_limit;
    size_t stream_limit;
    size_t lease_limit;
    size_t nonce_limit;
} cdc_supervisor_config;

typedef struct {
    cdc_supervisor_verdict verdict;
    cdc_transport_verdict transport_verdict;
    cdc_authority_verdict authority_verdict;
    cdc_supervisor_application_verdict application_verdict;
    char sender[CDC_TRANSPORT_ID_MAX + 1];
    char frame[CDC_TRANSPORT_ID_MAX + 1];
    char lease_id[CDC_TRANSPORT_ID_MAX + 1];
    uint32_t action;
    uint32_t verified_approvals;
    uint64_t horizon;
    uint64_t sequence;
    uint64_t authority_expires_at;
    uint64_t authority_revision;
    uint8_t proposal_digest[CDC_TRANSPORT_TAG_SIZE];
    uint8_t envelope_digest[CDC_TRANSPORT_TAG_SIZE];
} cdc_supervisor_receipt;

/*
 * The commit callback runs only after authentication, causal ordering,
 * authority, quorum, and resource reservation succeed. It executes while the
 * supervisor lock is held, must not re-enter this supervisor, and must be
 * all-or-nothing: returning zero must leave application state unchanged.
 * Returning one commits application state; the pre-reserved authority nonce
 * and causal cursor are then consumed without an allocation path.
 */
typedef int (*cdc_supervisor_commit_fn)(
    const cdc_transport_envelope *envelope,
    const cdc_supervisor_receipt *receipt, void *context);

/*
 * Typed application admission. ACCEPT may commit an all-or-nothing mutation.
 * HOLD must leave application state unchanged and keeps the authority nonce
 * and causal cursor retryable. REJECT must also leave application state
 * unchanged, but terminally consumes the already-reserved nonce and causal
 * position so authenticated malformed input cannot poison its stream.
 */
typedef cdc_supervisor_application_verdict
(*cdc_supervisor_apply_fn)(const cdc_transport_envelope *envelope,
                           const cdc_supervisor_receipt *receipt,
                           void *context);

cdc_supervisor *
cdc_supervisor_create(const cdc_supervisor_config *config);
void cdc_supervisor_destroy(cdc_supervisor *supervisor);

cdc_authority_verdict
cdc_supervisor_add_lease(cdc_supervisor *supervisor,
                         const cdc_authority_lease *lease);
cdc_authority_verdict
cdc_supervisor_revoke_lease(cdc_supervisor *supervisor,
                            const char *lease_id);
void cdc_supervisor_set_partitioned(cdc_supervisor *supervisor,
                                    int partitioned);

/*
 * verified_approvals is local evidence supplied by the supervisor's quorum
 * verifier. It is never trusted from the remote envelope.
 */
cdc_supervisor_verdict
cdc_supervisor_admit(cdc_supervisor *supervisor,
                     const cdc_transport_envelope *envelope, uint64_t now,
                     uint32_t verified_approvals,
                     cdc_supervisor_commit_fn commit, void *context,
                     cdc_supervisor_receipt *receipt);

cdc_supervisor_verdict
cdc_supervisor_admit_ex(cdc_supervisor *supervisor,
                        const cdc_transport_envelope *envelope, uint64_t now,
                        uint32_t verified_approvals,
                        cdc_supervisor_apply_fn apply, void *context,
                        cdc_supervisor_receipt *receipt);

const char *cdc_supervisor_verdict_name(cdc_supervisor_verdict verdict);
const char *cdc_supervisor_application_verdict_name(
    cdc_supervisor_application_verdict verdict);

#endif
