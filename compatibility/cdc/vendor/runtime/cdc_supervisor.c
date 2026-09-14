#include "cdc_supervisor.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

struct cdc_supervisor {
    pthread_mutex_t mutex;
    int mutex_ready;
    cdc_transport_peer peer;
    cdc_authority_guard authority;
    uint8_t key[CDC_TRANSPORT_TAG_SIZE];
};

typedef struct {
    cdc_supervisor_commit_fn commit;
    void *context;
} commit_adapter_context;

static int transport_is_hold(cdc_transport_verdict verdict) {
    return verdict >= CDC_TRANSPORT_HOLD_PARTITION &&
           verdict <= CDC_TRANSPORT_HOLD_LIMIT;
}

static int authority_is_hold(cdc_authority_verdict verdict) {
    return verdict == CDC_AUTHORITY_HOLD_LIMIT;
}

static void secure_zero(void *memory, size_t length) {
    volatile uint8_t *bytes = memory;
    while (length > 0) {
        *bytes++ = 0;
        length--;
    }
}

static void fill_receipt(cdc_supervisor_receipt *receipt,
                         const cdc_transport_envelope *envelope) {
    memset(receipt, 0, sizeof(*receipt));
    receipt->transport_verdict = CDC_TRANSPORT_ACCEPT;
    receipt->authority_verdict = CDC_AUTHORITY_ACCEPT;
    if (!envelope) {
        return;
    }
    memcpy(receipt->sender, envelope->sender, strlen(envelope->sender) + 1);
    memcpy(receipt->frame, envelope->frame, strlen(envelope->frame) + 1);
    memcpy(receipt->lease_id, envelope->lease_id,
           strlen(envelope->lease_id) + 1);
    receipt->action = envelope->authority_action;
    receipt->horizon = envelope->horizon;
    receipt->sequence = envelope->sequence;
    memcpy(receipt->proposal_digest, envelope->payload_digest,
           sizeof(receipt->proposal_digest));
    memcpy(receipt->envelope_digest, envelope->envelope_digest,
           sizeof(receipt->envelope_digest));
}

static cdc_supervisor_application_verdict
adapt_commit(const cdc_transport_envelope *envelope,
             const cdc_supervisor_receipt *receipt, void *opaque) {
    commit_adapter_context *adapter = opaque;
    return adapter->commit(envelope, receipt, adapter->context)
               ? CDC_SUPERVISOR_APPLICATION_ACCEPT
               : CDC_SUPERVISOR_APPLICATION_HOLD;
}

cdc_supervisor *
cdc_supervisor_create(const cdc_supervisor_config *config) {
    cdc_supervisor *supervisor;
    if (!config || !config->key || config->schema_version == 0 ||
        config->payload_limit == 0 || config->stream_limit == 0 ||
        config->lease_limit == 0 || config->nonce_limit == 0) {
        return NULL;
    }
    supervisor = calloc(1, sizeof(*supervisor));
    if (!supervisor) {
        return NULL;
    }
    if (pthread_mutex_init(&supervisor->mutex, NULL) != 0) {
        free(supervisor);
        return NULL;
    }
    supervisor->mutex_ready = 1;
    if (!cdc_transport_peer_init(
            &supervisor->peer, config->schema_version, config->local_id,
            config->key_id, config->payload_limit, config->stream_limit) ||
        !cdc_authority_guard_init(&supervisor->authority, config->lease_limit,
                                  config->nonce_limit)) {
        cdc_supervisor_destroy(supervisor);
        return NULL;
    }
    memcpy(supervisor->key, config->key, sizeof(supervisor->key));
    return supervisor;
}

void cdc_supervisor_destroy(cdc_supervisor *supervisor) {
    if (!supervisor) {
        return;
    }
    cdc_authority_guard_free(&supervisor->authority);
    cdc_transport_peer_free(&supervisor->peer);
    secure_zero(supervisor->key, sizeof(supervisor->key));
    if (supervisor->mutex_ready) {
        pthread_mutex_destroy(&supervisor->mutex);
    }
    free(supervisor);
}

cdc_authority_verdict
cdc_supervisor_add_lease(cdc_supervisor *supervisor,
                         const cdc_authority_lease *lease) {
    cdc_authority_verdict verdict;
    if (!supervisor) {
        return CDC_AUTHORITY_REJECT_ARGUMENT;
    }
    pthread_mutex_lock(&supervisor->mutex);
    verdict = cdc_authority_add(&supervisor->authority, lease);
    pthread_mutex_unlock(&supervisor->mutex);
    return verdict;
}

cdc_authority_verdict
cdc_supervisor_revoke_lease(cdc_supervisor *supervisor,
                            const char *lease_id) {
    cdc_authority_verdict verdict;
    if (!supervisor) {
        return CDC_AUTHORITY_REJECT_ARGUMENT;
    }
    pthread_mutex_lock(&supervisor->mutex);
    verdict = cdc_authority_revoke(&supervisor->authority, lease_id);
    pthread_mutex_unlock(&supervisor->mutex);
    return verdict;
}

void cdc_supervisor_set_partitioned(cdc_supervisor *supervisor,
                                    int partitioned) {
    if (!supervisor) {
        return;
    }
    pthread_mutex_lock(&supervisor->mutex);
    cdc_transport_peer_set_partitioned(&supervisor->peer, partitioned);
    pthread_mutex_unlock(&supervisor->mutex);
}

cdc_supervisor_verdict
cdc_supervisor_admit_ex(cdc_supervisor *supervisor,
                        const cdc_transport_envelope *envelope, uint64_t now,
                        uint32_t verified_approvals,
                        cdc_supervisor_apply_fn apply, void *context,
                        cdc_supervisor_receipt *receipt) {
    cdc_transport_ticket transport_ticket;
    cdc_authority_request request;
    cdc_authority_ticket authority_ticket;
    cdc_transport_verdict transport_verdict;
    cdc_authority_verdict authority_verdict;
    cdc_supervisor_application_verdict application_verdict;
    cdc_supervisor_verdict verdict;

    if (!supervisor || !envelope || !apply || !receipt) {
        if (receipt) {
            memset(receipt, 0, sizeof(*receipt));
            receipt->verdict = CDC_SUPERVISOR_REJECT_INTERNAL;
        }
        return CDC_SUPERVISOR_REJECT_INTERNAL;
    }
    pthread_mutex_lock(&supervisor->mutex);
    memset(receipt, 0, sizeof(*receipt));
    receipt->transport_verdict = CDC_TRANSPORT_ACCEPT;
    receipt->authority_verdict = CDC_AUTHORITY_ACCEPT;

    transport_verdict =
        cdc_transport_inspect(&supervisor->peer, envelope, supervisor->key,
                              &transport_ticket);
    receipt->transport_verdict = transport_verdict;
    if (transport_verdict != CDC_TRANSPORT_ACCEPT) {
        verdict = transport_is_hold(transport_verdict)
                      ? CDC_SUPERVISOR_HOLD_TRANSPORT
                      : CDC_SUPERVISOR_REJECT_TRANSPORT;
        goto done;
    }
    fill_receipt(receipt, envelope);

    memset(&request, 0, sizeof(request));
    memcpy(request.lease_id, envelope->lease_id,
           strlen(envelope->lease_id) + 1);
    memcpy(request.subject, envelope->sender, strlen(envelope->sender) + 1);
    memcpy(request.frame, envelope->frame, strlen(envelope->frame) + 1);
    request.action = envelope->authority_action;
    request.horizon = envelope->horizon;
    request.now = now;
    request.approvals = verified_approvals;
    memcpy(request.nonce, envelope->nonce, sizeof(request.nonce));
    memcpy(request.proposal_digest, envelope->payload_digest,
           sizeof(request.proposal_digest));

    authority_verdict =
        cdc_authority_check(&supervisor->authority, &request,
                            &authority_ticket);
    receipt->authority_verdict = authority_verdict;
    if (authority_verdict != CDC_AUTHORITY_ACCEPT) {
        verdict = authority_is_hold(authority_verdict)
                      ? CDC_SUPERVISOR_HOLD_AUTHORITY
                      : CDC_SUPERVISOR_REJECT_AUTHORITY;
        goto done;
    }
    receipt->verified_approvals = verified_approvals;
    receipt->authority_expires_at = authority_ticket.expires_at;
    receipt->authority_revision = authority_ticket.revision;

    authority_verdict =
        cdc_authority_prepare_consume(&supervisor->authority,
                                      &authority_ticket, now);
    receipt->authority_verdict = authority_verdict;
    if (authority_verdict != CDC_AUTHORITY_ACCEPT) {
        verdict = authority_is_hold(authority_verdict)
                      ? CDC_SUPERVISOR_HOLD_AUTHORITY
                      : CDC_SUPERVISOR_REJECT_AUTHORITY;
        goto done;
    }
    transport_verdict =
        cdc_transport_prepare_accept(&supervisor->peer, &transport_ticket);
    receipt->transport_verdict = transport_verdict;
    if (transport_verdict != CDC_TRANSPORT_ACCEPT) {
        verdict = transport_is_hold(transport_verdict)
                      ? CDC_SUPERVISOR_HOLD_TRANSPORT
                      : CDC_SUPERVISOR_REJECT_TRANSPORT;
        goto done;
    }

    receipt->verdict = CDC_SUPERVISOR_ACCEPT;
    application_verdict = apply(envelope, receipt, context);
    receipt->application_verdict = application_verdict;
    if (application_verdict == CDC_SUPERVISOR_APPLICATION_HOLD) {
        verdict = CDC_SUPERVISOR_HOLD_COMMIT;
        goto done;
    }
    if (application_verdict != CDC_SUPERVISOR_APPLICATION_ACCEPT &&
        application_verdict != CDC_SUPERVISOR_APPLICATION_REJECT) {
        verdict = CDC_SUPERVISOR_REJECT_INTERNAL;
        goto done;
    }

    authority_verdict =
        cdc_authority_consume(&supervisor->authority, &authority_ticket, now);
    transport_verdict =
        cdc_transport_accept(&supervisor->peer, &transport_ticket);
    receipt->authority_verdict = authority_verdict;
    receipt->transport_verdict = transport_verdict;
    if (authority_verdict != CDC_AUTHORITY_ACCEPT ||
        transport_verdict != CDC_TRANSPORT_ACCEPT) {
        /*
         * Both paths were pre-reserved and this mutex excludes intervening
         * state changes. Reaching here is an internal invariant violation.
         */
        verdict = CDC_SUPERVISOR_REJECT_INTERNAL;
        goto done;
    }
    verdict = application_verdict == CDC_SUPERVISOR_APPLICATION_ACCEPT
                  ? CDC_SUPERVISOR_ACCEPT
                  : CDC_SUPERVISOR_REJECT_APPLICATION;

done:
    receipt->verdict = verdict;
    pthread_mutex_unlock(&supervisor->mutex);
    return verdict;
}

cdc_supervisor_verdict
cdc_supervisor_admit(cdc_supervisor *supervisor,
                     const cdc_transport_envelope *envelope, uint64_t now,
                     uint32_t verified_approvals,
                     cdc_supervisor_commit_fn commit, void *context,
                     cdc_supervisor_receipt *receipt) {
    commit_adapter_context adapter;
    if (!commit) {
        if (receipt) {
            memset(receipt, 0, sizeof(*receipt));
            receipt->verdict = CDC_SUPERVISOR_REJECT_INTERNAL;
        }
        return CDC_SUPERVISOR_REJECT_INTERNAL;
    }
    adapter.commit = commit;
    adapter.context = context;
    return cdc_supervisor_admit_ex(supervisor, envelope, now,
                                   verified_approvals, adapt_commit,
                                   &adapter, receipt);
}

const char *cdc_supervisor_verdict_name(cdc_supervisor_verdict verdict) {
    static const char *const NAMES[] = {
        "accept",          "hold-transport", "hold-authority",
        "hold-commit",     "reject-transport", "reject-authority",
        "reject-internal", "reject-application",
    };
    if ((size_t)verdict >= sizeof(NAMES) / sizeof(NAMES[0])) {
        return "reject-unknown";
    }
    return NAMES[verdict];
}

const char *cdc_supervisor_application_verdict_name(
    cdc_supervisor_application_verdict verdict) {
    static const char *const NAMES[] = {
        "not-run", "accept", "hold", "reject",
    };
    if ((size_t)verdict >= sizeof(NAMES) / sizeof(NAMES[0])) {
        return "unknown";
    }
    return NAMES[verdict];
}
