#include "cdc_frame.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const uint8_t FRAME_DOMAIN[] = "CDC-RFTC-FRAME-V1";

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

static int digest_is_zero(const uint8_t digest[CDC_DIGEST_SIZE]) {
    uint8_t value = 0;
    size_t i;
    for (i = 0; i < CDC_DIGEST_SIZE; i++) {
        value |= digest[i];
    }
    return value == 0;
}

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
    size_t i;
    for (i = 0; i < sizeof(encoded); i++) {
        encoded[sizeof(encoded) - 1 - i] = (uint8_t)(value >> (8 * i));
    }
    cdc_digest_update(ctx, encoded, sizeof(encoded));
}

static void digest_string(cdc_digest_ctx *ctx, const char *text) {
    uint8_t length = (uint8_t)strlen(text);
    cdc_digest_update(ctx, &length, sizeof(length));
    cdc_digest_update(ctx, text, length);
}

static size_t find_member(const cdc_frame_observation *observations,
                          size_t count, uint64_t member_id) {
    size_t i;
    for (i = 0; i < count; i++) {
        if (observations[i].member_id == member_id) {
            return i;
        }
    }
    return SIZE_MAX;
}

void cdc_frame_snapshot_init(cdc_frame_snapshot *snapshot) {
    if (snapshot) {
        memset(snapshot, 0, sizeof(*snapshot));
    }
}

void cdc_frame_snapshot_free(cdc_frame_snapshot *snapshot) {
    if (!snapshot) {
        return;
    }
    free(snapshot->observations);
    memset(snapshot, 0, sizeof(*snapshot));
}

cdc_frame_status
cdc_frame_seal(const cdc_frame_config *config,
               const cdc_frame_observation *observations, size_t count,
               uint64_t now, cdc_frame_snapshot *out) {
    cdc_frame_observation *ordered = NULL;
    uint8_t *used = NULL;
    cdc_digest_ctx digest;
    uint8_t raw_digest[CDC_DIGEST_SIZE];
    uint64_t oldest;
    uint64_t logical_clock;
    size_t anchor = 0;
    size_t current;
    size_t i;

    if (!config || !observations || !out || out->observations ||
        bounded_length(config->frame_id, CDC_FRAME_ID_MAX) == 0 ||
        bounded_length(config->frame_id, CDC_FRAME_ID_MAX) >
            CDC_FRAME_ID_MAX ||
        config->frame_version == 0 || config->reducer_version == 0 ||
        config->topology_version == 0 ||
        config->window_start > config->window_end ||
        config->minimum_members < 2 ||
        config->minimum_members > config->maximum_members) {
        return CDC_FRAME_EARG;
    }
    if (count < config->minimum_members ||
        count > config->maximum_members) {
        return CDC_FRAME_ELIMIT;
    }
    if (now < config->window_end) {
        return CDC_FRAME_EWINDOW;
    }

    oldest = observations[0].observed_at;
    logical_clock = observations[0].logical_clock;
    for (i = 0; i < count; i++) {
        size_t j;
        if (observations[i].member_id == 0 ||
            observations[i].successor_id == 0 ||
            !isfinite(observations[i].phase) ||
            observations[i].observed_at < config->window_start ||
            observations[i].observed_at > config->window_end ||
            observations[i].logical_clock == 0 ||
            digest_is_zero(observations[i].source_digest)) {
            return CDC_FRAME_EWINDOW;
        }
        if (observations[i].logical_clock != logical_clock) {
            return CDC_FRAME_ECLOCK;
        }
        if (observations[i].member_id < observations[anchor].member_id) {
            anchor = i;
        }
        if (observations[i].observed_at < oldest) {
            oldest = observations[i].observed_at;
        }
        for (j = i + 1; j < count; j++) {
            if (observations[i].member_id == observations[j].member_id) {
                return CDC_FRAME_EDUPLICATE;
            }
        }
    }
    if (now - oldest > config->stale_after) {
        return CDC_FRAME_HOLD_STALE;
    }

    ordered = calloc(count, sizeof(*ordered));
    used = calloc(count, sizeof(*used));
    if (!ordered || !used) {
        free(ordered);
        free(used);
        return CDC_FRAME_ELIMIT;
    }
    current = anchor;
    for (i = 0; i < count; i++) {
        size_t next;
        if (used[current]) {
            free(ordered);
            free(used);
            return CDC_FRAME_ETOPOLOGY;
        }
        used[current] = 1;
        ordered[i] = observations[current];
        ordered[i].phase = wrap_pi(ordered[i].phase);
        next = find_member(observations, count,
                           observations[current].successor_id);
        if (next == SIZE_MAX) {
            free(ordered);
            free(used);
            return CDC_FRAME_ETOPOLOGY;
        }
        current = next;
    }
    if (current != anchor) {
        free(ordered);
        free(used);
        return CDC_FRAME_ETOPOLOGY;
    }
    free(used);

    cdc_digest_init(&digest);
    cdc_digest_update(&digest, FRAME_DOMAIN, sizeof(FRAME_DOMAIN));
    digest_string(&digest, config->frame_id);
    digest_u64(&digest, config->frame_version);
    digest_u64(&digest, config->reducer_version);
    digest_u64(&digest, config->topology_version);
    digest_u64(&digest, config->window_start);
    digest_u64(&digest, config->window_end);
    digest_u64(&digest, logical_clock);
    digest_u64(&digest, now - oldest);
    digest_u64(&digest, (uint64_t)count);
    for (i = 0; i < count; i++) {
        int64_t quantized =
            (int64_t)llround(ordered[i].phase * 1000000000000.0);
        digest_u64(&digest, ordered[i].member_id);
        digest_u64(&digest, ordered[i].successor_id);
        digest_u64(&digest, (uint64_t)quantized);
        digest_u64(&digest, ordered[i].observed_at);
        digest_u64(&digest, ordered[i].logical_clock);
        cdc_digest_update(&digest, ordered[i].source_digest,
                          CDC_DIGEST_SIZE);
    }
    cdc_digest_final(&digest, raw_digest);

    memset(out, 0, sizeof(*out));
    memcpy(out->frame_id, config->frame_id, strlen(config->frame_id) + 1);
    out->frame_version = config->frame_version;
    out->reducer_version = config->reducer_version;
    out->topology_version = config->topology_version;
    out->window_start = config->window_start;
    out->window_end = config->window_end;
    out->logical_clock = logical_clock;
    out->freshness = now - oldest;
    out->observations = ordered;
    out->member_count = count;
    memcpy(out->snapshot_digest, raw_digest, sizeof(out->snapshot_digest));
    return CDC_FRAME_OK;
}

const char *cdc_frame_status_name(cdc_frame_status status) {
    static const char *const NAMES[] = {
        "ok",          "hold-stale", "argument", "limit",
        "duplicate",   "topology",   "window",     "clock",
    };
    if ((size_t)status >= sizeof(NAMES) / sizeof(NAMES[0])) {
        return "unknown";
    }
    return NAMES[status];
}
