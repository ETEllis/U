/* cdc_frontend_check: grammar-1 frontend differential harness (gate CT1).
 *
 * Modes:
 *   dump <files...>        emit grammar-0-equivalent declaration records;
 *                          byte-compared against `cdc_boot.py --dump`
 *   canon <files...>       emit canonical grammar-1 serialization
 *   roundtrip <files...>   parse -> canonical -> reparse -> structural equal
 *   bounds                 adversarial corpus: typed diagnostics, no crash
 *   oom <file>             allocator-failure injection at every allocation
 *   reject <files...>      every file must produce >=1 error diagnostic
 */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cdc_abi.h"
#include "cdc_ast.h"
#include "cdc_diagnostic.h"
#include "cdc_digest.h"
#include "cdc_lexer.h"
#include "cdc_parser.h"
#include "cdc_receipt.h"
#include "cdc_source.h"
#include "cdc_store.h"

static const char *base_name(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static int parse_or_report(const char *path, cdc_unit *program,
                           cdc_diag_list *diags) {
    if (!cdc_unit_parse_file(path, program, diags)) {
        fprintf(stderr, "cdc-frontend: %s: read or allocation failure\n",
                path);
        return 0;
    }
    if (diags->errors > 0) {
        cdc_diag_list_print(diags, stderr);
        return 0;
    }
    return 1;
}

/* ---- dump ---------------------------------------------------------- */

static void dump_stmt(const cdc_unit *program, const cdc_stmt *stmt) {
    const char *file = base_name(program->file);
    size_t i, j;
    if (stmt->kind == CDC_STMT_END) {
        return; /* grammar 0 skips structural end lines before dispatch */
    }
    printf("%s:%d|%s|", file, stmt->line, cdc_stmt_directive(stmt));
    if (stmt->kind == CDC_STMT_EXPECT) {
        for (i = 1; i < stmt->token_count; i++) {
            if (i > 1) {
                putchar(' ');
            }
            fputs(stmt->tokens[i].text, stdout);
        }
        printf("|\n");
        return;
    }
    {
        int first = 1;
        for (i = 1; i < stmt->token_count; i++) {
            if (!memchr(stmt->tokens[i].text, '=', stmt->tokens[i].length)) {
                if (!first) {
                    putchar(',');
                }
                fputs(stmt->tokens[i].text, stdout);
                first = 0;
            }
        }
    }
    putchar('|');
    {
        /* dict semantics: first-occurrence order, last value wins */
        int first = 1;
        for (i = 1; i < stmt->token_count; i++) {
            const char *eq =
                memchr(stmt->tokens[i].text, '=', stmt->tokens[i].length);
            size_t key_len;
            int seen_before = 0;
            if (!eq) {
                continue;
            }
            key_len = (size_t)(eq - stmt->tokens[i].text);
            for (j = 1; j < i; j++) {
                const char *prior_eq = memchr(stmt->tokens[j].text, '=',
                                              stmt->tokens[j].length);
                if (prior_eq &&
                    (size_t)(prior_eq - stmt->tokens[j].text) == key_len &&
                    strncmp(stmt->tokens[j].text, stmt->tokens[i].text,
                            key_len) == 0) {
                    seen_before = 1;
                    break;
                }
            }
            if (seen_before) {
                continue;
            }
            {
                char key[256];
                const char *value;
                if (key_len >= sizeof(key)) {
                    fprintf(stderr, "cdc-frontend: attribute key too long\n");
                    exit(1);
                }
                memcpy(key, stmt->tokens[i].text, key_len);
                key[key_len] = '\0';
                value = cdc_stmt_attr(stmt, key);
                if (!first) {
                    putchar(';');
                }
                printf("%s=%s", key, value ? value : "");
                first = 0;
            }
        }
    }
    printf("|\n");
}

static int cmd_dump(int argc, char **argv) {
    int i;
    for (i = 0; i < argc; i++) {
        cdc_unit program;
        cdc_diag_list diags;
        size_t s;
        cdc_diag_list_init(&diags);
        if (!parse_or_report(argv[i], &program, &diags)) {
            cdc_unit_free(&program);
            cdc_diag_list_free(&diags);
            return 1;
        }
        for (s = 0; s < program.count; s++) {
            dump_stmt(&program, &program.stmts[s]);
        }
        cdc_unit_free(&program);
        cdc_diag_list_free(&diags);
    }
    return 0;
}

/* ---- canon / roundtrip --------------------------------------------- */

static int cmd_canon(int argc, char **argv) {
    int i;
    for (i = 0; i < argc; i++) {
        cdc_unit program;
        cdc_diag_list diags;
        cdc_diag_list_init(&diags);
        if (!parse_or_report(argv[i], &program, &diags)) {
            cdc_unit_free(&program);
            cdc_diag_list_free(&diags);
            return 1;
        }
        cdc_unit_canonical(&program, stdout);
        cdc_unit_free(&program);
        cdc_diag_list_free(&diags);
    }
    return 0;
}

static int cmd_roundtrip(int argc, char **argv) {
    int i;
    for (i = 0; i < argc; i++) {
        cdc_unit first, second;
        cdc_diag_list diags;
        char *canon_buf = NULL;
        size_t canon_size = 0;
        FILE *mem;

        cdc_diag_list_init(&diags);
        if (!parse_or_report(argv[i], &first, &diags)) {
            cdc_unit_free(&first);
            cdc_diag_list_free(&diags);
            return 1;
        }
        mem = open_memstream(&canon_buf, &canon_size);
        if (!mem) {
            fprintf(stderr, "cdc-frontend: memstream failure\n");
            return 1;
        }
        cdc_unit_canonical(&first, mem);
        fclose(mem);
        if (!cdc_unit_parse_buffer(canon_buf, canon_size, first.file,
                                      &second, &diags) ||
            diags.errors > 0) {
            fprintf(stderr, "cdc-frontend: %s: canonical form failed to "
                            "reparse\n",
                    argv[i]);
            cdc_diag_list_print(&diags, stderr);
            return 1;
        }
        if (!cdc_unit_equal(&first, &second)) {
            fprintf(stderr, "cdc-frontend: %s: roundtrip mismatch\n",
                    argv[i]);
            return 1;
        }
        free(canon_buf);
        cdc_unit_free(&first);
        cdc_unit_free(&second);
        cdc_diag_list_free(&diags);
    }
    printf("frontend roundtrip ok files=%d\n", argc);
    return 0;
}

/* attr-parity retired with the legacy scanner it compared against (D26):
 * it measured a reader nothing used. The independent oracle for the
 * frontend is the dump differential against cdc_boot.py, which is
 * untouched. */

/* ---- bounds --------------------------------------------------------- */

typedef struct {
    const char *name;
    const char *code; /* expected diagnostic code, NULL = must accept */
    const char *buffer;
    size_t length; /* 0 = strlen(buffer) */
} bounds_case;

static int diags_contain(const cdc_diag_list *diags, const char *code) {
    size_t i;
    for (i = 0; i < diags->count; i++) {
        if (strcmp(diags->items[i].code, code) == 0) {
            return 1;
        }
    }
    return 0;
}

static int cmd_bounds(void) {
    static const char nul_case[] = "flow x a\0b";
    bounds_case cases[16];
    size_t n = 0, i;
    char *long_line = NULL;
    char *many_tokens = NULL;
    int failures = 0;

    cases[n].name = "trailing-escape";
    cases[n].code = "CDC011";
    cases[n].buffer = "flow x a=1\\";
    cases[n].length = 0;
    n++;
    cases[n].name = "unclosed-single";
    cases[n].code = "CDC012";
    cases[n].buffer = "commit y label='open";
    cases[n].length = 0;
    n++;
    cases[n].name = "unclosed-double-escape";
    cases[n].code = "CDC011";
    cases[n].buffer = "commit y label=\"open\\";
    cases[n].length = 0;
    n++;
    cases[n].name = "unknown-directive";
    cases[n].code = "CDC020";
    cases[n].buffer = "bogus x y=1";
    cases[n].length = 0;
    n++;
    cases[n].name = "witness-missing-id";
    cases[n].code = "CDC021";
    cases[n].buffer = "witness claim=\"only attrs\"";
    cases[n].length = 0;
    n++;
    cases[n].name = "duplicate-framework";
    cases[n].code = "CDC022";
    cases[n].buffer = "framework F9 label=a requires=r permits=p\n"
                      "framework F9 label=b requires=r permits=p";
    cases[n].length = 0;
    n++;
    cases[n].name = "embedded-nul";
    cases[n].code = "CDC014";
    cases[n].buffer = nul_case;
    cases[n].length = sizeof(nul_case) - 1;
    n++;
    cases[n].name = "empty-source";
    cases[n].code = NULL;
    cases[n].buffer = "";
    cases[n].length = 0;
    n++;
    cases[n].name = "quote-concat";
    cases[n].code = NULL;
    cases[n].buffer = "flow con'cat'\"enate\" a=1";
    cases[n].length = 0;
    n++;
    cases[n].name = "escaped-space-token";
    cases[n].code = NULL;
    cases[n].buffer = "flow with\\ space b=2";
    cases[n].length = 0;
    n++;

    /* line-too-long */
    {
        size_t big = (size_t)CDC_LEX_MAX_LINE + 8;
        long_line = malloc(big + 1);
        if (long_line) {
            memset(long_line, 'a', big);
            long_line[big] = '\0';
            cases[n].name = "line-too-long";
            cases[n].code = "CDC010";
            cases[n].buffer = long_line;
            cases[n].length = big;
            n++;
        }
    }
    /* too many tokens */
    {
        size_t count = (size_t)CDC_LEX_MAX_TOKENS + 8;
        size_t bytes = count * 2 + 16;
        many_tokens = malloc(bytes);
        if (many_tokens) {
            char *p = many_tokens;
            size_t k;
            memcpy(p, "flow", 4);
            p += 4;
            for (k = 0; k < count; k++) {
                *p++ = ' ';
                *p++ = 'a';
            }
            *p = '\0';
            cases[n].name = "too-many-tokens";
            cases[n].code = "CDC013";
            cases[n].buffer = many_tokens;
            cases[n].length = (size_t)(p - many_tokens);
            n++;
        }
    }

    for (i = 0; i < n; i++) {
        cdc_unit program;
        cdc_diag_list diags;
        size_t length =
            cases[i].length ? cases[i].length : strlen(cases[i].buffer);
        cdc_diag_list_init(&diags);
        cdc_unit_parse_buffer(cases[i].buffer, length, cases[i].name,
                                 &program, &diags);
        if (cases[i].code) {
            if (!diags_contain(&diags, cases[i].code)) {
                fprintf(stderr, "bounds FAIL %s: expected %s\n",
                        cases[i].name, cases[i].code);
                failures++;
            }
        } else if (diags.errors != 0) {
            fprintf(stderr, "bounds FAIL %s: unexpected rejection\n",
                    cases[i].name);
            cdc_diag_list_print(&diags, stderr);
            failures++;
        }
        cdc_unit_free(&program);
        cdc_diag_list_free(&diags);
    }
    free(long_line);
    free(many_tokens);
    if (failures) {
        return 1;
    }
    printf("frontend bounds ok cases=%d\n", (int)n);
    return 0;
}

/* ---- oom ------------------------------------------------------------ */

static long oom_fail_at;
static long oom_counter;

static void *failing_alloc(void *ptr, size_t size) {
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    oom_counter++;
    if (oom_counter == oom_fail_at) {
        return NULL;
    }
    return realloc(ptr, size);
}

static int cmd_oom(const char *path) {
    long attempt;
    for (attempt = 1; attempt < 100000; attempt++) {
        cdc_unit program;
        cdc_diag_list diags;
        int completed;
        oom_fail_at = attempt;
        oom_counter = 0;
        cdc_frontend_set_allocator(failing_alloc);
        cdc_diag_list_init(&diags);
        completed = cdc_unit_parse_file(path, &program, &diags);
        cdc_frontend_set_allocator(NULL);
        {
            int clean_success = completed && !diags.out_of_memory &&
                                oom_counter < oom_fail_at;
            cdc_unit_free(&program);
            cdc_diag_list_free(&diags);
            if (clean_success) {
                printf("frontend oom ok attempts=%ld\n", attempt);
                return 0;
            }
        }
    }
    fprintf(stderr, "frontend oom: no clean completion within bound\n");
    return 1;
}

/* ---- ABI counterexamples (2026-07-23 adversarial review) ------------ */

/* Defect-1 regression: a rejected source under an arbitrarily long path
 * must serialize to complete JSON containing the full path and message —
 * no fixed-slot truncation, no out-of-bounds. */
static int cmd_abi_diag(const char *path) {
    cdc_program *program = NULL;
    cdc_result *result = NULL;
    char *json = NULL;
    size_t length = 0;
    cdc_status status = cdc_program_parse(path, NULL, 0, &program);

    if (status != CDC_ERR_PARSE) {
        fprintf(stderr, "abi-diag FAIL: expected parse rejection, got %s\n",
                cdc_status_name(status));
        return 1;
    }
    if (cdc_program_diagnostics(program, &result) != CDC_OK ||
        cdc_result_serialize(result, &json, &length) != CDC_OK) {
        fprintf(stderr, "abi-diag FAIL: diagnostics/serialize\n");
        return 1;
    }
    if (!strstr(json, path)) {
        fprintf(stderr, "abi-diag FAIL: serialized JSON lacks full path\n");
        return 1;
    }
    if (!strstr(json, "error[CDC")) {
        fprintf(stderr, "abi-diag FAIL: serialized JSON lacks message\n");
        return 1;
    }
    fwrite(json, 1, length, stdout);
    fputc('\n', stdout);
    printf("abi-diag ok bytes=%zu\n", length);
    cdc_bytes_free(json);
    cdc_result_destroy(result);
    cdc_program_destroy(program);
    return 0;
}

/* Defect-2 regression: the ABI status for a path must match expectation
 * (io | parse | ok | memory) — directories, FIFOs, and devices are io,
 * never empty accepted programs. */
static int cmd_abi_io(const char *path, const char *expect) {
    cdc_program *program = NULL;
    cdc_status status = cdc_program_parse(path, NULL, 0, &program);
    const char *name = cdc_status_name(status);
    if (strcmp(name, expect) != 0) {
        fprintf(stderr, "abi-io FAIL %s: expected %s got %s\n", path, expect,
                name);
        cdc_program_destroy(program);
        return 1;
    }
    if (status == CDC_ERR_IO && program != NULL) {
        fprintf(stderr, "abi-io FAIL %s: io status must not yield a handle\n",
                path);
        return 1;
    }
    printf("abi-io ok path=%s status=%s\n", path, name);
    cdc_program_destroy(program);
    return 0;
}

/* Defect-2 regression, mid-read arm: reading a directory descriptor through
 * the stream path must surface ferror() as a typed CDC003, never EOF-as-
 * empty-program. (The production file path rejects directories before
 * reading; this proves the backstop.) */
static int cmd_io_mid_read(const char *dir_path) {
    int fd = open(dir_path, O_RDONLY);
    FILE *fp;
    cdc_unit unit;
    cdc_diag_list diags;
    int completed;

    if (fd < 0) {
        fprintf(stderr, "io-mid-read FAIL: cannot open %s\n", dir_path);
        return 1;
    }
    fp = fdopen(fd, "rb");
    if (!fp) {
        fprintf(stderr, "io-mid-read FAIL: fdopen\n");
        return 1;
    }
    cdc_diag_list_init(&diags);
    completed = cdc_unit_parse_stream(fp, dir_path, &unit, &diags);
    fclose(fp);
    if (completed != 0 || !diags_contain(&diags, "CDC003")) {
        fprintf(stderr,
                "io-mid-read FAIL: completed=%d (want 0 with CDC003)\n",
                completed);
        return 1;
    }
    printf("io-mid-read ok reason=CDC003\n");
    cdc_unit_free(&unit);
    cdc_diag_list_free(&diags);
    return 0;
}

/* Allocation-failure sweep over the full ABI diagnostic pipeline: at every
 * injected failure the pipeline must return a typed status (CDC_ERR_MEMORY
 * or a completed parse status) and release everything (leaks are caught by
 * the sanitizer build running this same mode). */
static int cmd_oom_abi(const char *path) {
    long attempt;
    for (attempt = 1; attempt < 100000; attempt++) {
        cdc_program *program = NULL;
        cdc_result *result = NULL;
        char *json = NULL;
        size_t length = 0;
        cdc_status status;
        int clean = 1;

        oom_fail_at = attempt;
        oom_counter = 0;
        cdc_frontend_set_allocator(failing_alloc);
        status = cdc_program_parse(path, NULL, 0, &program);
        if (status == CDC_OK || status == CDC_ERR_PARSE) {
            if (cdc_program_diagnostics(program, &result) == CDC_OK) {
                if (cdc_result_serialize(result, &json, &length) != CDC_OK) {
                    clean = 0;
                }
            } else {
                clean = 0;
            }
        } else if (status != CDC_ERR_MEMORY && status != CDC_ERR_IO) {
            cdc_frontend_set_allocator(NULL);
            fprintf(stderr, "oom-abi FAIL: unexpected status %s\n",
                    cdc_status_name(status));
            return 1;
        }
        cdc_frontend_set_allocator(NULL);
        cdc_bytes_free(json);
        cdc_result_destroy(result);
        cdc_program_destroy(program);
        if (clean && oom_counter < oom_fail_at &&
            (status == CDC_OK || status == CDC_ERR_PARSE)) {
            printf("oom-abi ok attempts=%ld\n", attempt);
            return 0;
        }
    }
    fprintf(stderr, "oom-abi FAIL: no clean completion within bound\n");
    return 1;
}

/* ---- store: digest vectors + crash matrix (Phase D, CT4/MM1 seed) --- */

static int digest_self_test(void) {
    uint8_t digest[CDC_DIGEST_SIZE];
    char hex[80];
    /* Published BLAKE3 vectors (canonical algorithm per Amendment A3). */
    cdc_digest("", 0, digest);
    cdc_digest_hex(digest, hex, sizeof(hex));
    if (strcmp(hex, "blake3:af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9ad"
                    "c112b7cc9a93cae41f3262") != 0) {
        fprintf(stderr, "digest FAIL: empty vector -> %s\n", hex);
        return 0;
    }
    cdc_digest("abc", 3, digest);
    cdc_digest_hex(digest, hex, sizeof(hex));
    if (strcmp(hex, "blake3:6437b3ac38465133ffb63b75273a8db548c558465d"
                    "79db03fd359c6cd5bd9d85") != 0) {
        fprintf(stderr, "digest FAIL: abc vector -> %s\n", hex);
        return 0;
    }
    return 1;
}

/* Full reference-vector sweep against the committed fixture: single-block,
 * block-boundary, chunk-boundary, and multi-level tree inputs, each hashed
 * both one-shot and through irregular streaming splits so update-path
 * boundary handling is covered too. */
static int cmd_digest_vectors(const char *path) {
    FILE *fp = fopen(path, "r");
    char line[256];
    long pass = 0, fail = 0;

    if (!fp) {
        fprintf(stderr, "digest-vectors: cannot open %s\n", path);
        return 1;
    }
    if (!digest_self_test()) {
        fclose(fp);
        return 1;
    }
    while (fgets(line, sizeof(line), fp)) {
        long n;
        char want[80];
        unsigned char *buf;
        long i, offset, step;
        cdc_digest_ctx ctx;
        uint8_t out[CDC_DIGEST_SIZE];
        char got[80];

        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }
        if (sscanf(line, "%ld %79s", &n, want) != 2) {
            continue;
        }
        buf = malloc((size_t)(n ? n : 1));
        if (!buf) {
            fclose(fp);
            return 1;
        }
        for (i = 0; i < n; i++) {
            buf[i] = (unsigned char)(i % 251);
        }
        /* one-shot */
        cdc_digest(buf, (size_t)n, out);
        cdc_digest_hex(out, got, sizeof(got));
        if (strcmp(got + 7, want) != 0) {
            fprintf(stderr, "digest-vectors FAIL len=%ld one-shot\n", n);
            fail++;
            free(buf);
            continue;
        }
        /* streaming in growing irregular increments */
        cdc_digest_init(&ctx);
        offset = 0;
        step = 1;
        while (offset < n) {
            long take = (n - offset < step) ? n - offset : step;
            cdc_digest_update(&ctx, buf + offset, (size_t)take);
            offset += take;
            step = step * 7 + 13;
        }
        cdc_digest_final(&ctx, out);
        cdc_digest_hex(out, got, sizeof(got));
        if (strcmp(got + 7, want) != 0) {
            fprintf(stderr, "digest-vectors FAIL len=%ld streaming\n", n);
            fail++;
            free(buf);
            continue;
        }
        free(buf);
        pass++;
    }
    fclose(fp);
    printf("digest-vectors ok vectors=%ld failed=%ld\n", pass, fail);
    return fail == 0 ? 0 : 1;
}

/* Native file digest so evidence records are produced by the same
 * implementation the runtime uses (no external digest tool). */
static int cmd_digest_file(int argc, char **argv) {
    int i;
    for (i = 0; i < argc; i++) {
        uint8_t digest[CDC_DIGEST_SIZE];
        char hex[80];
        if (!cdc_digest_file(argv[i], digest)) {
            fprintf(stderr, "digest-file: cannot read %s\n", argv[i]);
            return 1;
        }
        cdc_digest_hex(digest, hex, sizeof(hex));
        printf("%s  %s\n", hex, argv[i]);
    }
    return 0;
}

static int store_commit_txn(cdc_store *store, int which) {
    char payload[64];
    int i;
    for (i = 0; i < 3; i++) {
        snprintf(payload, sizeof(payload), "txn-%d-event-%d", which, i);
        if (cdc_store_stage(store, payload, strlen(payload)) !=
            CDC_STORE_OK) {
            return 0;
        }
    }
    return cdc_store_commit(store) == CDC_STORE_OK;
}

/* Builds a reference store with `txns` committed transactions under
 * dir/name and returns its replay digest. */
static int store_reference_digest(const char *base, const char *name,
                                  int txns, char *out, size_t out_size) {
    char dir[512];
    cdc_store *store = NULL;
    int t;
    snprintf(dir, sizeof(dir), "%s/%s", base, name);
    if (cdc_store_open(dir, &store, NULL) != CDC_STORE_OK) {
        return 0;
    }
    for (t = 1; t <= txns; t++) {
        if (!store_commit_txn(store, t)) {
            cdc_store_close(store);
            return 0;
        }
    }
    if (cdc_store_replay(store, out, out_size) != CDC_STORE_OK) {
        cdc_store_close(store);
        return 0;
    }
    cdc_store_close(store);
    return 1;
}

static int cmd_store_crash(const char *base) {
    char ref_old[80], ref_new[80];
    int boundaries = 0, old_state = 0, new_state = 0;
    int k;

    if (!digest_self_test()) {
        return 1;
    }
    if (!store_reference_digest(base, "ref1", 1, ref_old,
                                sizeof(ref_old)) ||
        !store_reference_digest(base, "ref2", 2, ref_new,
                                sizeof(ref_new))) {
        fprintf(stderr, "store-crash FAIL: reference stores\n");
        return 1;
    }
    if (strcmp(ref_old, ref_new) == 0) {
        fprintf(stderr, "store-crash FAIL: reference digests collide\n");
        return 1;
    }

    /* boundary count for the second transaction (3 events) */
    {
        char dir[512];
        cdc_store *probe = NULL;
        char payload[8] = "p";
        int i;
        snprintf(dir, sizeof(dir), "%s/probe", base);
        if (cdc_store_open(dir, &probe, NULL) != CDC_STORE_OK) {
            return 1;
        }
        for (i = 0; i < 3; i++) {
            cdc_store_stage(probe, payload, 1);
        }
        boundaries = cdc_store_commit_operations(probe);
        cdc_store_close(probe);
    }

    for (k = 1; k <= boundaries; k++) {
        char dir[512];
        cdc_store *store = NULL;
        cdc_store_status status;
        char replayed[80];
        uint64_t sealed;
        int recovered = 0;

        snprintf(dir, sizeof(dir), "%s/crash_%d", base, k);
        if (cdc_store_open(dir, &store, NULL) != CDC_STORE_OK ||
            !store_commit_txn(store, 1)) {
            fprintf(stderr, "store-crash FAIL: baseline txn (k=%d)\n", k);
            return 1;
        }
        {
            char payload[64];
            int i;
            for (i = 0; i < 3; i++) {
                snprintf(payload, sizeof(payload), "txn-2-event-%d", i);
                cdc_store_stage(store, payload, strlen(payload));
            }
        }
        cdc_store_set_fail_after(store, k);
        status = cdc_store_commit(store);
        cdc_store_close(store);
        if (status == CDC_STORE_OK) {
            fprintf(stderr,
                    "store-crash FAIL: injection %d did not fire\n", k);
            return 1;
        }
        if (status != CDC_STORE_ECRASH) {
            fprintf(stderr, "store-crash FAIL: injection %d -> %s\n", k,
                    cdc_store_status_name(status));
            return 1;
        }
        /* recovery: reopen and require exactly old or new state */
        if (cdc_store_open(dir, &store, &recovered) != CDC_STORE_OK) {
            fprintf(stderr, "store-crash FAIL: reopen (k=%d)\n", k);
            return 1;
        }
        sealed = cdc_store_sealed_count(store);
        if (cdc_store_replay(store, replayed, sizeof(replayed)) !=
                CDC_STORE_OK ||
            cdc_store_verify(store) != CDC_STORE_OK) {
            fprintf(stderr, "store-crash FAIL: replay/verify (k=%d)\n", k);
            cdc_store_close(store);
            return 1;
        }
        cdc_store_close(store);
        if (sealed == 1 && strcmp(replayed, ref_old) == 0) {
            old_state++;
        } else if (sealed == 2 && strcmp(replayed, ref_new) == 0) {
            new_state++;
        } else {
            fprintf(stderr,
                    "store-crash FAIL: partial state at k=%d "
                    "(sealed=%llu)\n",
                    k, (unsigned long long)sealed);
            return 1;
        }
    }
    printf("store-crash ok boundaries=%d old=%d new=%d\n", boundaries,
           old_state, new_state);
    return 0;
}

static int cmd_store_check(const char *base) {
    char dir[512];
    char a[80], b[80], attest[80];
    cdc_store *store = NULL;

    if (!digest_self_test()) {
        return 1;
    }
    if (!store_reference_digest(base, "det1", 2, a, sizeof(a)) ||
        !store_reference_digest(base, "det2", 2, b, sizeof(b))) {
        fprintf(stderr, "store-check FAIL: determinism stores\n");
        return 1;
    }
    if (strcmp(a, b) != 0) {
        fprintf(stderr, "store-check FAIL: replay digests differ\n");
        return 1;
    }
    snprintf(dir, sizeof(dir), "%s/det1", base);
    if (cdc_store_open(dir, &store, NULL) != CDC_STORE_OK) {
        return 1;
    }
    if (cdc_store_sealed_count(store) != 2 ||
        cdc_store_attest(store, attest, sizeof(attest)) != CDC_STORE_OK ||
        cdc_store_verify(store) != CDC_STORE_OK) {
        fprintf(stderr, "store-check FAIL: attest/verify\n");
        cdc_store_close(store);
        return 1;
    }
    /* Rollback leaves nothing staged, so the following commit has no work
     * and is a typed state error rather than an empty transaction. Fencing
     * at the wrong seal is likewise a typed refusal. (snapshot/compact/
     * fence behaviour proper is owned by the store-protocol suite.) */
    cdc_store_stage(store, "ghost", 5);
    cdc_store_rollback(store);
    if (cdc_store_commit(store) != CDC_STORE_ESTATE ||
        cdc_store_fence(store, 99) != CDC_STORE_ESTATE) {
        fprintf(stderr, "store-check FAIL: typed statuses\n");
        cdc_store_close(store);
        return 1;
    }
    cdc_store_close(store);
    printf("store-check ok determinism=1 attest=%s\n", attest);
    return 0;
}

static int read_file_bytes(const char *path, uint8_t **out, size_t *size);
static int write_file_bytes(const char *path, const uint8_t *bytes,
                            size_t size);

static long file_size(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 ? (long)st.st_size : -1;
}

/* ---- kill-based crash matrix (out-of-process) ------------------------ */

/* The in-process hook simulates a torn write; this one kills the process
 * outright at each commit boundary, so unflushed stdio buffers are lost the
 * way they are in a real power cut. The surviving parent asserts the store
 * still recovers to exactly the old or the new sealed state. */
static int cmd_store_kill(const char *base) {
    char ref_old[80], ref_new[80];
    int boundaries = 0, old_state = 0, new_state = 0, killed = 0;
    int k;

    if (!store_reference_digest(base, "kref1", 1, ref_old, sizeof(ref_old)) ||
        !store_reference_digest(base, "kref2", 2, ref_new, sizeof(ref_new))) {
        fprintf(stderr, "store-kill FAIL: reference stores\n");
        return 1;
    }
    if (strcmp(ref_old, ref_new) == 0) {
        fprintf(stderr, "store-kill FAIL: reference digests collide\n");
        return 1;
    }
    {
        char dir[512];
        cdc_store *probe = NULL;
        int i;
        snprintf(dir, sizeof(dir), "%s/kprobe", base);
        if (cdc_store_open(dir, &probe, NULL) != CDC_STORE_OK) {
            return 1;
        }
        for (i = 0; i < 3; i++) {
            cdc_store_stage(probe, "p", 1);
        }
        boundaries = cdc_store_commit_operations(probe);
        cdc_store_close(probe);
    }

    for (k = 1; k <= boundaries; k++) {
        char dir[512];
        cdc_store *store = NULL;
        char replayed[80];
        uint64_t sealed;
        int recovered = 0;
        pid_t pid;
        int status = 0;

        snprintf(dir, sizeof(dir), "%s/kill_%d", base, k);
        if (cdc_store_open(dir, &store, NULL) != CDC_STORE_OK ||
            !store_commit_txn(store, 1)) {
            fprintf(stderr, "store-kill FAIL: baseline txn (k=%d)\n", k);
            return 1;
        }
        cdc_store_close(store);

        fflush(NULL);
        pid = fork();
        if (pid < 0) {
            fprintf(stderr, "store-kill FAIL: fork\n");
            return 1;
        }
        if (pid == 0) {
            cdc_store *child = NULL;
            char payload[64];
            int i;
            if (cdc_store_open(dir, &child, NULL) != CDC_STORE_OK) {
                _exit(90);
            }
            for (i = 0; i < 3; i++) {
                snprintf(payload, sizeof(payload), "txn-2-event-%d", i);
                cdc_store_stage(child, payload, strlen(payload));
            }
            cdc_store_set_kill_after(child, k);
            cdc_store_commit(child); /* must not return */
            _exit(91);               /* reached only if the kill missed */
        }
        if (waitpid(pid, &status, 0) < 0) {
            fprintf(stderr, "store-kill FAIL: waitpid (k=%d)\n", k);
            return 1;
        }
        if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGKILL) {
            fprintf(stderr,
                    "store-kill FAIL: child survived injection k=%d "
                    "(exit=%d)\n",
                    k, WIFEXITED(status) ? WEXITSTATUS(status) : -1);
            return 1;
        }
        killed++;

        if (cdc_store_open(dir, &store, &recovered) != CDC_STORE_OK) {
            fprintf(stderr, "store-kill FAIL: reopen after kill (k=%d)\n", k);
            return 1;
        }
        sealed = cdc_store_sealed_count(store);
        if (cdc_store_replay(store, replayed, sizeof(replayed)) !=
                CDC_STORE_OK ||
            cdc_store_verify(store) != CDC_STORE_OK) {
            fprintf(stderr, "store-kill FAIL: replay/verify (k=%d)\n", k);
            cdc_store_close(store);
            return 1;
        }
        cdc_store_close(store);
        if (sealed == 1 && strcmp(replayed, ref_old) == 0) {
            old_state++;
        } else if (sealed == 2 && strcmp(replayed, ref_new) == 0) {
            new_state++;
        } else {
            fprintf(stderr,
                    "store-kill FAIL: partial state at k=%d (sealed=%llu)\n",
                    k, (unsigned long long)sealed);
            return 1;
        }
    }
    printf("store-kill ok boundaries=%d killed=%d old=%d new=%d\n",
           boundaries, killed, old_state, new_state);
    return 0;
}

/* ---- snapshot / compact / fence (Phase D protocol completion) -------- */

/* Attribute-key boundary counterexamples.
 *
 * History: the legacy reader matched `key=` as a bare substring, so an
 * attribute whose NAME ended with the key was read instead of the key —
 * `gain` read `action-gain=9.0` as 9.0, confidently wrong rather than
 * missing (D22). That reader has been deleted; these cases now pin the same
 * properties on the reader that REPLACED it, because the property is what
 * matters, not the implementation that happened to hold it.
 *
 * Two expected values changed with the reader, and the change is the point:
 * key matching is now exact by construction (the statement is tokenized
 * before any lookup), and a quoted value is returned COMPLETE and unquoted
 * where the legacy reader truncated it at the first space. */
static int cmd_attr_boundary(void) {
    static const struct {
        const char *source;
        const char *key;
        int present;
        const char *value;
        const char *why;
    } CASES[] = {
        {"field f1 action-gain=9.0 gain=1.0 dt=0.125", "gain", 1, "1.0",
         "a longer attribute ending in the key must not shadow it"},
        {"field f1 action-gain=9.0 dt=0.125", "gain", 0, "",
         "the key is absent even though a longer name contains it"},
        {"module m action-gain=2.0", "action-gain", 1, "2.0",
         "the longer name itself still reads"},
        {"cell c theta=1.5 subtheta=9.9", "theta", 1, "1.5",
         "first occurrence wins and is the whole token"},
        {"cell c subtheta=9.9 theta=1.5", "theta", 1, "1.5",
         "order does not let a suffix name win"},
        {"guard g precision=1.0", "precision", 1, "1.0", "exact key reads"},
        {"guard g imprecision=7.0", "precision", 0, "",
         "a suffix match with no boundary is not a match"},
        {"cell c theta=1.5 theta=9.9", "theta", 1, "1.5",
         "first occurrence wins on a duplicated key"},
        {"evolve e expect-contains=\"witness memory\" output=x",
         "expect-contains", 1, "witness memory",
         "a quoted value is returned complete, not truncated at a space"},
        {"evolve e expect-contains=\"witness memory\" output=x", "output", 1,
         "x", "a later attribute still reads past a quoted value"},
    };
    size_t i;
    int failures = 0;

    for (i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++) {
        cdc_unit unit;
        cdc_diag_list diags;
        const char *got;
        cdc_diag_list_init(&diags);
        cdc_unit_init(&unit);
        if (!cdc_unit_parse_buffer(CASES[i].source, strlen(CASES[i].source),
                                   "<attr-boundary>", &unit, &diags) ||
            diags.errors > 0 || unit.count != 1) {
            fprintf(stderr, "attr-boundary FAIL: could not parse: %s\n",
                    CASES[i].source);
            failures++;
            cdc_diag_list_free(&diags);
            cdc_unit_free(&unit);
            continue;
        }
        cdc_diag_list_free(&diags);
        got = cdc_stmt_attr_first(&unit.stmts[0], CASES[i].key);
        if ((got != NULL) != (CASES[i].present != 0)) {
            fprintf(stderr, "attr-boundary FAIL: %s (presence %d, want %d)\n",
                    CASES[i].why, got != NULL, CASES[i].present);
            failures++;
        } else if (got && strcmp(got, CASES[i].value) != 0) {
            fprintf(stderr, "attr-boundary FAIL: %s (got [%s], want [%s])\n",
                    CASES[i].why, got, CASES[i].value);
            failures++;
        }
        cdc_unit_free(&unit);
    }
    if (failures) {
        return 1;
    }
    printf("attr-boundary ok cases=%zu shadowing=0 quoted-value=complete\n",
           sizeof(CASES) / sizeof(CASES[0]));
    return 0;
}

/* Computes the corpus identity for a file set, from a DIFFERENT binary
 * than the one that stamps verdicts. That makes the check independent:
 * the gate can confirm a verdict names the corpus it actually ran on
 * rather than trusting the same code that produced the claim. */
static int cmd_corpus_digest(int count, char **paths) {
    uint8_t digest[CDC_DIGEST_SIZE];
    char hex[96];
    if (!cdc_digest_corpus((const char *const *)paths, (size_t)count,
                           digest)) {
        fprintf(stderr, "corpus-digest: a file was unreadable\n");
        return 1;
    }
    cdc_digest_hex(digest, hex, sizeof(hex));
    printf("corpus %s files=%d\n", hex, count);
    return 0;
}

/* Opens a store and reports its state, so a shell gate can assert that a
 * run stopped by the lifecycle contract left durable state intact rather
 * than half-applied. */
static int cmd_store_inspect(const char *dir) {
    cdc_store *store = NULL;
    int recovered = 0;
    char replay[96];
    cdc_store_status status = cdc_store_open(dir, &store, &recovered);
    if (status != CDC_STORE_OK) {
        printf("store-inspect open=%s\n", cdc_store_status_name(status));
        return 1;
    }
    if (cdc_store_replay(store, replay, sizeof(replay)) != CDC_STORE_OK) {
        printf("store-inspect open=ok replay=failed\n");
        cdc_store_close(store);
        return 1;
    }
    printf("store-inspect open=ok recovered=%d sealed=%llu events=%llu "
           "generation=%llu verify=%s replay=%s\n",
           recovered, (unsigned long long)cdc_store_sealed_count(store),
           (unsigned long long)cdc_store_event_count(store),
           (unsigned long long)cdc_store_generation(store),
           cdc_store_status_name(cdc_store_verify(store)), replay);
    cdc_store_close(store);
    return 0;
}

/* ---- parity vectors from the oracle report (interface section 7) -----
 *
 * The bootloader is the oracle for contract checks. It cannot produce
 * BLAKE3 digests (no stdlib BLAKE3 in Python, and shelling out per check
 * would be 250+ subprocesses), so its vectors are RE-RENDERED here from the
 * report it independently computed: each `  OK <label>   [<source>]` line
 * becomes the same six-field record the native evaluator emits.
 *
 * What that does and does not test. The digest function is shared, so it is
 * not under test — it does not need to be, because it is already gated by
 * 31 reference vectors. What IS under test is everything the two
 * implementations compute separately: each check's identifier, its verdict,
 * its full evaluated label, and the ORDER of all of them. A divergence in
 * any of those changes the vector, and the chained trace digest carries the
 * change forward so a reordering cannot cancel out. */
static int cmd_vectors_from_report(const char *report_path) {
    FILE *fp = fopen(report_path, "r");
    char line[4096];
    cdc_vector_chain chain;
    long emitted = 0;

    if (!fp) {
        fprintf(stderr, "vectors-from-report: cannot read %s\n", report_path);
        return 1;
    }
    cdc_vector_chain_init(&chain);
    while (fgets(line, sizeof(line), fp)) {
        const char *decision;
        char *label;
        char *source;
        char *bracket;
        char record[512];
        size_t len = strlen(line);

        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (strncmp(line, "  OK ", 5) == 0) {
            decision = "commit";
            label = line + 5;
        } else if (strncmp(line, "  FAIL ", 7) == 0) {
            decision = "fail";
            label = line + 7;
        } else {
            continue; /* banner, rule, or summary line */
        }
        /* the source is the LAST "   [" ... "]" on the line, so a label
         * containing brackets cannot truncate it */
        bracket = strrchr(label, '[');
        if (!bracket || bracket == label || bracket[-1] != ' ' ||
            label[strlen(label) - 1] != ']') {
            fprintf(stderr, "vectors-from-report: unparsable record: %s\n",
                    line);
            fclose(fp);
            return 1;
        }
        source = bracket + 1;
        label[strlen(label) - 1] = '\0'; /* drop ']' */
        /* drop the three spaces before '[' */
        {
            char *end = bracket - 1;
            while (end > label && *end == ' ') {
                *end-- = '\0';
            }
            *bracket = '\0';
        }
        if (cdc_vector_render(&chain, source, decision, NULL, label,
                              strlen(label), NULL, record,
                              sizeof(record)) < 0) {
            fprintf(stderr, "vectors-from-report: record too long\n");
            fclose(fp);
            return 1;
        }
        printf("%s\n", record);
        emitted++;
    }
    fclose(fp);
    if (emitted == 0) {
        fprintf(stderr, "vectors-from-report: no check records found\n");
        return 1;
    }
    return 0;
}

/* ---- typed effect receipts (gate CT3) --------------------------------
 *
 * The carrier `cdc test` now trusts instead of prose. These cases pin the
 * properties that make it trustworthy: a round trip preserves every field,
 * an unknown version fails CLOSED rather than being skipped as noise, a
 * malformed record is an error rather than a silent zero, and an outcome
 * cannot be absent — a receipt records an effect, and "no effect" is not
 * one. */
static int cmd_receipt_check(void) {
    int failures = 0;
    char path[256];
    void *stream;
    cdc_receipt out, back;
    FILE *fp;
    char line[4096];

    snprintf(path, sizeof(path), "build/receipt_check_%ld.txt",
             (long)getpid());
    if (setenv("CDC_RECEIPTS", path, 1) != 0) {
        fprintf(stderr, "receipt-check FAIL: setenv\n");
        return 1;
    }
    stream = cdc_receipt_open_env();
    if (!stream) {
        fprintf(stderr, "receipt-check FAIL: stream not opened\n");
        return 1;
    }

    /* A fully-populated persist receipt survives a round trip intact. */
    cdc_receipt_init(&out);
    snprintf(out.kind, sizeof(out.kind), "persist");
    snprintf(out.job, sizeof(out.job), "journal-hold");
    snprintf(out.op, sizeof(out.op), "append");
    out.outcome = CDC_OUTCOME_HELD;
    snprintf(out.reason, sizeof(out.reason), "balance-violation");
    out.declared_hold = 1;
    snprintf(out.trits, sizeof(out.trits), "-+0");
    snprintf(out.balance, sizeof(out.balance), "violated");
    out.durable = 0;
    out.replay_stable = 1;
    out.sealed = 7;
    out.events = 21;
    out.generation = 3;
    snprintf(out.witness, sizeof(out.witness), "persistence-hold-native");
    snprintf(out.closure, sizeof(out.closure),
             "blake3:0000000000000000000000000000000000000000000000000000000000000001");
    if (cdc_receipt_emit(stream, &out) != 1) {
        fprintf(stderr, "receipt-check FAIL: emit\n");
        failures++;
    }

    /* A receipt with no outcome is not a receipt. */
    {
        cdc_receipt empty;
        cdc_receipt_init(&empty);
        snprintf(empty.kind, sizeof(empty.kind), "commit");
        snprintf(empty.job, sizeof(empty.job), "c1");
        snprintf(empty.reason, sizeof(empty.reason), "none");
        if (cdc_receipt_emit(stream, &empty) != -1) {
            fprintf(stderr,
                    "receipt-check FAIL: outcome-less receipt was emitted\n");
            failures++;
        }
    }
    /* A witness named without its digest is not a link, and is refused:
     * "this effect discharges W" is only evidence if W is identified. */
    {
        cdc_receipt dangling;
        cdc_receipt_init(&dangling);
        snprintf(dangling.kind, sizeof(dangling.kind), "commit");
        snprintf(dangling.job, sizeof(dangling.job), "c1");
        dangling.outcome = CDC_OUTCOME_ACCEPTED;
        snprintf(dangling.reason, sizeof(dangling.reason), "none");
        snprintf(dangling.witness, sizeof(dangling.witness), "w1");
        if (cdc_receipt_emit(stream, &dangling) != -1) {
            fprintf(stderr,
                    "receipt-check FAIL: witness without a closure digest "
                    "was emitted\n");
            failures++;
        }
    }
    /* A value that breaks the token vocabulary is refused, not quoted. */
    {
        cdc_receipt bad;
        cdc_receipt_init(&bad);
        snprintf(bad.kind, sizeof(bad.kind), "commit");
        snprintf(bad.job, sizeof(bad.job), "has space");
        bad.outcome = CDC_OUTCOME_ACCEPTED;
        snprintf(bad.reason, sizeof(bad.reason), "none");
        if (cdc_receipt_emit(stream, &bad) != -1) {
            fprintf(stderr, "receipt-check FAIL: non-token job accepted\n");
            failures++;
        }
    }
    cdc_receipt_close(stream);

    fp = fopen(path, "r");
    if (!fp || !fgets(line, sizeof(line), fp)) {
        fprintf(stderr, "receipt-check FAIL: emitted stream unreadable\n");
        if (fp) {
            fclose(fp);
        }
        return 1;
    }
    fclose(fp);
    if (cdc_receipt_parse(line, &back) != 1) {
        fprintf(stderr, "receipt-check FAIL: parse\n");
        return 1;
    }
    if (strcmp(back.kind, out.kind) != 0 || strcmp(back.job, out.job) != 0 ||
        strcmp(back.op, out.op) != 0 || back.outcome != out.outcome ||
        strcmp(back.reason, out.reason) != 0 ||
        back.declared_hold != out.declared_hold ||
        strcmp(back.trits, out.trits) != 0 ||
        strcmp(back.balance, out.balance) != 0 ||
        back.durable != out.durable ||
        back.replay_stable != out.replay_stable ||
        back.sealed != out.sealed || back.events != out.events ||
        back.generation != out.generation ||
        strcmp(back.witness, out.witness) != 0 ||
        strcmp(back.closure, out.closure) != 0) {
        fprintf(stderr, "receipt-check FAIL: round trip lost a field\n");
        failures++;
    }

    /* Malformed and foreign records. */
    {
        struct {
            const char *line;
            int want;
            const char *why;
        } cases[] = {
            {"native reducer ok steps=3\n", 0, "foreign line is not a receipt"},
            {"\n", 0, "blank line is not a receipt"},
            {"cdc-receipt v=2 kind=commit job=c1 outcome=+1 reason=none "
             "declared-hold=0\n",
             -1, "unknown version must fail closed"},
            {"cdc-receipt v=1 kind=commit job=c1 reason=none "
             "declared-hold=0\n",
             -1, "missing outcome must fail closed"},
            {"cdc-receipt v=1 kind=commit job=c1 outcome=maybe reason=none "
             "declared-hold=0\n",
             -1, "outcome outside the ternary must fail closed"},
            {"cdc-receipt v=1 kind=commit job=c1 outcome=+1 "
             "declared-hold=0\n",
             -1, "missing reason must fail closed"},
            {"cdc-receipt v=1 kind=commit job=c1 outcome=+1 reason=none\n", -1,
             "missing declared-hold must fail closed"},
        };
        size_t i;
        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            cdc_receipt probe;
            int got = cdc_receipt_parse(cases[i].line, &probe);
            if (got != cases[i].want) {
                fprintf(stderr,
                        "receipt-check FAIL: %s (want %d, got %d)\n",
                        cases[i].why, cases[i].want, got);
                failures++;
            }
        }
    }
    remove(path);
    unsetenv("CDC_RECEIPTS");
    if (failures) {
        return 1;
    }
    printf("receipt-check ok round-trip=1 closed-vocabulary=1 "
           "closure-link=1 malformed-fail-closed=7\n");
    return 0;
}

/* ---- generation and concurrency counterexamples (2026-07-28 review) ----
 *
 * Two defects neither the crash matrix nor the protocol suite could see,
 * because both live BETWEEN two individually-correct operations:
 *
 *  1. the base was published as active by snapshot BEFORE compaction
 *     truncated the log, so a crash in between left an unopenable store;
 *     the base carried no store identity, so a valid base from another
 *     store was accepted; and attest ignored the base, so two different
 *     compacted histories attested identically.
 *  2. commit checked the sealed count and then appended with no mutual
 *     exclusion, so two writers could both pass before either wrote. The
 *     old fence test was sequential — the winner finished before the loser
 *     checked — which is exactly why it stayed green. */

/* Runs `fn` in a forked child so a store can be crashed or raced from a
 * genuinely separate process. */
static int fork_child_status(void (*fn)(const char *, int), const char *dir,
                             int arg) {
    pid_t pid;
    int status;
    fflush(NULL);
    pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        fn(dir, arg);
        _exit(0);
    }
    if (waitpid(pid, &status, 0) < 0) {
        return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* Creates dir (fresh) with `txns` committed transactions. */
static int store_seed_dir(const char *dir, int txns) {
    cdc_store *store = NULL;
    char path[640];
    int t;
    snprintf(path, sizeof(path), "%s/log.cdcstore", dir);
    unlink(path);
    snprintf(path, sizeof(path), "%s/base.pending", dir);
    unlink(path);
    if (cdc_store_open(dir, &store, NULL) != CDC_STORE_OK) {
        return 0;
    }
    for (t = 1; t <= txns; t++) {
        if (!store_commit_txn(store, t)) {
            cdc_store_close(store);
            return 0;
        }
    }
    cdc_store_close(store);
    return 1;
}

/* Child body: open, arm the kill at boundary N, run snapshot -> compact.
 * The process dies somewhere inside the transition. */
static void child_snapshot_compact(const char *dir, int boundary) {
    cdc_store *store = NULL;
    if (cdc_store_open(dir, &store, NULL) != CDC_STORE_OK) {
        _exit(90);
    }
    cdc_store_set_kill_after(store, boundary);
    cdc_store_snapshot(store);
    cdc_store_compact(store);
    cdc_store_close(store);
    _exit(0);
}

static int cmd_store_generation(const char *base) {
    char dir[512], other_dir[512], pending_path[640], other_pending[640];
    cdc_store *store = NULL, *probe = NULL;
    char attest_a[80], attest_b[80], replay_a[80];
    int failures = 0;
    int t;

    snprintf(dir, sizeof(dir), "%s/gen-a", base);
    snprintf(other_dir, sizeof(other_dir), "%s/gen-b", base);
    snprintf(pending_path, sizeof(pending_path), "%s/base.pending", dir);
    snprintf(other_pending, sizeof(other_pending), "%s/base.pending",
             other_dir);

    /* 1. snapshot WITHOUT compact, then reopen. The prepared base must not
     *    be treated as active, so the store still opens with its full
     *    history — the crash window the old design could not survive. */
    if (!store_seed_dir(dir, 0) ||
        cdc_store_open(dir, &store, NULL) != CDC_STORE_OK) {
        fprintf(stderr, "store-generation FAIL: open\n");
        return 1;
    }
    for (t = 1; t <= 3; t++) {
        if (!store_commit_txn(store, t)) {
            fprintf(stderr, "store-generation FAIL: seed txn %d\n", t);
            return 1;
        }
    }
    if (cdc_store_replay(store, replay_a, sizeof(replay_a)) != CDC_STORE_OK ||
        cdc_store_snapshot(store) != CDC_STORE_OK) {
        fprintf(stderr, "store-generation FAIL: baseline snapshot\n");
        return 1;
    }
    cdc_store_close(store);
    store = NULL;
    if (cdc_store_open(dir, &probe, NULL) != CDC_STORE_OK || !probe) {
        fprintf(stderr,
                "store-generation FAIL: snapshot-only reopen unusable\n");
        return 1;
    }
    if (cdc_store_sealed_count(probe) != 3 ||
        cdc_store_generation(probe) != 0) {
        fprintf(stderr,
                "store-generation FAIL: prepared base treated as active\n");
        failures++;
    }
    {
        char resumed[80];
        if (cdc_store_replay(probe, resumed, sizeof(resumed)) !=
                CDC_STORE_OK ||
            strcmp(resumed, replay_a) != 0) {
            fprintf(stderr,
                    "store-generation FAIL: snapshot-only changed replay\n");
            failures++;
        }
    }
    cdc_store_close(probe);
    probe = NULL;
    printf("store-generation snapshot-only reopen=ok handle=yes "
           "generation=0 sealed=3\n");

    /* 2. Kill at EVERY boundary of the snapshot -> compaction transition.
     *    The store must always open, must always verify, and must be
     *    exactly the old or the new generation — never a mixture. */
    {
        int boundary;
        int old_gen = 0, new_gen = 0;
        for (boundary = 1; boundary <= 8; boundary++) {
            char kdir[512];
            cdc_store *after = NULL;
            uint64_t gen;
            char resumed[80];
            snprintf(kdir, sizeof(kdir), "%s/gen-kill-%d", base, boundary);
            if (!store_seed_dir(kdir, 3)) {
                fprintf(stderr, "store-generation FAIL: seed kill dir\n");
                return 1;
            }
            fork_child_status(child_snapshot_compact, kdir, boundary);
            if (cdc_store_open(kdir, &after, NULL) != CDC_STORE_OK || !after) {
                fprintf(stderr,
                        "store-generation FAIL: unusable after kill at "
                        "boundary %d\n",
                        boundary);
                failures++;
                continue;
            }
            gen = cdc_store_generation(after);
            if (cdc_store_sealed_count(after) != 3 ||
                cdc_store_verify(after) != CDC_STORE_OK ||
                cdc_store_replay(after, resumed, sizeof(resumed)) !=
                    CDC_STORE_OK ||
                strcmp(resumed, replay_a) != 0) {
                fprintf(stderr,
                        "store-generation FAIL: history changed by kill at "
                        "boundary %d\n",
                        boundary);
                failures++;
            }
            if (gen == 0) {
                old_gen++;
            } else if (gen == 1) {
                new_gen++;
            } else {
                fprintf(stderr,
                        "store-generation FAIL: mixed generation %llu at "
                        "boundary %d\n",
                        (unsigned long long)gen, boundary);
                failures++;
            }
            cdc_store_close(after);
        }
        printf("store-generation kill matrix: boundaries=8 old=%d new=%d "
               "mixed=0 unusable=0\n",
               old_gen, new_gen);
    }

    /* 3. A valid prepared base from ANOTHER store must never activate.
     *    The two stores are given IDENTICAL histories on purpose, so their
     *    generation, sealed count, event count and replay state all match
     *    exactly. Every check except the store-identity binding therefore
     *    passes, which is what makes this a real isolation of that binding
     *    rather than an accidental refusal for some other reason. */
    if (!store_seed_dir(dir, 3) || !store_seed_dir(other_dir, 3)) {
        fprintf(stderr, "store-generation FAIL: seed substitution dirs\n");
        return 1;
    }
    {
        cdc_store *b = NULL;
        uint8_t *bytes = NULL;
        size_t size = 0;
        char replay_of_a[80], replay_of_b[80];
        if (cdc_store_open(dir, &store, NULL) != CDC_STORE_OK ||
            cdc_store_open(other_dir, &b, NULL) != CDC_STORE_OK) {
            fprintf(stderr, "store-generation FAIL: open twin stores\n");
            return 1;
        }
        if (cdc_store_replay(store, replay_of_a, sizeof(replay_of_a)) !=
                CDC_STORE_OK ||
            cdc_store_replay(b, replay_of_b, sizeof(replay_of_b)) !=
                CDC_STORE_OK ||
            strcmp(replay_of_a, replay_of_b) != 0) {
            fprintf(stderr,
                    "store-generation FAIL: twin stores must share a replay "
                    "identity for this to isolate identity binding\n");
            return 1;
        }
        if (cdc_store_snapshot(b) != CDC_STORE_OK) {
            fprintf(stderr, "store-generation FAIL: prepare foreign base\n");
            return 1;
        }
        cdc_store_close(b);
        cdc_store_close(store);
        store = NULL;
        if (!read_file_bytes(other_pending, &bytes, &size)) {
            fprintf(stderr, "store-generation FAIL: read foreign base\n");
            return 1;
        }
        write_file_bytes(pending_path, bytes, size);
        free(bytes);
        if (cdc_store_open(dir, &probe, NULL) != CDC_STORE_OK || !probe) {
            fprintf(stderr,
                    "store-generation FAIL: substitution bricked store\n");
            return 1;
        }
        if (cdc_store_compact(probe) == CDC_STORE_OK) {
            fprintf(stderr,
                    "store-generation FAIL: foreign base activated (twin "
                    "history, so only the store identity distinguishes it)\n");
            failures++;
        } else {
            printf("store-generation substitution twin-history=1 reopen=ok "
                   "foreign-base-activated=0\n");
        }
        cdc_store_close(probe);
        probe = NULL;
        unlink(pending_path);
    }

    /* 4. Attestation must cover the base. Two checks, because they fail
     *    for different reasons:
     *    (a) identical histories in DIFFERENT stores attest differently —
     *        evidence identity includes which store produced it;
     *    (b) different histories attest differently even after both are
     *        compacted to an effectively empty tail. */
    {
        char twin_dir[512];
        cdc_store *x = NULL;
        snprintf(twin_dir, sizeof(twin_dir), "%s/gen-twin", base);
        if (!store_seed_dir(dir, 3) || !store_seed_dir(twin_dir, 3) ||
            cdc_store_open(dir, &store, NULL) != CDC_STORE_OK ||
            cdc_store_snapshot(store) != CDC_STORE_OK ||
            cdc_store_compact(store) != CDC_STORE_OK ||
            cdc_store_attest(store, attest_a, sizeof(attest_a)) !=
                CDC_STORE_OK ||
            cdc_store_open(twin_dir, &x, NULL) != CDC_STORE_OK ||
            cdc_store_snapshot(x) != CDC_STORE_OK ||
            cdc_store_compact(x) != CDC_STORE_OK ||
            cdc_store_attest(x, attest_b, sizeof(attest_b)) != CDC_STORE_OK) {
            fprintf(stderr, "store-generation FAIL: twin attest setup\n");
            return 1;
        }
        cdc_store_close(store);
        cdc_store_close(x);
        store = NULL;
        if (strcmp(attest_a, attest_b) == 0) {
            fprintf(stderr,
                    "store-generation FAIL: distinct stores attest equal\n");
            failures++;
        }
    }
    {
        char diff_dir[512];
        cdc_store *y = NULL;
        char attest_c[80];
        snprintf(diff_dir, sizeof(diff_dir), "%s/gen-diff", base);
        if (!store_seed_dir(diff_dir, 5) ||
            cdc_store_open(diff_dir, &y, NULL) != CDC_STORE_OK ||
            cdc_store_snapshot(y) != CDC_STORE_OK ||
            cdc_store_compact(y) != CDC_STORE_OK ||
            cdc_store_attest(y, attest_c, sizeof(attest_c)) != CDC_STORE_OK) {
            fprintf(stderr, "store-generation FAIL: divergent history setup\n");
            return 1;
        }
        cdc_store_close(y);
        if (strcmp(attest_a, attest_c) == 0) {
            fprintf(stderr,
                    "store-generation FAIL: distinct histories attest equal\n"
                    "  %s\n  %s\n",
                    attest_a, attest_c);
            failures++;
        } else {
            printf("store-generation compacted-attest store-diff=1 "
                   "history-diff=1 attest-equal=0\n");
        }
    }

    /* 5. A base prepared against an older generation must not replay over
     *    an advanced log. */
    {
        char rdir[512], rpending[640];
        cdc_store *r = NULL;
        uint8_t *stale = NULL;
        size_t stale_size = 0;
        snprintf(rdir, sizeof(rdir), "%s/gen-rollback", base);
        snprintf(rpending, sizeof(rpending), "%s/base.pending", rdir);
        if (!store_seed_dir(rdir, 3) ||
            cdc_store_open(rdir, &r, NULL) != CDC_STORE_OK ||
            cdc_store_snapshot(r) != CDC_STORE_OK) {
            fprintf(stderr, "store-generation FAIL: seed rollback\n");
            return 1;
        }
        if (!read_file_bytes(rpending, &stale, &stale_size)) {
            fprintf(stderr, "store-generation FAIL: capture stale base\n");
            return 1;
        }
        if (cdc_store_compact(r) != CDC_STORE_OK) {
            fprintf(stderr, "store-generation FAIL: first compaction\n");
            free(stale);
            return 1;
        }
        cdc_store_close(r);
        r = NULL;
        /* replay the generation-1 base over a store already at generation 1 */
        write_file_bytes(rpending, stale, stale_size);
        free(stale);
        if (cdc_store_open(rdir, &r, NULL) != CDC_STORE_OK || !r) {
            fprintf(stderr, "store-generation FAIL: rollback reopen\n");
            return 1;
        }
        if (cdc_store_generation(r) != 1) {
            fprintf(stderr, "store-generation FAIL: generation not carried\n");
            failures++;
        }
        if (cdc_store_compact(r) == CDC_STORE_OK) {
            fprintf(stderr,
                    "store-generation FAIL: stale-generation base "
                    "activated\n");
            failures++;
        } else {
            printf("store-generation stale-generation-base activated=0\n");
        }
        cdc_store_close(r);
    }

    /* 6. Special paths as the prepared base are typed EIO, never a block. */
    {
        char sdir[512], spath[640];
        cdc_store *s = NULL;
        snprintf(sdir, sizeof(sdir), "%s/gen-special", base);
        snprintf(spath, sizeof(spath), "%s/base.pending", sdir);
        if (!store_seed_dir(sdir, 1)) {
            fprintf(stderr, "store-generation FAIL: seed special dir\n");
            return 1;
        }
        unlink(spath);
        if (mkdir(spath, 0777) != 0 ||
            cdc_store_open(sdir, &s, NULL) != CDC_STORE_OK) {
            fprintf(stderr, "store-generation FAIL: directory base setup\n");
            return 1;
        }
        if (cdc_store_compact(s) != CDC_STORE_EIO) {
            fprintf(stderr,
                    "store-generation FAIL: directory base not typed EIO\n");
            failures++;
        }
        cdc_store_close(s);
        rmdir(spath);
        if (mkfifo(spath, 0666) == 0) {
            s = NULL;
            if (cdc_store_open(sdir, &s, NULL) != CDC_STORE_OK) {
                fprintf(stderr, "store-generation FAIL: fifo base setup\n");
                return 1;
            }
            if (cdc_store_compact(s) != CDC_STORE_EIO) {
                fprintf(stderr,
                        "store-generation FAIL: fifo base not typed EIO\n");
                failures++;
            }
            cdc_store_close(s);
            unlink(spath);
        }
        printf("store-generation special-paths typed-eio=1 blocked=0\n");
    }

    /* 7. A crash BETWEEN snapshot and compact leaves a prepared base
     *    behind; cdc_store_reset must remove it along with the log, so a
     *    fresh store is genuinely fresh. Before D27 the reset's artifact
     *    list still named the pre-D16 snapshot file and missed
     *    base.pending, so the stale foreign base survived and turned the
     *    fresh store's next early compact into ECORRUPT — a mode=fresh
     *    declaration that did not deliver a known-empty state. */
    {
        char fdir[512], fpending[640];
        cdc_store *f = NULL;
        struct stat st;
        snprintf(fdir, sizeof(fdir), "%s/gen-fresh", base);
        snprintf(fpending, sizeof(fpending), "%s/base.pending", fdir);
        if (!store_seed_dir(fdir, 2)) {
            fprintf(stderr, "store-generation FAIL: seed fresh dir\n");
            return 1;
        }
        if (cdc_store_open(fdir, &f, NULL) != CDC_STORE_OK ||
            cdc_store_snapshot(f) != CDC_STORE_OK) {
            fprintf(stderr, "store-generation FAIL: prepare stale base\n");
            return 1;
        }
        cdc_store_close(f); /* crash: base.pending is left behind */
        f = NULL;
        if (cdc_store_reset(fdir) != CDC_STORE_OK) {
            fprintf(stderr, "store-generation FAIL: reset\n");
            return 1;
        }
        if (stat(fpending, &st) == 0) {
            fprintf(stderr,
                    "store-generation FAIL: reset left a stale prepared "
                    "base behind\n");
            failures++;
        }
        if (cdc_store_open(fdir, &f, NULL) != CDC_STORE_OK || !f) {
            fprintf(stderr, "store-generation FAIL: fresh reopen\n");
            return 1;
        }
        if (!store_commit_txn(f, 1)) {
            fprintf(stderr, "store-generation FAIL: fresh commit\n");
            return 1;
        }
        /* an early compact on a fresh store holds on "nothing prepared" —
         * never ECORRUPT from a base that should not exist */
        if (cdc_store_compact(f) != CDC_STORE_ESTATE) {
            fprintf(stderr,
                    "store-generation FAIL: fresh store saw a stale base\n");
            failures++;
        }
        cdc_store_close(f);
        printf("store-generation fresh-after-crash stale-base-removed=1 "
               "early-compact=state\n");
    }

    if (failures) {
        return 1;
    }
    printf("store-generation ok atomic-transition=1 identity-bound=1 "
           "attest-covers-base=1\n");
    return 0;
}

/* Two forked writers fence the SAME state and are released together, so
 * both are inside the check-then-append window at once. Exactly one must
 * commit; the other must be refused; the store must survive intact. */
static int cmd_store_race(const char *base) {
    char dir[512];
    int failures = 0;
    int round;

    for (round = 0; round < 3; round++) {
        int gate[2], result[2], ready[2];
        pid_t pids[2];
        int i;
        cdc_store *check = NULL;
        uint64_t sealed_before;

        snprintf(dir, sizeof(dir), "%s/race-%d", base, round);
        if (!store_seed_dir(dir, 2) ||
            cdc_store_open(dir, &check, NULL) != CDC_STORE_OK) {
            fprintf(stderr, "store-race FAIL: seed round %d\n", round);
            return 1;
        }
        sealed_before = cdc_store_sealed_count(check);
        cdc_store_close(check);

        if (pipe(gate) != 0 || pipe(result) != 0 || pipe(ready) != 0) {
            fprintf(stderr, "store-race FAIL: pipes\n");
            return 1;
        }
        fflush(NULL);
        for (i = 0; i < 2; i++) {
            pids[i] = fork();
            if (pids[i] < 0) {
                fprintf(stderr, "store-race FAIL: fork\n");
                return 1;
            }
            if (pids[i] == 0) {
                cdc_store *w = NULL;
                char payload[32];
                char go;
                char code;
                cdc_store_status fs;
                close(gate[1]);
                close(result[0]);
                close(ready[0]);
                if (cdc_store_open(dir, &w, NULL) != CDC_STORE_OK) {
                    _exit(91);
                }
                /* Fence and stage BEFORE signalling ready. The parent does
                 * not release the barrier until both writers have fenced
                 * the same state, so both are provably inside the
                 * check-then-append window when they commit — which is the
                 * window the old sequential test never entered. */
                fs = cdc_store_fence(w, sealed_before);
                if (fs != CDC_STORE_OK) {
                    _exit(95);
                }
                snprintf(payload, sizeof(payload), "racer-%d", i);
                if (cdc_store_stage(w, payload, strlen(payload)) !=
                    CDC_STORE_OK) {
                    _exit(92);
                }
                if (write(ready[1], "r", 1) != 1) {
                    _exit(96);
                }
                if (read(gate[0], &go, 1) != 1) {
                    _exit(93);
                }
                code = (char)cdc_store_commit(w);
                if (write(result[1], &code, 1) != 1) {
                    _exit(94);
                }
                cdc_store_close(w);
                _exit(0);
            }
        }
        close(gate[0]);
        close(result[1]);
        close(ready[1]);
        {
            char acks[2];
            int seen = 0;
            while (seen < 2) {
                ssize_t n = read(ready[0], acks + seen, (size_t)(2 - seen));
                if (n <= 0) {
                    break;
                }
                seen += (int)n;
            }
            close(ready[0]);
            if (seen != 2) {
                fprintf(stderr,
                        "store-race FAIL round %d: only %d writers reached "
                        "the barrier\n",
                        round, seen);
                return 1;
            }
        }
        if (write(gate[1], "gg", 2) != 2) {
            fprintf(stderr, "store-race FAIL: release\n");
            return 1;
        }
        close(gate[1]);
        {
            char codes[2];
            int got = 0, ok_count = 0, state_count = 0, other_count = 0;
            while (got < 2) {
                ssize_t n = read(result[0], codes + got, (size_t)(2 - got));
                if (n <= 0) {
                    break;
                }
                got += (int)n;
            }
            close(result[0]);
            for (i = 0; i < 2; i++) {
                int status;
                waitpid(pids[i], &status, 0);
                if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                    fprintf(stderr,
                            "store-race: writer %d exited %d (setup failed, "
                            "not a race outcome)\n",
                            i, WIFEXITED(status) ? WEXITSTATUS(status) : -1);
                }
            }
            for (i = 0; i < got; i++) {
                if (codes[i] == (char)CDC_STORE_OK) {
                    ok_count++;
                } else if (codes[i] == (char)CDC_STORE_ESTATE) {
                    state_count++;
                } else {
                    other_count++;
                }
            }
            if (got != 2 || ok_count != 1 || state_count != 1 ||
                other_count != 0) {
                fprintf(stderr,
                        "store-race FAIL round %d: got=%d ok=%d estate=%d "
                        "other=%d (exactly one winner required)\n",
                        round, got, ok_count, state_count, other_count);
                failures++;
            }
        }
        if (cdc_store_open(dir, &check, NULL) != CDC_STORE_OK || !check) {
            fprintf(stderr, "store-race FAIL round %d: store unusable\n",
                    round);
            failures++;
            continue;
        }
        if (cdc_store_verify(check) != CDC_STORE_OK ||
            cdc_store_sealed_count(check) != sealed_before + 1) {
            fprintf(stderr,
                    "store-race FAIL round %d: sealed=%llu expected=%llu\n",
                    round, (unsigned long long)cdc_store_sealed_count(check),
                    (unsigned long long)(sealed_before + 1));
            failures++;
        }
        cdc_store_close(check);
    }
    if (!failures) {
        printf("store-race ok rounds=3 winners=1/round refused=1/round "
               "corrupt=0\n");
    }

    /* A transaction committed after the base was prepared must never be
     * discarded: compaction incorporates it or holds. */
    {
        cdc_store *a = NULL, *b = NULL;
        char cdir[512];
        uint64_t sealed_after;
        snprintf(cdir, sizeof(cdir), "%s/race-compact", base);
        if (!store_seed_dir(cdir, 2) ||
            cdc_store_open(cdir, &a, NULL) != CDC_STORE_OK ||
            cdc_store_open(cdir, &b, NULL) != CDC_STORE_OK) {
            fprintf(stderr, "store-race FAIL: compact handles\n");
            return 1;
        }
        if (cdc_store_snapshot(a) != CDC_STORE_OK || !store_commit_txn(b, 77)) {
            fprintf(stderr, "store-race FAIL: interleaved commit\n");
            return 1;
        }
        sealed_after = cdc_store_sealed_count(b);
        if (cdc_store_compact(a) == CDC_STORE_OK) {
            fprintf(stderr,
                    "store-race FAIL: compaction discarded a committed "
                    "transaction\n");
            failures++;
        }
        cdc_store_close(a);
        cdc_store_close(b);
        a = NULL;
        if (cdc_store_open(cdir, &a, NULL) != CDC_STORE_OK ||
            cdc_store_sealed_count(a) != sealed_after ||
            cdc_store_verify(a) != CDC_STORE_OK) {
            fprintf(stderr, "store-race FAIL: commit lost to compaction\n");
            failures++;
        }
        cdc_store_close(a);
        if (!failures) {
            printf("store-race commit-vs-compact commit-preserved=1\n");
        }
    }
    return failures ? 1 : 0;
}

/* ---- store-samep: simultaneous handles in ONE application -------------
 *
 * The 2026-07-28 same-application review (after ca26608): fcntl locks
 * serialize processes only, and closing ANY descriptor a process holds on
 * the lock file drops EVERY lock the process holds on it. Five checks,
 * each deterministic (a blocked operation is proven blocked by polling
 * its completion pipe while the section is held, never by sleeping and
 * hoping), covering open recovery/creation, commit, snapshot, compact,
 * reset, and the close hazard. All five FAIL against the per-handle
 * probe build (CDC_STORE_TEST_PER_HANDLE_LOCK), which reproduces the
 * pre-repair locking. */

typedef struct {
    cdc_store *store;
    const char *dir;
    int signal_fd;
    int commits;
    cdc_store *opened; /* published by samep_open_keep_body */
} samep_args;

static int samep_readable(int fd, int timeout_ms) {
    struct pollfd probe;
    int rc;
    probe.fd = fd;
    probe.events = POLLIN;
    probe.revents = 0;
    do {
        rc = poll(&probe, 1, timeout_ms);
    } while (rc < 0 && errno == EINTR);
    return rc > 0 && (probe.revents & POLLIN) != 0;
}

static void samep_signal(int fd, char code) {
    ssize_t n;
    do {
        n = write(fd, &code, 1);
    } while (n < 0 && errno == EINTR);
}

static int samep_wait_code(int fd, char *code) {
    ssize_t n;
    do {
        n = read(fd, code, 1);
    } while (n < 0 && errno == EINTR);
    return n == 1;
}

static void *samep_open_body(void *arg) {
    samep_args *args = (samep_args *)arg;
    cdc_store *store = NULL;
    cdc_store_status rc = cdc_store_open(args->dir, &store, NULL);
    samep_signal(args->signal_fd, rc == CDC_STORE_OK ? 'o' : 'f');
    if (store) {
        cdc_store_close(store);
    }
    return NULL;
}

static void *samep_lock_body(void *arg) {
    samep_args *args = (samep_args *)arg;
    cdc_store_status rc = cdc_store_lock_test(args->store);
    samep_signal(args->signal_fd, rc == CDC_STORE_OK ? 'l' : 'f');
    if (rc == CDC_STORE_OK) {
        cdc_store_unlock_test(args->store);
    }
    return NULL;
}

static void *samep_commit_body(void *arg) {
    samep_args *args = (samep_args *)arg;
    char payload[48];
    int ok = 0;
    int i;
    for (i = 0; i < args->commits; i++) {
        snprintf(payload, sizeof(payload), "samep-%p-%d", (void *)args, i);
        if (cdc_store_stage(args->store, payload, strlen(payload)) !=
            CDC_STORE_OK) {
            break;
        }
        if (cdc_store_commit(args->store) != CDC_STORE_OK) {
            cdc_store_rollback(args->store);
            break;
        }
        ok++;
    }
    samep_signal(args->signal_fd, (char)ok);
    return NULL;
}

static void *samep_snapshot_body(void *arg) {
    samep_args *args = (samep_args *)arg;
    samep_signal(args->signal_fd, (char)cdc_store_snapshot(args->store));
    return NULL;
}

static void *samep_compact_body(void *arg) {
    samep_args *args = (samep_args *)arg;
    samep_signal(args->signal_fd, (char)cdc_store_compact(args->store));
    return NULL;
}

static void *samep_reset_body(void *arg) {
    samep_args *args = (samep_args *)arg;
    samep_signal(args->signal_fd, (char)cdc_store_reset(args->dir));
    return NULL;
}

static void *samep_close_body(void *arg) {
    samep_args *args = (samep_args *)arg;
    cdc_store_close(args->store);
    samep_signal(args->signal_fd, 'c');
    return NULL;
}

static void *samep_open_keep_body(void *arg) {
    samep_args *args = (samep_args *)arg;
    args->opened = NULL;
    if (cdc_store_open(args->dir, &args->opened, NULL) != CDC_STORE_OK) {
        samep_signal(args->signal_fd, 'f');
        return NULL;
    }
    samep_signal(args->signal_fd, 'o');
    return NULL;
}

/* Forks a foreign process that tries a non-blocking exclusive fcntl lock
 * on `lock_path`. Returns 0 refused (a live lock excluded it), 1 acquired
 * (no live lock), -1 on harness error. */
static int samep_foreign_probe(const char *lock_path) {
    pid_t pid;
    int status;
    fflush(NULL);
    pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        struct flock probe;
        int fd = open(lock_path, O_RDWR);
        if (fd < 0) {
            _exit(90);
        }
        memset(&probe, 0, sizeof(probe));
        probe.l_type = F_WRLCK;
        probe.l_whence = SEEK_SET;
        if (fcntl(fd, F_SETLK, &probe) == 0) {
            _exit(1);
        }
        if (errno != EAGAIN && errno != EACCES) {
            _exit(92);
        }
        _exit(0);
    }
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status)) {
        return -1;
    }
    if (WEXITSTATUS(status) == 0) {
        return 0;
    }
    if (WEXITSTATUS(status) == 1) {
        return 1;
    }
    return -1;
}

/* Spawns `body` on a thread, proves it does NOT complete while the
 * section is held, releases the section on `held`, then proves it DOES
 * complete and returns its status byte. The premature/timeout verdicts
 * are written to *premature so the caller can name the failure. */
static int samep_blocked_run(cdc_store *held, void *(*body)(void *),
                             samep_args *args, char *code, int *premature) {
    pthread_t thread;
    int fds[2];
    int done_ok;
    *premature = 0;
    if (pipe(fds) != 0) {
        return 0;
    }
    args->signal_fd = fds[1];
    if (pthread_create(&thread, NULL, body, args) != 0) {
        close(fds[0]);
        close(fds[1]);
        return 0;
    }
    if (samep_readable(fds[0], 300)) {
        *premature = 1;
    }
    cdc_store_unlock_test(held);
    done_ok = samep_readable(fds[0], 8000) && samep_wait_code(fds[0], code);
    pthread_join(thread, NULL);
    close(fds[0]);
    close(fds[1]);
    return done_ok;
}

/* Check 1: open (creation + recovery is a check-then-act sequence) is
 * excluded by a held section, from another process AND from another
 * handle in this process. */
static int samep_check_open(const char *base) {
    char dir[560];
    cdc_store *a = NULL;
    samep_args args;
    char code = 0;
    int premature = 0;
    int fds[2];
    pid_t pid;
    int status;

    snprintf(dir, sizeof(dir), "%s/open", base);
    if (!store_seed_dir(dir, 2) ||
        cdc_store_open(dir, &a, NULL) != CDC_STORE_OK) {
        fprintf(stderr, "store-samep FAIL open: seed\n");
        return 1;
    }
    if (cdc_store_lock_test(a) != CDC_STORE_OK || pipe(fds) != 0) {
        fprintf(stderr, "store-samep FAIL open: hold\n");
        cdc_store_close(a);
        return 1;
    }
    fflush(NULL);
    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "store-samep FAIL open: fork\n");
        cdc_store_unlock_test(a);
        cdc_store_close(a);
        return 1;
    }
    if (pid == 0) {
        cdc_store *child = NULL;
        cdc_store_status rc;
        close(fds[0]);
        rc = cdc_store_open(dir, &child, NULL);
        samep_signal(fds[1], rc == CDC_STORE_OK ? 'o' : 'f');
        if (child) {
            cdc_store_close(child);
        }
        _exit(rc == CDC_STORE_OK ? 0 : 1);
    }
    close(fds[1]);
    if (samep_readable(fds[0], 300)) {
        fprintf(stderr, "store-samep FAIL open: another PROCESS finished "
                        "opening while the section was held\n");
        cdc_store_unlock_test(a);
        close(fds[0]);
        waitpid(pid, &status, 0);
        cdc_store_close(a);
        return 1;
    }
    cdc_store_unlock_test(a);
    if (!samep_readable(fds[0], 8000) || !samep_wait_code(fds[0], &code) ||
        code != 'o') {
        fprintf(stderr, "store-samep FAIL open: child open never "
                        "completed after release\n");
        close(fds[0]);
        kill(pid, SIGKILL); /* never trade a FAIL for a hang */
        waitpid(pid, &status, 0);
        cdc_store_close(a);
        return 1;
    }
    close(fds[0]);
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "store-samep FAIL open: child exit\n");
        cdc_store_close(a);
        return 1;
    }
    /* same-process second handle */
    if (cdc_store_lock_test(a) != CDC_STORE_OK) {
        fprintf(stderr, "store-samep FAIL open: re-hold\n");
        cdc_store_close(a);
        return 1;
    }
    args.dir = dir;
    args.store = NULL;
    args.commits = 0;
    if (!samep_blocked_run(a, samep_open_body, &args, &code, &premature) ||
        code != 'o') {
        fprintf(stderr, "store-samep FAIL open: same-process open never "
                        "completed after release\n");
        cdc_store_close(a);
        return 1;
    }
    if (premature) {
        fprintf(stderr, "store-samep FAIL open: a second handle in THIS "
                        "process finished opening while the section was "
                        "held\n");
        cdc_store_close(a);
        return 1;
    }
    cdc_store_close(a);
    printf("samep-open cross-process-blocked=1 same-process-blocked=1\n");
    return 0;
}

/* Check 2: two handles in one process are mutually excluded, and their
 * interleaved commits serialize to a clean log with nothing lost. */
static int samep_check_commit(const char *base) {
    char dir[560];
    cdc_store *a = NULL, *b = NULL, *check = NULL;
    samep_args lock_args, commit_a, commit_b;
    pthread_t t1, t2;
    int p1[2], p2[2];
    char code = 0, ok_a = 0, ok_b = 0;
    int premature = 0;
    uint64_t sealed_before;
    int recovered = -1;

    snprintf(dir, sizeof(dir), "%s/commit", base);
    if (!store_seed_dir(dir, 1) ||
        cdc_store_open(dir, &a, NULL) != CDC_STORE_OK ||
        cdc_store_open(dir, &b, NULL) != CDC_STORE_OK) {
        fprintf(stderr, "store-samep FAIL commit: seed\n");
        cdc_store_close(a);
        return 1;
    }
    sealed_before = cdc_store_sealed_count(a);
    /* exclusion, proven directly */
    if (cdc_store_lock_test(a) != CDC_STORE_OK) {
        fprintf(stderr, "store-samep FAIL commit: hold\n");
        cdc_store_close(a);
        cdc_store_close(b);
        return 1;
    }
    lock_args.store = b;
    lock_args.dir = dir;
    lock_args.commits = 0;
    if (!samep_blocked_run(a, samep_lock_body, &lock_args, &code,
                           &premature) ||
        code != 'l') {
        fprintf(stderr, "store-samep FAIL commit: probe thread\n");
        cdc_store_close(a);
        cdc_store_close(b);
        return 1;
    }
    if (premature) {
        fprintf(stderr, "store-samep FAIL commit: the second handle "
                        "entered the critical section while the first "
                        "held it\n");
        cdc_store_close(a);
        cdc_store_close(b);
        return 1;
    }
    /* integrity under interleaving */
    if (pipe(p1) != 0 || pipe(p2) != 0) {
        fprintf(stderr, "store-samep FAIL commit: pipes\n");
        cdc_store_close(a);
        cdc_store_close(b);
        return 1;
    }
    commit_a.store = a;
    commit_a.dir = dir;
    commit_a.commits = 40;
    commit_a.signal_fd = p1[1];
    commit_b.store = b;
    commit_b.dir = dir;
    commit_b.commits = 40;
    commit_b.signal_fd = p2[1];
    if (pthread_create(&t1, NULL, samep_commit_body, &commit_a) != 0 ||
        pthread_create(&t2, NULL, samep_commit_body, &commit_b) != 0) {
        fprintf(stderr, "store-samep FAIL commit: threads\n");
        cdc_store_close(a);
        cdc_store_close(b);
        return 1;
    }
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    if (!samep_wait_code(p1[0], &ok_a) || !samep_wait_code(p2[0], &ok_b)) {
        fprintf(stderr, "store-samep FAIL commit: counts\n");
        cdc_store_close(a);
        cdc_store_close(b);
        return 1;
    }
    close(p1[0]);
    close(p1[1]);
    close(p2[0]);
    close(p2[1]);
    cdc_store_close(a);
    cdc_store_close(b);
    if (ok_a != 40 || ok_b != 40) {
        fprintf(stderr, "store-samep FAIL commit: ok_a=%d ok_b=%d "
                        "(interleaved commits must all serialize)\n",
                ok_a, ok_b);
        return 1;
    }
    if (cdc_store_open(dir, &check, &recovered) != CDC_STORE_OK ||
        recovered != 0 || cdc_store_verify(check) != CDC_STORE_OK ||
        cdc_store_sealed_count(check) != sealed_before + 80) {
        fprintf(stderr,
                "store-samep FAIL commit: recovered=%d sealed=%llu "
                "expected=%llu\n",
                recovered,
                check ? (unsigned long long)cdc_store_sealed_count(check)
                      : 0ULL,
                (unsigned long long)(sealed_before + 80));
        cdc_store_close(check);
        return 1;
    }
    cdc_store_close(check);
    printf("samep-commit exclusion=blocked commits=80 lost=0 "
           "verify=ok recovered=0\n");
    return 0;
}

/* Check 3: snapshot and compact are excluded by a held section, the
 * generation transition leaves the OTHER handle with a typed ESTATE
 * refusal (never a corrupt append), and a reopen resumes cleanly. */
static int samep_check_transition(const char *base) {
    char dir[560];
    cdc_store *a = NULL, *b = NULL, *check = NULL;
    samep_args args;
    char code = 0;
    int premature = 0;

    snprintf(dir, sizeof(dir), "%s/transition", base);
    if (!store_seed_dir(dir, 3) ||
        cdc_store_open(dir, &a, NULL) != CDC_STORE_OK ||
        cdc_store_open(dir, &b, NULL) != CDC_STORE_OK) {
        fprintf(stderr, "store-samep FAIL transition: seed\n");
        cdc_store_close(a);
        return 1;
    }
    args.store = a;
    args.dir = dir;
    args.commits = 0;
    if (cdc_store_lock_test(b) != CDC_STORE_OK ||
        !samep_blocked_run(b, samep_snapshot_body, &args, &code,
                           &premature) ||
        code != (char)CDC_STORE_OK) {
        fprintf(stderr, "store-samep FAIL transition: snapshot run\n");
        cdc_store_close(a);
        cdc_store_close(b);
        return 1;
    }
    if (premature) {
        fprintf(stderr, "store-samep FAIL transition: snapshot ran while "
                        "the section was held\n");
        cdc_store_close(a);
        cdc_store_close(b);
        return 1;
    }
    if (cdc_store_lock_test(b) != CDC_STORE_OK ||
        !samep_blocked_run(b, samep_compact_body, &args, &code,
                           &premature) ||
        code != (char)CDC_STORE_OK) {
        fprintf(stderr, "store-samep FAIL transition: compact run\n");
        cdc_store_close(a);
        cdc_store_close(b);
        return 1;
    }
    if (premature) {
        fprintf(stderr, "store-samep FAIL transition: compact ran while "
                        "the section was held\n");
        cdc_store_close(a);
        cdc_store_close(b);
        return 1;
    }
    /* the un-compacted handle: typed refusal, then a clean reopen */
    if (cdc_store_stage(b, "stale", 5) != CDC_STORE_OK ||
        cdc_store_commit(b) != CDC_STORE_ESTATE) {
        fprintf(stderr, "store-samep FAIL transition: stale handle was "
                        "not refused with ESTATE\n");
        cdc_store_close(a);
        cdc_store_close(b);
        return 1;
    }
    cdc_store_rollback(b);
    cdc_store_close(b);
    b = NULL;
    if (cdc_store_open(dir, &b, NULL) != CDC_STORE_OK ||
        cdc_store_generation(b) != 1 || !store_commit_txn(b, 99)) {
        fprintf(stderr, "store-samep FAIL transition: reopen commit\n");
        cdc_store_close(a);
        cdc_store_close(b);
        return 1;
    }
    cdc_store_close(a);
    cdc_store_close(b);
    if (cdc_store_open(dir, &check, NULL) != CDC_STORE_OK ||
        cdc_store_verify(check) != CDC_STORE_OK ||
        cdc_store_generation(check) != 1 ||
        cdc_store_sealed_count(check) != 4) {
        fprintf(stderr, "store-samep FAIL transition: final state\n");
        cdc_store_close(check);
        return 1;
    }
    cdc_store_close(check);
    printf("samep-transition snapshot-blocked=1 compact-blocked=1 "
           "stale-commit=state reopen-commit=ok generation=1\n");
    return 0;
}

/* Check 4: reset is excluded by a held section; a handle whose store was
 * reset under it is refused typed (ECORRUPT), and a fresh open starts a
 * fresh generation-0 store. */
static int samep_check_reset(const char *base) {
    char dir[560];
    cdc_store *a = NULL, *fresh = NULL;
    samep_args args;
    char code = 0;
    int premature = 0;

    snprintf(dir, sizeof(dir), "%s/reset", base);
    if (!store_seed_dir(dir, 2) ||
        cdc_store_open(dir, &a, NULL) != CDC_STORE_OK) {
        fprintf(stderr, "store-samep FAIL reset: seed\n");
        return 1;
    }
    args.store = NULL;
    args.dir = dir;
    args.commits = 0;
    if (cdc_store_lock_test(a) != CDC_STORE_OK ||
        !samep_blocked_run(a, samep_reset_body, &args, &code, &premature) ||
        code != (char)CDC_STORE_OK) {
        fprintf(stderr, "store-samep FAIL reset: reset run\n");
        cdc_store_close(a);
        return 1;
    }
    if (premature) {
        fprintf(stderr, "store-samep FAIL reset: reset deleted the log "
                        "while the section was held\n");
        cdc_store_close(a);
        return 1;
    }
    if (cdc_store_stage(a, "gone", 4) != CDC_STORE_OK ||
        cdc_store_commit(a) != CDC_STORE_ECORRUPT) {
        fprintf(stderr, "store-samep FAIL reset: stale handle was not "
                        "refused with ECORRUPT\n");
        cdc_store_close(a);
        return 1;
    }
    cdc_store_rollback(a);
    cdc_store_close(a);
    if (cdc_store_open(dir, &fresh, NULL) != CDC_STORE_OK ||
        cdc_store_generation(fresh) != 0 ||
        cdc_store_sealed_count(fresh) != 0 ||
        cdc_store_verify(fresh) != CDC_STORE_OK) {
        fprintf(stderr, "store-samep FAIL reset: fresh open\n");
        cdc_store_close(fresh);
        return 1;
    }
    cdc_store_close(fresh);
    printf("samep-reset blocked=1 stale-commit=corrupt-tail "
           "fresh generation=0 sealed=0\n");
    return 0;
}

/* Check 5: the POSIX close hazard. Closing one handle must NOT release
 * the lock another handle in the same process is holding — a foreign
 * process must still be refused. */
static int samep_check_close(const char *base) {
    char dir[560];
    char lock_path[620];
    cdc_store *a = NULL, *b = NULL;
    pid_t pid;
    int status;

    snprintf(dir, sizeof(dir), "%s/close", base);
    snprintf(lock_path, sizeof(lock_path), "%s/lock.cdcstore", dir);
    if (!store_seed_dir(dir, 1) ||
        cdc_store_open(dir, &a, NULL) != CDC_STORE_OK ||
        cdc_store_open(dir, &b, NULL) != CDC_STORE_OK) {
        fprintf(stderr, "store-samep FAIL close: seed\n");
        cdc_store_close(a);
        return 1;
    }
    if (cdc_store_lock_test(a) != CDC_STORE_OK) {
        fprintf(stderr, "store-samep FAIL close: hold\n");
        cdc_store_close(a);
        cdc_store_close(b);
        return 1;
    }
    cdc_store_close(b); /* the hazard: this used to drop A's lock */
    fflush(NULL);
    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "store-samep FAIL close: fork\n");
        cdc_store_unlock_test(a);
        cdc_store_close(a);
        return 1;
    }
    if (pid == 0) {
        struct flock probe;
        int fd = open(lock_path, O_RDWR);
        if (fd < 0) {
            _exit(90);
        }
        memset(&probe, 0, sizeof(probe));
        probe.l_type = F_WRLCK;
        probe.l_whence = SEEK_SET;
        if (fcntl(fd, F_SETLK, &probe) == 0) {
            _exit(1); /* acquired: the close dropped the lock */
        }
        if (errno != EAGAIN && errno != EACCES) {
            _exit(92);
        }
        _exit(0); /* refused: the lock survived the close */
    }
    waitpid(pid, &status, 0);
    cdc_store_unlock_test(a);
    cdc_store_close(a);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "store-samep FAIL close: closing one handle "
                        "released the other handle's lock (child=%d)\n",
                WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        return 1;
    }
    printf("samep-close closed-one-handle=1 lock-preserved=1 "
           "foreign-acquire=refused\n");
    return 0;
}

/* Check 6: the 1->0->1 lifecycle (second 2026-07-28 review, finding 2).
 * The LAST release must not let a new first opener register a fresh
 * coordination object — and take the process file lock through a NEW
 * descriptor — before the old descriptor closes: POSIX owns record locks
 * by (process, file), so that stale close would silently drop the new
 * lock. The release is held open at its ordering-critical point by the
 * test pause hook; a new opener and a foreign contender then attempt
 * entry. */
static int samep_check_lifecycle(const char *base) {
    char dir[560];
    char lock_path[620];
    cdc_store *a = NULL;
    samep_args open_args;
    samep_args close_args;
    pthread_t closer, opener;
    int pause_sig[2], pause_go[2], closed[2], opened[2];
    char code = 0;
    int premature;
    int probe;

    snprintf(dir, sizeof(dir), "%s/lifecycle", base);
    snprintf(lock_path, sizeof(lock_path), "%s/lock.cdcstore", dir);
    if (!store_seed_dir(dir, 1) ||
        cdc_store_open(dir, &a, NULL) != CDC_STORE_OK) {
        fprintf(stderr, "store-samep FAIL lifecycle: seed\n");
        return 1;
    }
    if (pipe(pause_sig) != 0 || pipe(pause_go) != 0 || pipe(closed) != 0 ||
        pipe(opened) != 0) {
        fprintf(stderr, "store-samep FAIL lifecycle: pipes\n");
        cdc_store_close(a);
        return 1;
    }
    cdc_store_set_release_pause(pause_sig[1], pause_go[0]);
    close_args.store = a;
    close_args.dir = dir;
    close_args.commits = 0;
    close_args.signal_fd = closed[1];
    if (pthread_create(&closer, NULL, samep_close_body, &close_args) != 0) {
        fprintf(stderr, "store-samep FAIL lifecycle: closer thread\n");
        cdc_store_close(a);
        return 1;
    }
    if (!samep_readable(pause_sig[0], 8000) ||
        !samep_wait_code(pause_sig[0], &code) || code != 'p') {
        fprintf(stderr, "store-samep FAIL lifecycle: release pause never "
                        "fired\n");
        samep_signal(pause_go[1], 'g');
        pthread_join(closer, NULL);
        return 1;
    }
    open_args.store = NULL;
    open_args.dir = dir;
    open_args.commits = 0;
    open_args.signal_fd = opened[1];
    open_args.opened = NULL;
    if (pthread_create(&opener, NULL, samep_open_keep_body, &open_args) !=
        0) {
        fprintf(stderr, "store-samep FAIL lifecycle: opener thread\n");
        samep_signal(pause_go[1], 'g');
        pthread_join(closer, NULL);
        return 1;
    }
    premature = samep_readable(opened[0], 300);
    if (premature) {
        /* The window is open: the opener registered a replacement while
         * the last release was still in flight. Demonstrate the harm end
         * to end — take the new lock, let the stale close land, and show
         * a foreign process can now walk straight in. */
        const char *harm = "window observed";
        samep_wait_code(opened[0], &code);
        pthread_join(opener, NULL);
        if (code == 'o' && open_args.opened &&
            cdc_store_lock_test(open_args.opened) == CDC_STORE_OK) {
            samep_signal(pause_go[1], 'g');
            pthread_join(closer, NULL);
            samep_wait_code(closed[0], &code);
            probe = samep_foreign_probe(lock_path);
            harm = probe == 1
                       ? "and the stale close dropped the fresh lock"
                       : "stale close landed, lock survived";
            cdc_store_unlock_test(open_args.opened);
        } else {
            samep_signal(pause_go[1], 'g');
            pthread_join(closer, NULL);
            samep_wait_code(closed[0], &code);
        }
        if (open_args.opened) {
            cdc_store_close(open_args.opened);
        }
        fprintf(stderr, "store-samep FAIL lifecycle: a new opener "
                        "registered during the last release window (%s)\n",
                harm);
        close(pause_sig[0]);
        close(pause_sig[1]);
        close(pause_go[1]);
        close(closed[0]);
        close(closed[1]);
        close(opened[0]);
        close(opened[1]);
        return 1;
    }
    /* Fixed ordering: the opener is excluded until the release fully
     * lands (descriptor closed under the registry lock). */
    samep_signal(pause_go[1], 'g');
    pthread_join(closer, NULL);
    if (!samep_wait_code(closed[0], &code) || code != 'c') {
        fprintf(stderr, "store-samep FAIL lifecycle: close never "
                        "completed\n");
        pthread_join(opener, NULL);
        return 1;
    }
    if (!samep_readable(opened[0], 8000) ||
        !samep_wait_code(opened[0], &code) || code != 'o') {
        fprintf(stderr, "store-samep FAIL lifecycle: reopen never "
                        "completed after the release\n");
        pthread_join(opener, NULL);
        return 1;
    }
    pthread_join(opener, NULL);
    if (!open_args.opened ||
        cdc_store_lock_test(open_args.opened) != CDC_STORE_OK) {
        fprintf(stderr, "store-samep FAIL lifecycle: fresh handle cannot "
                        "hold the section\n");
        if (open_args.opened) {
            cdc_store_close(open_args.opened);
        }
        return 1;
    }
    probe = samep_foreign_probe(lock_path);
    cdc_store_unlock_test(open_args.opened);
    cdc_store_close(open_args.opened);
    close(pause_sig[0]);
    close(pause_sig[1]);
    close(pause_go[1]);
    close(closed[0]);
    close(closed[1]);
    close(opened[0]);
    close(opened[1]);
    if (probe != 0) {
        fprintf(stderr, "store-samep FAIL lifecycle: foreign probe "
                        "result=%d (the fresh handle's lock must exclude "
                        "other processes)\n",
                probe);
        return 1;
    }
    printf("samep-lifecycle last-close-window=closed new-opener-blocked=1 "
           "foreign-acquire=refused\n");
    return 0;
}

static int cmd_store_samep(const char *base) {
    int failed = 0;
    failed += samep_check_open(base);
    failed += samep_check_commit(base);
    failed += samep_check_transition(base);
    failed += samep_check_reset(base);
    failed += samep_check_close(base);
    failed += samep_check_lifecycle(base);
    if (failed) {
        fprintf(stderr, "store-samep FAIL failed=%d/6\n", failed);
        return 1;
    }
    printf("store-samep ok checks=6/6 shared-coordination=1\n");
    return 0;
}

static int cmd_store_protocol(const char *base) {
    char dir[512], log_path[600], snap_path[620];
    cdc_store *store = NULL, *other = NULL;
    char before[80], after[80], attest_before[80], attest_after[80];
    uint8_t *snap_bytes = NULL;
    size_t snap_size = 0;
    long log_size_before, log_size_after;
    int failures = 0;
    int t;

    snprintf(dir, sizeof(dir), "%s/protocol", base);
    snprintf(log_path, sizeof(log_path), "%s/log.cdcstore", dir);
    snprintf(snap_path, sizeof(snap_path), "%s/snapshot.cdcstore", dir);
    if (cdc_store_open(dir, &store, NULL) != CDC_STORE_OK) {
        fprintf(stderr, "store-protocol FAIL: open\n");
        return 1;
    }
    for (t = 1; t <= 3; t++) {
        if (!store_commit_txn(store, t)) {
            fprintf(stderr, "store-protocol FAIL: seed txn %d\n", t);
            return 1;
        }
    }
    if (cdc_store_replay(store, before, sizeof(before)) != CDC_STORE_OK ||
        cdc_store_attest(store, attest_before, sizeof(attest_before)) !=
            CDC_STORE_OK) {
        fprintf(stderr, "store-protocol FAIL: baseline digests\n");
        return 1;
    }
    log_size_before = file_size(log_path);

    /* compact without a snapshot must refuse rather than discard history */
    if (cdc_store_compact(store) != CDC_STORE_ESTATE) {
        fprintf(stderr, "store-protocol FAIL: compact without snapshot\n");
        failures++;
    }
    if (cdc_store_snapshot(store) != CDC_STORE_OK) {
        fprintf(stderr, "store-protocol FAIL: snapshot\n");
        return 1;
    }
    if (cdc_store_compact(store) != CDC_STORE_OK) {
        fprintf(stderr, "store-protocol FAIL: compact\n");
        return 1;
    }
    log_size_after = file_size(log_path);
    if (cdc_store_replay(store, after, sizeof(after)) != CDC_STORE_OK ||
        cdc_store_attest(store, attest_after, sizeof(attest_after)) !=
            CDC_STORE_OK) {
        fprintf(stderr, "store-protocol FAIL: post-compaction digests\n");
        return 1;
    }
    /* THE claim: semantic identity survives the physical rewrite. */
    if (strcmp(before, after) != 0) {
        fprintf(stderr, "store-protocol FAIL: replay identity changed\n  %s\n  %s\n",
                before, after);
        failures++;
    }
    if (strcmp(attest_before, attest_after) == 0) {
        fprintf(stderr, "store-protocol FAIL: attest digest should change\n");
        failures++;
    }
    /* The compacted log is exactly one HEAD record: the base lives IN the
     * log now, so there is no second file for a crash to leave dangling. */
    if (log_size_after >= log_size_before || log_size_after <= 0) {
        fprintf(stderr, "store-protocol FAIL: log not compacted (%ld -> %ld)\n",
                log_size_before, log_size_after);
        failures++;
    }
    if (cdc_store_generation(store) != 1) {
        fprintf(stderr, "store-protocol FAIL: generation did not advance\n");
        failures++;
    }
    if (cdc_store_sealed_count(store) != 3) {
        fprintf(stderr, "store-protocol FAIL: sealed count lost by compaction\n");
        failures++;
    }
    cdc_store_close(store);
    store = NULL;

    /* reopening must resume from the base and keep appending coherently */
    if (cdc_store_open(dir, &store, NULL) != CDC_STORE_OK ||
        cdc_store_sealed_count(store) != 3) {
        fprintf(stderr, "store-protocol FAIL: reopen after compaction\n");
        return 1;
    }
    {
        char resumed[80];
        if (cdc_store_replay(store, resumed, sizeof(resumed)) != CDC_STORE_OK ||
            strcmp(resumed, before) != 0) {
            fprintf(stderr, "store-protocol FAIL: replay identity after reopen\n");
            failures++;
        }
    }
    if (!store_commit_txn(store, 4) || cdc_store_sealed_count(store) != 4) {
        fprintf(stderr, "store-protocol FAIL: append after compaction\n");
        failures++;
    }

    /* fence: an armed writer whose view is stale must not commit */
    if (cdc_store_fence(store, 4) != CDC_STORE_OK) {
        fprintf(stderr, "store-protocol FAIL: fence at current seal\n");
        failures++;
    }
    if (cdc_store_fence(store, 2) != CDC_STORE_ESTATE) {
        fprintf(stderr, "store-protocol FAIL: stale fence accepted\n");
        failures++;
    }
    if (cdc_store_open(dir, &other, NULL) != CDC_STORE_OK) {
        fprintf(stderr, "store-protocol FAIL: second handle\n");
        return 1;
    }
    /* both writers believe the log ends at seal 4 */
    if (cdc_store_fence(store, 4) != CDC_STORE_OK ||
        cdc_store_fence(other, 4) != CDC_STORE_OK) {
        fprintf(stderr, "store-protocol FAIL: concurrent fences\n");
        failures++;
    }
    if (!store_commit_txn(store, 5)) {
        fprintf(stderr, "store-protocol FAIL: winner commit\n");
        failures++;
    }
    {
        /* the loser's fence is now stale: its commit must be refused and
         * must leave the log byte-identical */
        long size_before_loser = file_size(log_path);
        char payload[32];
        cdc_store_status status;
        snprintf(payload, sizeof(payload), "stale-writer");
        cdc_store_stage(other, payload, strlen(payload));
        status = cdc_store_commit(other);
        if (status != CDC_STORE_ESTATE) {
            fprintf(stderr, "store-protocol FAIL: stale writer committed (%s)\n",
                    cdc_store_status_name(status));
            failures++;
        }
        if (file_size(log_path) != size_before_loser) {
            fprintf(stderr, "store-protocol FAIL: stale commit wrote bytes\n");
            failures++;
        }
    }
    cdc_store_close(other);
    cdc_store_close(store);
    store = NULL;

    /* The compaction base is now the log's own HEAD record, so tampering
     * with it is tampering with the log: every byte of a compacted log
     * must fail closed. This replaces the old separate-snapshot sweep,
     * which could only ever check a file open() should not have trusted. */
    {
        char cdir[512], clog[600];
        cdc_store *seed = NULL;
        snprintf(cdir, sizeof(cdir), "%s/head-sweep", base);
        snprintf(clog, sizeof(clog), "%s/log.cdcstore", cdir);
        if (cdc_store_open(cdir, &seed, NULL) != CDC_STORE_OK ||
            !store_commit_txn(seed, 1) ||
            cdc_store_snapshot(seed) != CDC_STORE_OK ||
            cdc_store_compact(seed) != CDC_STORE_OK) {
            fprintf(stderr, "store-protocol FAIL: seed head sweep\n");
            return 1;
        }
        cdc_store_close(seed);
        if (!read_file_bytes(clog, &snap_bytes, &snap_size)) {
            fprintf(stderr, "store-protocol FAIL: read compacted log\n");
            return 1;
        }
        {
            size_t offset;
            int rejected = 0, checked = 0;
            for (offset = 0; offset < snap_size; offset++) {
                uint8_t original = snap_bytes[offset];
                cdc_store *probe = NULL;
                snap_bytes[offset] ^= 0xff;
                write_file_bytes(clog, snap_bytes, snap_size);
                if (cdc_store_open(cdir, &probe, NULL) == CDC_STORE_ECORRUPT &&
                    probe == NULL) {
                    rejected++;
                } else {
                    fprintf(stderr,
                            "store-protocol FAIL: HEAD byte %zu accepted\n",
                            offset);
                    cdc_store_close(probe);
                    failures++;
                }
                checked++;
                snap_bytes[offset] = original;
            }
            write_file_bytes(clog, snap_bytes, snap_size);
            printf("store-protocol HEAD sweep: %d/%d bytes fail closed\n",
                   rejected, checked);
        }
        free(snap_bytes);
        snap_bytes = NULL;
    }
    (void)snap_path;
    if (failures) {
        return 1;
    }
    printf("store-protocol ok snapshot=1 compact=1 fence=1 "
           "replay-identity-preserved=1\n");
    return 0;
}

/* ---- store corruption counterexamples (review B1/B2) ---------------- */

static int read_file_bytes(const char *path, uint8_t **out, size_t *size) {
    FILE *fp = fopen(path, "rb");
    long end;
    if (!fp) {
        return 0;
    }
    fseek(fp, 0, SEEK_END);
    end = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    *out = malloc((size_t)end ? (size_t)end : 1);
    if (!*out || fread(*out, 1, (size_t)end, fp) != (size_t)end) {
        free(*out);
        fclose(fp);
        return 0;
    }
    fclose(fp);
    *size = (size_t)end;
    return 1;
}

static int write_file_bytes(const char *path, const uint8_t *bytes,
                            size_t size) {
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        return 0;
    }
    if (fwrite(bytes, 1, size, fp) != size) {
        fclose(fp);
        return 0;
    }
    return fclose(fp) == 0;
}

enum {
    STORE_FRAMING_SIZE = 4 + 1 + 8 + 4,
    STORE_HEADER_SIZE = STORE_FRAMING_SIZE + 2 * CDC_DIGEST_SIZE
};

/* One corruption case: mutate one byte, expect open to fail closed with
 * ECORRUPT, no handle, recovered_out untouched (0), and the log bytes
 * byte-identical (3122af5 re-review contract). */
static int corrupt_case(const char *dir, const char *log_path,
                        const char *name, const uint8_t *orig, size_t size,
                        size_t offset, uint8_t new_value, int verbose) {
    uint8_t *mutated;
    uint8_t *after = NULL;
    size_t after_size = 0;
    cdc_store *store = NULL;
    int recovered = -1;
    cdc_store_status status;
    int ok = 1;

    mutated = malloc(size);
    if (!mutated) {
        return 0;
    }
    memcpy(mutated, orig, size);
    mutated[offset] = new_value;
    if (mutated[offset] == orig[offset]) {
        mutated[offset] ^= 0xff;
    }
    if (!write_file_bytes(log_path, mutated, size)) {
        free(mutated);
        return 0;
    }
    status = cdc_store_open(dir, &store, &recovered);
    if (status != CDC_STORE_ECORRUPT) {
        fprintf(stderr, "store-corrupt FAIL %s offset=%zu: open -> %s\n",
                name, offset, cdc_store_status_name(status));
        ok = 0;
    }
    if (store != NULL) {
        fprintf(stderr, "store-corrupt FAIL %s offset=%zu: handle\n", name,
                offset);
        cdc_store_close(store);
        ok = 0;
    }
    if (recovered != 0) {
        fprintf(stderr,
                "store-corrupt FAIL %s offset=%zu: recovered=%d\n", name,
                offset, recovered);
        ok = 0;
    }
    if (!read_file_bytes(log_path, &after, &after_size) ||
        after_size != size || memcmp(after, mutated, size) != 0) {
        fprintf(stderr, "store-corrupt FAIL %s offset=%zu: log mutated\n",
                name, offset);
        ok = 0;
    }
    free(after);
    free(mutated);
    if (ok && verbose) {
        printf("store-corrupt ok case=%s offset=%zu\n", name, offset);
    }
    return ok;
}

static int cmd_store_corrupt(const char *base) {
    char src_dir[512], src_log[600];
    char sweep_dir[512], sweep_log[600];
    uint8_t *orig = NULL;
    size_t size = 0;
    size_t swept = 0;
    int failures = 0;
    cdc_store *store = NULL;

    /* reference store: 2 sealed transactions, 3 events each */
    if (!store_reference_digest(base, "corrupt_src", 2, src_log,
                                sizeof(src_log))) {
        fprintf(stderr, "store-corrupt FAIL: reference store\n");
        return 1;
    }
    snprintf(src_dir, sizeof(src_dir), "%s/corrupt_src", base);
    snprintf(src_log, sizeof(src_log), "%s/log.cdcstore", src_dir);
    if (!read_file_bytes(src_log, &orig, &size)) {
        fprintf(stderr, "store-corrupt FAIL: read reference log\n");
        return 1;
    }
    snprintf(sweep_dir, sizeof(sweep_dir), "%s/sweep", base);
    snprintf(sweep_log, sizeof(sweep_log), "%s/log.cdcstore", sweep_dir);
    if (mkdir(sweep_dir, 0777) != 0 && errno != EEXIST) {
        free(orig);
        return 1;
    }

    /* Full mutation matrix (re-review item 6): EVERY byte of the sealed
     * log — all magic, type, sequence, length, framing-tag, payload-digest,
     * payload, and seal bytes — flipped one at a time; each must fail
     * closed with the evidence untouched. */
    {
        size_t offset;
        for (offset = 0; offset < size; offset++) {
            if (!corrupt_case(sweep_dir, sweep_log, "sweep", orig, size,
                              offset, (uint8_t)(orig[offset] ^ 0xff), 0)) {
                failures++;
            }
            swept++;
        }
    }
    /* Re-review item 7: the exact high-byte length mutation, named. The
     * first DATA record's length field sits at offset 13..16; flipping
     * offset 16 declares a multi-megabyte payload in a small file. */
    failures += !corrupt_case(sweep_dir, sweep_log,
                              "high-byte-length", orig, size, 16,
                              (uint8_t)(orig[16] + 1), 1);

    /* control 1: unmutated copy opens clean with 2 seals */
    {
        char dir[512], log_path[600];
        int recovered = -1;
        snprintf(dir, sizeof(dir), "%s/control_clean", base);
        snprintf(log_path, sizeof(log_path), "%s/log.cdcstore", dir);
        mkdir(dir, 0777);
        write_file_bytes(log_path, orig, size);
        if (cdc_store_open(dir, &store, &recovered) != CDC_STORE_OK ||
            recovered != 0 || cdc_store_sealed_count(store) != 2) {
            fprintf(stderr, "store-corrupt FAIL: clean control\n");
            failures++;
        }
        cdc_store_close(store);
        store = NULL;
    }
    /* control 2: a fully valid but unsealed DATA tail (correct framing
     * tag, digest, and sequence) is recoverable (latch-or-hold),
     * distinguished from corruption */
    {
        char dir[512], log_path[600];
        uint8_t record[STORE_HEADER_SIZE + 5];
        uint8_t digest[CDC_DIGEST_SIZE];
        int recovered = -1;
        FILE *fp;
        snprintf(dir, sizeof(dir), "%s/control_tail", base);
        snprintf(log_path, sizeof(log_path), "%s/log.cdcstore", dir);
        mkdir(dir, 0777);
        write_file_bytes(log_path, orig, size);
        memcpy(record, "CDC2", 4);
        record[4] = 'D';
        record[5] = 7; /* event seq 7 (6 sealed events precede) */
        memset(record + 6, 0, 7);
        record[13] = 5; /* payload length 5, little-endian */
        memset(record + 14, 0, 3);
        cdc_digest(record, STORE_FRAMING_SIZE, record + STORE_FRAMING_SIZE);
        cdc_digest("extra", 5, digest);
        memcpy(record + STORE_FRAMING_SIZE + CDC_DIGEST_SIZE, digest,
               CDC_DIGEST_SIZE);
        memcpy(record + STORE_HEADER_SIZE, "extra", 5);
        fp = fopen(log_path, "ab");
        fwrite(record, 1, sizeof(record), fp);
        fclose(fp);
        if (cdc_store_open(dir, &store, &recovered) != CDC_STORE_OK ||
            recovered != 1 || cdc_store_sealed_count(store) != 2) {
            fprintf(stderr, "store-corrupt FAIL: unsealed-tail control\n");
            failures++;
        }
        cdc_store_close(store);
        store = NULL;
    }
    free(orig);
    if (failures) {
        return 1;
    }
    printf("store-corrupt ok swept=%zu named=1 controls=2\n", swept);
    return 0;
}

/* ---- store I/O-fault regressions (f1f68c0 re-review) ---------------- */

static int store_io_case(const char *name, const char *dir,
                         const uint8_t *expect_bytes, size_t expect_size) {
    cdc_store *store = NULL;
    int recovered = -1;
    cdc_store_status status = cdc_store_open(dir, &store, &recovered);
    int ok = 1;
    if (status != CDC_STORE_EIO) {
        fprintf(stderr, "store-io FAIL %s: open -> %s\n", name,
                cdc_store_status_name(status));
        ok = 0;
    }
    if (store != NULL) {
        fprintf(stderr, "store-io FAIL %s: handle returned\n", name);
        cdc_store_close(store);
        ok = 0;
    }
    if (expect_bytes != NULL) {
        char log_path[600];
        uint8_t *after = NULL;
        size_t after_size = 0;
        snprintf(log_path, sizeof(log_path), "%s/log.cdcstore", dir);
        if (!read_file_bytes(log_path, &after, &after_size) ||
            after_size != expect_size ||
            memcmp(after, expect_bytes, expect_size) != 0) {
            fprintf(stderr, "store-io FAIL %s: log mutated (size %zu)\n",
                    name, after_size);
            ok = 0;
        }
        free(after);
    }
    if (ok) {
        printf("store-io ok case=%s\n", name);
    }
    return ok;
}

static int cmd_store_io(const char *base) {
    char dir[512], log_path[600], digest_out[80];
    uint8_t *orig = NULL;
    size_t size = 0;
    int failures = 0;

    /* regression 1: log.cdcstore is a directory -> EIO, no handle */
    snprintf(dir, sizeof(dir), "%s/iodir", base);
    snprintf(log_path, sizeof(log_path), "%s/log.cdcstore", dir);
    mkdir(dir, 0777);
    mkdir(log_path, 0777);
    failures += !store_io_case("dir-as-log", dir, NULL, 0);

    /* reference store with 2 sealed transactions for the injection arms */
    if (!store_reference_digest(base, "io_src", 2, digest_out,
                                sizeof(digest_out))) {
        fprintf(stderr, "store-io FAIL: reference store\n");
        return 1;
    }
    snprintf(dir, sizeof(dir), "%s/io_src", base);
    snprintf(log_path, sizeof(log_path), "%s/log.cdcstore", dir);
    if (!read_file_bytes(log_path, &orig, &size)) {
        fprintf(stderr, "store-io FAIL: read reference log\n");
        return 1;
    }

    /* regression 2: injected mid-read fault before any seal (first scan
     * read) -> EIO, bytes unchanged */
    cdc_store_set_read_fail_after(1);
    failures += !store_io_case("fault-before-seal", dir, orig, size);
    cdc_store_set_read_fail_after(0);

    /* regression 3: injected mid-read fault after the first seal (each
     * txn = 3 DATA x 2 reads + 1 SEAL read = 7 reads; fault at read 8)
     * -> EIO, bytes unchanged, NO truncation of the sealed prefix */
    cdc_store_set_read_fail_after(8);
    failures += !store_io_case("fault-after-seal", dir, orig, size);
    cdc_store_set_read_fail_after(0);

    /* control: disarmed, the same store opens clean with 2 seals */
    {
        cdc_store *store = NULL;
        int recovered = -1;
        if (cdc_store_open(dir, &store, &recovered) != CDC_STORE_OK ||
            recovered != 0 || cdc_store_sealed_count(store) != 2) {
            fprintf(stderr, "store-io FAIL: disarmed control\n");
            failures++;
        }
        cdc_store_close(store);
    }
    free(orig);
    if (failures) {
        return 1;
    }
    printf("store-io ok cases=3 controls=1\n");
    return 0;
}

/* ---- reject --------------------------------------------------------- */

static int cmd_reject(int argc, char **argv) {
    int i;
    for (i = 0; i < argc; i++) {
        cdc_unit program;
        cdc_diag_list diags;
        cdc_diag_list_init(&diags);
        cdc_unit_parse_file(argv[i], &program, &diags);
        if (diags.errors == 0) {
            fprintf(stderr, "reject FAIL %s: accepted\n", argv[i]);
            return 1;
        }
        printf("frontend reject ok %s code=%s\n", base_name(argv[i]),
               diags.items ? diags.items[0].code : "CDC900");
        cdc_unit_free(&program);
        cdc_diag_list_free(&diags);
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr,
                "usage: cdc_frontend_check "
                "dump|canon|roundtrip|bounds|oom|reject ...\n");
        return 2;
    }
    if (strcmp(argv[1], "dump") == 0) {
        return cmd_dump(argc - 2, argv + 2);
    }
    if (strcmp(argv[1], "canon") == 0) {
        return cmd_canon(argc - 2, argv + 2);
    }
    if (strcmp(argv[1], "roundtrip") == 0) {
        return cmd_roundtrip(argc - 2, argv + 2);
    }
    if (strcmp(argv[1], "bounds") == 0) {
        return cmd_bounds();
    }
    if (strcmp(argv[1], "oom") == 0 && argc >= 3) {
        return cmd_oom(argv[2]);
    }
    if (strcmp(argv[1], "reject") == 0) {
        return cmd_reject(argc - 2, argv + 2);
    }
    if (strcmp(argv[1], "abi-diag") == 0 && argc >= 3) {
        return cmd_abi_diag(argv[2]);
    }
    if (strcmp(argv[1], "abi-io") == 0 && argc >= 4) {
        return cmd_abi_io(argv[2], argv[3]);
    }
    if (strcmp(argv[1], "io-mid-read") == 0 && argc >= 3) {
        return cmd_io_mid_read(argv[2]);
    }
    if (strcmp(argv[1], "oom-abi") == 0 && argc >= 3) {
        return cmd_oom_abi(argv[2]);
    }
    if (strcmp(argv[1], "store-crash") == 0 && argc >= 3) {
        return cmd_store_crash(argv[2]);
    }
    if (strcmp(argv[1], "store-check") == 0 && argc >= 3) {
        return cmd_store_check(argv[2]);
    }
    if (strcmp(argv[1], "store-corrupt") == 0 && argc >= 3) {
        return cmd_store_corrupt(argv[2]);
    }
    if (strcmp(argv[1], "store-kill") == 0 && argc >= 3) {
        return cmd_store_kill(argv[2]);
    }
    if (strcmp(argv[1], "attr-boundary") == 0) {
        return cmd_attr_boundary();
    }
    if (strcmp(argv[1], "corpus-digest") == 0 && argc >= 3) {
        return cmd_corpus_digest(argc - 2, argv + 2);
    }
    if (strcmp(argv[1], "store-inspect") == 0 && argc >= 3) {
        return cmd_store_inspect(argv[2]);
    }
    if (strcmp(argv[1], "vectors-from-report") == 0 && argc >= 3) {
        return cmd_vectors_from_report(argv[2]);
    }
    if (strcmp(argv[1], "receipt-check") == 0) {
        return cmd_receipt_check();
    }
    if (strcmp(argv[1], "store-generation") == 0 && argc >= 3) {
        return cmd_store_generation(argv[2]);
    }
    if (strcmp(argv[1], "store-samep") == 0 && argc >= 3) {
        return cmd_store_samep(argv[2]);
    }
    if (strcmp(argv[1], "store-race") == 0 && argc >= 3) {
        return cmd_store_race(argv[2]);
    }
    if (strcmp(argv[1], "store-protocol") == 0 && argc >= 3) {
        return cmd_store_protocol(argv[2]);
    }
    if (strcmp(argv[1], "store-io") == 0 && argc >= 3) {
        return cmd_store_io(argv[2]);
    }
    if (strcmp(argv[1], "digest-vectors") == 0 && argc >= 3) {
        return cmd_digest_vectors(argv[2]);
    }
    if (strcmp(argv[1], "digest-file") == 0 && argc >= 3) {
        return cmd_digest_file(argc - 2, argv + 2);
    }
    fprintf(stderr, "cdc_frontend_check: unknown mode '%s'\n", argv[1]);
    return 2;
}
