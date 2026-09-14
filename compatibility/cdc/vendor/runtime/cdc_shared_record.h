#ifndef CDC_SHARED_RECORD_H
#define CDC_SHARED_RECORD_H

#include <stddef.h>
#include <stdint.h>

#include "cdc_store.h"

typedef enum {
    CDC_SHARED_RECORD_OK = 0,
    CDC_SHARED_RECORD_EARG = 1,
    CDC_SHARED_RECORD_ENODATA = 2,
    CDC_SHARED_RECORD_ECONFLICT = 3,
    CDC_SHARED_RECORD_ESTORE = 4
} cdc_shared_record_status;

typedef struct {
    int value;
    size_t fragments;
    size_t zero_votes;
    size_t one_votes;
} cdc_shared_record_result;

/*
 * Appends one independently addressable binary fragment through the durable
 * store transaction path. The caller chooses a separate store/authority
 * domain for each independent fragment.
 */
cdc_shared_record_status cdc_shared_record_publish(
    cdc_store *store, uint64_t record_id, uint64_t fragment_id, int value);

/*
 * Recovers one record only from sealed fragment stores. NULL entries are
 * missing fragments. A producer-local label is deliberately not an input.
 * Duplicate fragment identifiers, contradictory copies in one store, tied
 * votes, or malformed matching records fail closed.
 */
cdc_shared_record_status cdc_shared_record_recover(
    cdc_store *const *stores, size_t store_count, uint64_t record_id,
    size_t quorum, cdc_shared_record_result *out);

#endif
