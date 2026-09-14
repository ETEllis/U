#include "cdc_shared_record.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    uint64_t wanted_record;
    int found;
    int malformed;
    uint64_t fragment_id;
    int value;
} fragment_scan;

static cdc_store_status scan_fragment(void *context, uint64_t event_sequence,
                                      uint64_t transaction_sequence,
                                      const void *payload,
                                      size_t payload_size) {
    fragment_scan *scan = context;
    char text[192];
    unsigned long long record_id;
    unsigned long long fragment_id;
    int value;
    char trailing;
    int fields;

    (void)event_sequence;
    (void)transaction_sequence;
    if (payload_size >= sizeof(text)) {
        return CDC_STORE_OK;
    }
    memcpy(text, payload, payload_size);
    text[payload_size] = '\0';
    fields = sscanf(text, "rftc-fragment/v1 record=%llu", &record_id);
    if (fields != 1 || (uint64_t)record_id != scan->wanted_record) {
        return CDC_STORE_OK;
    }
    fields = sscanf(text,
                    "rftc-fragment/v1 record=%llu fragment=%llu value=%d%c",
                    &record_id, &fragment_id, &value, &trailing);
    if (fields != 3 || (value != 0 && value != 1) || scan->found) {
        scan->malformed = 1;
        return CDC_STORE_ESTATE;
    }
    scan->found = 1;
    scan->fragment_id = (uint64_t)fragment_id;
    scan->value = value;
    return CDC_STORE_OK;
}

cdc_shared_record_status cdc_shared_record_publish(
    cdc_store *store, uint64_t record_id, uint64_t fragment_id, int value) {
    char payload[160];
    int length;
    cdc_store_status status;

    if (!store || (value != 0 && value != 1)) {
        return CDC_SHARED_RECORD_EARG;
    }
    length = snprintf(payload, sizeof(payload),
                      "rftc-fragment/v1 record=%llu fragment=%llu value=%d",
                      (unsigned long long)record_id,
                      (unsigned long long)fragment_id, value);
    if (length < 0 || (size_t)length >= sizeof(payload)) {
        return CDC_SHARED_RECORD_EARG;
    }
    status = cdc_store_stage(store, payload, (size_t)length);
    if (status != CDC_STORE_OK) {
        return CDC_SHARED_RECORD_ESTORE;
    }
    status = cdc_store_commit(store);
    if (status != CDC_STORE_OK) {
        (void)cdc_store_rollback(store);
        return CDC_SHARED_RECORD_ESTORE;
    }
    return CDC_SHARED_RECORD_OK;
}

cdc_shared_record_status cdc_shared_record_recover(
    cdc_store *const *stores, size_t store_count, uint64_t record_id,
    size_t quorum, cdc_shared_record_result *out) {
    uint64_t fragment_ids[64];
    size_t fragment_count = 0;
    size_t zero_votes = 0;
    size_t one_votes = 0;
    size_t i;

    if (!out || (!stores && store_count > 0) || store_count > 64 ||
        quorum == 0 || quorum > store_count) {
        return CDC_SHARED_RECORD_EARG;
    }
    memset(out, 0, sizeof(*out));
    for (i = 0; i < store_count; i++) {
        fragment_scan scan;
        cdc_store_status status;
        size_t prior;

        if (!stores[i]) {
            continue;
        }
        memset(&scan, 0, sizeof(scan));
        scan.wanted_record = record_id;
        status = cdc_store_visit_events(stores[i], scan_fragment, &scan);
        if (scan.malformed || status == CDC_STORE_ESTATE) {
            return CDC_SHARED_RECORD_ECONFLICT;
        }
        if (status != CDC_STORE_OK) {
            return CDC_SHARED_RECORD_ESTORE;
        }
        if (!scan.found) {
            continue;
        }
        for (prior = 0; prior < fragment_count; prior++) {
            if (fragment_ids[prior] == scan.fragment_id) {
                return CDC_SHARED_RECORD_ECONFLICT;
            }
        }
        fragment_ids[fragment_count++] = scan.fragment_id;
        if (scan.value == 0) {
            zero_votes++;
        } else {
            one_votes++;
        }
    }
    if (fragment_count < quorum) {
        return CDC_SHARED_RECORD_ENODATA;
    }
    if (zero_votes == one_votes) {
        return CDC_SHARED_RECORD_ECONFLICT;
    }
    out->value = one_votes > zero_votes ? 1 : 0;
    out->fragments = fragment_count;
    out->zero_votes = zero_votes;
    out->one_votes = one_votes;
    return CDC_SHARED_RECORD_OK;
}
