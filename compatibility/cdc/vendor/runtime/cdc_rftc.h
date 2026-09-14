#ifndef CDC_RFTC_H
#define CDC_RFTC_H

#include <stddef.h>
#include <stdint.h>

#include "cdc_digest.h"

#define CDC_RFTC_DIGEST_TEXT 72

typedef enum {
    CDC_RFTC_OK = 0,
    CDC_RFTC_EARG = 1,
    CDC_RFTC_ESTATE = 2
} cdc_rftc_status;

typedef struct {
    const double *phases;
    const uint64_t *member_ids;
    size_t member_count;
    uint64_t frame_version;
    uint64_t topology_version;
    uint64_t logical_clock;
} cdc_rftc_frame;

typedef struct {
    double amplitude;
    double mean_phase;
    double dispersion;
    int winding;
    size_t member_count;
    uint64_t frame_version;
    uint64_t topology_version;
    uint64_t logical_clock;
    char microstate_digest[CDC_RFTC_DIGEST_TEXT];
    char state_digest[CDC_RFTC_DIGEST_TEXT];
} cdc_rftc_state;

/*
 * Deterministically reduces one immutable, oriented ring snapshot into its
 * reference-frame macrostate. Member order is load-bearing topology. The
 * microstate digest preserves constituent provenance while the macrostate
 * exposes only the frame's actionable quotient.
 */
cdc_rftc_status cdc_rftc_reduce(const cdc_rftc_frame *frame,
                                cdc_rftc_state *out);

#endif
