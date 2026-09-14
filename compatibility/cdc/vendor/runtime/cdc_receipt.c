#define _POSIX_C_SOURCE 200809L

#include "cdc_receipt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cdc_digest.h"

void cdc_receipt_init(cdc_receipt *receipt) {
    if (!receipt) {
        return;
    }
    memset(receipt, 0, sizeof(*receipt));
    receipt->version = CDC_RECEIPT_VERSION;
    receipt->outcome = CDC_OUTCOME_NONE;
    receipt->declared_hold = 0;
    receipt->durable = CDC_RECEIPT_NA;
    receipt->replay_stable = CDC_RECEIPT_NA;
    receipt->sealed = CDC_RECEIPT_NA;
    receipt->events = CDC_RECEIPT_NA;
    receipt->generation = CDC_RECEIPT_NA;
}

const char *cdc_outcome_token(cdc_outcome outcome) {
    switch (outcome) {
    case CDC_OUTCOME_ACCEPTED:
        return "+1";
    case CDC_OUTCOME_HELD:
        return "0";
    case CDC_OUTCOME_VIOLATED:
        return "-1";
    default:
        return "none";
    }
}

cdc_outcome cdc_outcome_from_token(const char *token) {
    if (!token) {
        return CDC_OUTCOME_NONE;
    }
    if (strcmp(token, "+1") == 0) {
        return CDC_OUTCOME_ACCEPTED;
    }
    if (strcmp(token, "0") == 0) {
        return CDC_OUTCOME_HELD;
    }
    if (strcmp(token, "-1") == 0) {
        return CDC_OUTCOME_VIOLATED;
    }
    return CDC_OUTCOME_NONE;
}

void *cdc_receipt_open_env(void) {
    const char *path = getenv("CDC_RECEIPTS");
    if (!path || path[0] == '\0') {
        return NULL;
    }
    return fopen(path, "w");
}

void cdc_receipt_close(void *stream) {
    if (stream) {
        fclose((FILE *)stream);
    }
}

/* A token is an identifier: printable, no spaces, no '=' . The closed
 * vocabulary means a value that fails this is a bug in the producer, not
 * anything a .cdc source can express, so emit refuses rather than quoting
 * its way around it. */
static int is_token(const char *value) {
    size_t i;
    if (!value) {
        return 0;
    }
    for (i = 0; value[i]; i++) {
        unsigned char c = (unsigned char)value[i];
        if (c <= ' ' || c > '~' || c == '=') {
            return 0;
        }
    }
    return 1;
}

int cdc_receipt_emit(void *stream, const cdc_receipt *receipt) {
    FILE *fp = (FILE *)stream;
    if (!fp) {
        return 0; /* receipts disabled */
    }
    if (!receipt || receipt->version != CDC_RECEIPT_VERSION) {
        return -1;
    }
    if (!is_token(receipt->kind) || receipt->kind[0] == '\0' ||
        !is_token(receipt->job) || receipt->job[0] == '\0' ||
        !is_token(receipt->reason) || receipt->reason[0] == '\0') {
        return -1;
    }
    if (receipt->op[0] && !is_token(receipt->op)) {
        return -1;
    }
    if (receipt->trits[0] && !is_token(receipt->trits)) {
        return -1;
    }
    if (receipt->balance[0] && !is_token(receipt->balance)) {
        return -1;
    }
    if (receipt->witness[0] &&
        (!is_token(receipt->witness) || !is_token(receipt->closure) ||
         receipt->closure[0] == '\0')) {
        return -1; /* a witness without its digest is not a link */
    }
    if (receipt->outcome == CDC_OUTCOME_NONE) {
        return -1; /* a receipt records an outcome; absence is not one */
    }

    fprintf(fp, "cdc-receipt v=%d kind=%s job=%s", receipt->version,
            receipt->kind, receipt->job);
    if (receipt->op[0]) {
        fprintf(fp, " op=%s", receipt->op);
    }
    fprintf(fp, " outcome=%s reason=%s declared-hold=%d",
            cdc_outcome_token(receipt->outcome), receipt->reason,
            receipt->declared_hold ? 1 : 0);
    if (receipt->trits[0]) {
        fprintf(fp, " trits=%s", receipt->trits);
    }
    if (receipt->balance[0]) {
        fprintf(fp, " balance=%s", receipt->balance);
    }
    if (receipt->witness[0]) {
        fprintf(fp, " witness=%s closure=%s", receipt->witness,
                receipt->closure);
    }
    if (receipt->durable != CDC_RECEIPT_NA) {
        fprintf(fp, " durable=%d", receipt->durable ? 1 : 0);
    }
    if (receipt->replay_stable != CDC_RECEIPT_NA) {
        fprintf(fp, " replay-stable=%d", receipt->replay_stable ? 1 : 0);
    }
    if (receipt->sealed != CDC_RECEIPT_NA) {
        fprintf(fp, " sealed=%ld", receipt->sealed);
    }
    if (receipt->events != CDC_RECEIPT_NA) {
        fprintf(fp, " events=%ld", receipt->events);
    }
    if (receipt->generation != CDC_RECEIPT_NA) {
        fprintf(fp, " generation=%ld", receipt->generation);
    }
    fputc('\n', fp);
    fflush(fp);
    return 1;
}

/* Copies the value of `key` from a whitespace-separated key=value line.
 * Returns 1 when present. */
static int field(const char *line, const char *key, char *out,
                 size_t out_size) {
    size_t key_len = strlen(key);
    const char *cursor = line;
    while ((cursor = strstr(cursor, key)) != NULL) {
        const char *value;
        size_t len;
        /* must start a token and be followed by '=' */
        if (cursor != line && cursor[-1] != ' ') {
            cursor += key_len;
            continue;
        }
        if (cursor[key_len] != '=') {
            cursor += key_len;
            continue;
        }
        value = cursor + key_len + 1;
        len = strcspn(value, " \t\r\n");
        if (len + 1 > out_size) {
            return 0;
        }
        memcpy(out, value, len);
        out[len] = '\0';
        return 1;
    }
    return 0;
}

static int int_field(const char *line, const char *key, long *out) {
    char buffer[32];
    char *end;
    long value;
    if (!field(line, key, buffer, sizeof(buffer)) || buffer[0] == '\0') {
        return 0;
    }
    value = strtol(buffer, &end, 10);
    if (*end != '\0') {
        return -1;
    }
    *out = value;
    return 1;
}

int cdc_receipt_parse(const char *line, cdc_receipt *receipt) {
    char buffer[64];
    long value;
    int got;

    if (!line || !receipt) {
        return -1;
    }
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    if (strncmp(line, "cdc-receipt ", 12) != 0) {
        return 0; /* not a receipt line */
    }
    cdc_receipt_init(receipt);

    got = int_field(line, "v", &value);
    if (got != 1 || value != CDC_RECEIPT_VERSION) {
        return -1; /* unknown version fails closed */
    }
    receipt->version = (int)value;

    if (!field(line, "kind", receipt->kind, sizeof(receipt->kind)) ||
        !field(line, "job", receipt->job, sizeof(receipt->job)) ||
        !field(line, "reason", receipt->reason, sizeof(receipt->reason))) {
        return -1;
    }
    if (!field(line, "outcome", buffer, sizeof(buffer))) {
        return -1;
    }
    receipt->outcome = cdc_outcome_from_token(buffer);
    if (receipt->outcome == CDC_OUTCOME_NONE) {
        return -1;
    }
    got = int_field(line, "declared-hold", &value);
    if (got != 1 || (value != 0 && value != 1)) {
        return -1;
    }
    receipt->declared_hold = (int)value;

    field(line, "op", receipt->op, sizeof(receipt->op));
    field(line, "trits", receipt->trits, sizeof(receipt->trits));
    field(line, "balance", receipt->balance, sizeof(receipt->balance));
    field(line, "witness", receipt->witness, sizeof(receipt->witness));
    field(line, "closure", receipt->closure, sizeof(receipt->closure));
    if (int_field(line, "durable", &value) == 1) {
        receipt->durable = (int)value;
    }
    if (int_field(line, "replay-stable", &value) == 1) {
        receipt->replay_stable = (int)value;
    }
    if (int_field(line, "sealed", &value) == 1) {
        receipt->sealed = value;
    }
    if (int_field(line, "events", &value) == 1) {
        receipt->events = value;
    }
    if (int_field(line, "generation", &value) == 1) {
        receipt->generation = value;
    }
    return 1;
}


/* ---- parity vectors --------------------------------------------------- */

void cdc_vector_chain_init(cdc_vector_chain *chain) {
    if (chain) {
        memset(chain, 0, sizeof(*chain));
    }
}

const char *cdc_receipt_decision(const cdc_receipt *receipt) {
    if (!receipt) {
        return "fail";
    }
    if (strcmp(receipt->kind, "nest") == 0) {
        return "nest";
    }
    switch (receipt->outcome) {
    case CDC_OUTCOME_ACCEPTED:
        return "commit";
    case CDC_OUTCOME_HELD:
        return "hold";
    default:
        return "fail";
    }
}

int cdc_vector_render(cdc_vector_chain *chain, const char *identifier,
                      const char *decision, const char *coordinate,
                      const void *effects, size_t effects_size,
                      const char *closure, char *out, size_t out_size) {
    uint8_t effects_digest[CDC_DIGEST_SIZE];
    char effects_hex[96];
    char trace_hex[96];
    cdc_digest_ctx ctx;
    uint8_t next[CDC_DIGEST_SIZE];
    int written;

    if (!chain || !identifier || !decision || !out) {
        return -1;
    }
    cdc_digest(effects, effects_size, effects_digest);
    cdc_digest_hex(effects_digest, effects_hex, sizeof(effects_hex));

    /* trace_i = digest(trace_{i-1} || effects_i): a record that changes
     * position changes its trace digest and every later one, so the
     * comparison catches reordering, not just substitution. */
    cdc_digest_init(&ctx);
    cdc_digest_update(&ctx, chain->chain, CDC_DIGEST_SIZE);
    cdc_digest_update(&ctx, effects_digest, CDC_DIGEST_SIZE);
    cdc_digest_final(&ctx, next);
    memcpy(chain->chain, next, CDC_DIGEST_SIZE);
    chain->started = 1;
    cdc_digest_hex(chain->chain, trace_hex, sizeof(trace_hex));

    written = snprintf(out, out_size, "%s %s %s %s %s %s", identifier,
                       decision, coordinate && coordinate[0] ? coordinate : "-",
                       effects_hex, trace_hex,
                       closure && closure[0] ? closure : "-");
    if (written < 0 || (size_t)written >= out_size) {
        return -1;
    }
    return written;
}
