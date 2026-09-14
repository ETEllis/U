#include "cdc_rftc.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double wrap_pi(double value) {
    double wrapped = fmod(value, 2.0 * M_PI);
    if (wrapped <= -M_PI) {
        wrapped += 2.0 * M_PI;
    } else if (wrapped > M_PI) {
        wrapped -= 2.0 * M_PI;
    }
    return wrapped;
}

static void digest_u64(cdc_digest_ctx *ctx, uint64_t value) {
    uint8_t encoded[8];
    int byte_index;
    for (byte_index = 0; byte_index < 8; byte_index++) {
        encoded[byte_index] =
            (uint8_t)((value >> (8 * byte_index)) & UINT64_C(0xff));
    }
    cdc_digest_update(ctx, encoded, sizeof(encoded));
}

cdc_rftc_status cdc_rftc_reduce(const cdc_rftc_frame *frame,
                                cdc_rftc_state *out) {
    cdc_digest_ctx micro_ctx;
    cdc_digest_ctx state_ctx;
    uint8_t micro_digest[CDC_DIGEST_SIZE];
    uint8_t state_digest[CDC_DIGEST_SIZE];
    double x = 0.0;
    double y = 0.0;
    double winding_total = 0.0;
    size_t i;

    if (!frame || !out || !frame->phases || !frame->member_ids ||
        frame->member_count < 2 || frame->member_count > UINT32_MAX) {
        return CDC_RFTC_EARG;
    }
    memset(out, 0, sizeof(*out));
    cdc_digest_init(&micro_ctx);
    digest_u64(&micro_ctx, (uint64_t)frame->member_count);
    for (i = 0; i < frame->member_count; i++) {
        int64_t quantized;
        double phase;
        double next_phase;
        if (!isfinite(frame->phases[i]) ||
            !isfinite(frame->phases[(i + 1) % frame->member_count])) {
            return CDC_RFTC_EARG;
        }
        phase = wrap_pi(frame->phases[i]);
        next_phase =
            wrap_pi(frame->phases[(i + 1) % frame->member_count]);
        x += cos(phase);
        y += sin(phase);
        winding_total += wrap_pi(next_phase - phase);
        digest_u64(&micro_ctx, frame->member_ids[i]);
        quantized = (int64_t)llround(phase * 1000000000000.0);
        digest_u64(&micro_ctx, (uint64_t)quantized);
    }
    cdc_digest_final(&micro_ctx, micro_digest);

    x /= (double)frame->member_count;
    y /= (double)frame->member_count;
    out->amplitude = hypot(x, y);
    out->mean_phase = atan2(y, x);
    out->dispersion = 1.0 - out->amplitude;
    out->winding = (int)llround(winding_total / (2.0 * M_PI));
    out->member_count = frame->member_count;
    out->frame_version = frame->frame_version;
    out->topology_version = frame->topology_version;
    out->logical_clock = frame->logical_clock;
    cdc_digest_hex(micro_digest, out->microstate_digest,
                   sizeof(out->microstate_digest));

    cdc_digest_init(&state_ctx);
    cdc_digest_update(&state_ctx, micro_digest, sizeof(micro_digest));
    digest_u64(&state_ctx, frame->frame_version);
    digest_u64(&state_ctx, frame->topology_version);
    digest_u64(&state_ctx, frame->logical_clock);
    cdc_digest_final(&state_ctx, state_digest);
    cdc_digest_hex(state_digest, out->state_digest,
                   sizeof(out->state_digest));
    return CDC_RFTC_OK;
}
