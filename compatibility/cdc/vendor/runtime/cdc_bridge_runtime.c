#include "cdc_ast.h"
#include "cdc_diagnostic.h"
#include "cdc_parser.h"
#include "cdc_source.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BRIDGE64_ROWS 64
#define LINE_MAX_BYTES 1024
#define MAX_CODEBOOK_ARITY 24

typedef struct {
    char witness[64];
    char dyadic[7];
    char triadic[4];
    int index;
    int seen;
} BridgeRow;

static const char DIGITS[] = "0123456789ABCDEF";

/* ---- grammar-1 source access (deletion gate step 1) ------------------
 *
 * These loops used to read the file themselves: fgets, trim the newline,
 * check a LINE PREFIX, then pull attributes out of the raw text. That is
 * the legacy scanner, and it is the thing the deletion gate removes.
 *
 * They now consume the same parsed statement stream every other consumer
 * sees. Two differences are worth naming rather than discovering later:
 *
 *  - A prefix test like `cdc_starts_with(line, "witness bridge64-")` was
 *    sensitive to spacing; the statement form tests the DIRECTIVE and its
 *    first ARGUMENT, so `witness  bridge64-x` (two spaces) now matches
 *    where it previously did not. That is a correction, and the generated
 *    codebooks use single spaces, so no output moves.
 *  - Attribute lookup uses cdc_stmt_attr_first, which replicates the legacy
 *    first-occurrence rule exactly (cdc_ast.h). Values agree because no
 *    attribute these runtimes consume is ever quoted — the precondition
 *    gated in scripts/verify.sh (D23).
 */
static void fail(const char *message);

/* Parses `path` or fails closed with the parser's typed diagnostic. */
static void load_unit(const char *path, cdc_unit *unit) {
    cdc_diag_list diags;
    cdc_diag_list_init(&diags);
    cdc_unit_init(unit);
    if (!cdc_unit_parse_file(path, unit, &diags) || diags.errors > 0) {
        const char *detail = diags.count > 0 ? diags.items[0].message : "parse failed";
        fprintf(stderr, "cdc-bridge-runtime: %s: %s\n", path, detail);
        cdc_diag_list_free(&diags);
        cdc_unit_free(unit);
        exit(1);
    }
    cdc_diag_list_free(&diags);
}

/* A `witness <id>` statement whose id starts with `prefix`, or NULL. */
static const char *witness_id_with_prefix(const cdc_stmt *stmt,
                                          const char *prefix) {
    const char *directive = cdc_stmt_directive(stmt);
    const char *id;
    if (!directive || strcmp(directive, "witness") != 0) {
        return NULL;
    }
    id = cdc_stmt_arg(stmt, 0);
    if (!id || strncmp(id, prefix, strlen(prefix)) != 0) {
        return NULL;
    }
    return id;
}

/* Same contract as the legacy cdc_read_attr — returns 0 when absent, fails
 * closed on overflow — but sourced from the parsed statement, so the loop
 * bodies below are unchanged. */
static int stmt_attr_copy(const cdc_stmt *stmt, const char *key, char *out,
                          size_t out_size) {
    const char *value = cdc_stmt_attr_first(stmt, key);
    int written;
    if (!value) {
        return 0;
    }
    written = snprintf(out, out_size, "%s", value);
    if (written < 0 || (size_t)written >= out_size) {
        fail("attribute too long");
    }
    return 1;
}

static void fail(const char *message) {
    fprintf(stderr, "cdc-bridge-runtime: %s\n", message);
    exit(1);
}

static int dyadic_index(const char *dyadic) {
    int value = 0;
    if (strlen(dyadic) != 6) {
        return -1;
    }
    for (size_t i = 0; i < 6; i++) {
        if (dyadic[i] != '0' && dyadic[i] != '1') {
            return -1;
        }
        value = (value << 1) | (dyadic[i] - '0');
    }
    return value;
}

static int triadic_index(const char *triadic) {
    int value = 0;
    if (strlen(triadic) != 3) {
        return -1;
    }
    for (size_t i = 0; i < 3; i++) {
        if (triadic[i] < '0' || triadic[i] > '3') {
            return -1;
        }
        value = (value << 2) | (triadic[i] - '0');
    }
    return value;
}

static int digit_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    return -1;
}

static int dyadic_index_n(const char *dyadic, int arity) {
    int value = 0;
    if ((int)strlen(dyadic) != arity) {
        return -1;
    }
    for (int i = 0; i < arity; i++) {
        if (dyadic[i] != '0' && dyadic[i] != '1') {
            return -1;
        }
        value = (value << 1) | (dyadic[i] - '0');
    }
    return value;
}

static int triadic_index_n(const char *triadic, int base) {
    int value = 0;
    if (strlen(triadic) != 3) {
        return -1;
    }
    for (size_t i = 0; i < 3; i++) {
        int digit = digit_value(triadic[i]);
        if (digit < 0 || digit >= base) {
            return -1;
        }
        value = value * base + digit;
    }
    return value;
}

static void expected_triadic(int index, char out[4]) {
    out[0] = (char)('0' + ((index >> 4) & 3));
    out[1] = (char)('0' + ((index >> 2) & 3));
    out[2] = (char)('0' + (index & 3));
    out[3] = '\0';
}

static void expected_dyadic(int index, char out[7]) {
    for (int i = 5; i >= 0; i--) {
        out[5 - i] = ((index >> i) & 1) ? '1' : '0';
    }
    out[6] = '\0';
}

static void expected_dyadic_n(int index, int arity, char *out, size_t out_size) {
    if ((size_t)arity + 1 > out_size) {
        fail("dyadic output buffer too small");
    }
    for (int i = arity - 1; i >= 0; i--) {
        out[arity - 1 - i] = ((index >> i) & 1) ? '1' : '0';
    }
    out[arity] = '\0';
}

static void expected_triadic_n(int index, int arity, char out[4]) {
    int digit_bits = arity / 3;
    int base = 1 << digit_bits;
    int high = index / (base * base);
    int mid = (index / base) % base;
    int low = index % base;
    if (base > 16) {
        fail("base too large for compact codebook rows");
    }
    out[0] = DIGITS[high];
    out[1] = DIGITS[mid];
    out[2] = DIGITS[low];
    out[3] = '\0';
}

static void load_bridge64(const char *path, BridgeRow rows[BRIDGE64_ROWS]) {
    cdc_unit unit;
    size_t s;
    int dyadic_seen[BRIDGE64_ROWS] = {0};
    int triadic_seen[BRIDGE64_ROWS] = {0};
    int count = 0;

    load_unit(path, &unit);

    for (int i = 0; i < BRIDGE64_ROWS; i++) {
        rows[i].seen = 0;
        rows[i].index = -1;
        rows[i].witness[0] = '\0';
        rows[i].dyadic[0] = '\0';
        rows[i].triadic[0] = '\0';
    }

    for (s = 0; s < unit.count; s++) {
        const cdc_stmt *stmt = &unit.stmts[s];
        const char *id = witness_id_with_prefix(stmt, "bridge64-");
        char witness[64];
        char dyadic[16];
        char triadic[16];
        char index_text[16];
        int index;
        int d_index;
        int t_index;
        char expected_t[4];
        char expected_d[7];

        if (!id) {
            continue;
        }
        snprintf(witness, sizeof(witness), "%s", id);
        if (!stmt_attr_copy(stmt, "dyadic", dyadic, sizeof(dyadic)) ||
            !stmt_attr_copy(stmt, "triadic", triadic, sizeof(triadic)) ||
            !stmt_attr_copy(stmt, "index", index_text, sizeof(index_text))) {
            fail("bridge64 witness missing dyadic, triadic, or index attribute");
        }

        index = atoi(index_text);
        if (index < 0 || index >= BRIDGE64_ROWS) {
            fail("bridge64 index out of range");
        }
        if (rows[index].seen) {
            fail("duplicate bridge64 index");
        }

        d_index = dyadic_index(dyadic);
        t_index = triadic_index(triadic);
        if (d_index < 0 || t_index < 0) {
            fail("invalid bridge64 dyadic or triadic code");
        }
        if (d_index != index || t_index != index) {
            fail("bridge64 row does not match its numeric index");
        }
        if (dyadic_seen[d_index] || triadic_seen[t_index]) {
            fail("bridge64 duplicate dyadic or triadic code");
        }

        expected_triadic(index, expected_t);
        expected_dyadic(index, expected_d);
        cdc_expect_string(triadic, expected_t, "bridge64 row triadic code does not match canonical codebook");
        cdc_expect_string(dyadic, expected_d, "bridge64 row dyadic code does not match canonical codebook");

        snprintf(rows[index].witness, sizeof(rows[index].witness), "%s", witness);
        snprintf(rows[index].dyadic, sizeof(rows[index].dyadic), "%s", dyadic);
        snprintf(rows[index].triadic, sizeof(rows[index].triadic), "%s", triadic);
        rows[index].index = index;
        rows[index].seen = 1;
        dyadic_seen[d_index] = 1;
        triadic_seen[t_index] = 1;
        count++;
    }
    cdc_unit_free(&unit);

    cdc_expect_int(count, BRIDGE64_ROWS, "bridge64 file does not contain exactly 64 rows");
    for (int i = 0; i < BRIDGE64_ROWS; i++) {
        if (!rows[i].seen) {
            fail("bridge64 table has an unfilled row");
        }
    }
}

static void trits_to_dyadic(const char *trits, char out[7]) {
    if (strlen(trits) != 6) {
        fail("trit projection expects exactly six trits");
    }
    for (size_t i = 0; i < 6; i++) {
        char c = trits[i];
        if (c == '+' || c == '-') {
            out[i] = '1';
        } else if (c == '0' || c == 'o' || c == 'O') {
            out[i] = '0';
        } else {
            fail("trits must use '+', '-', '0', or 'o'");
        }
    }
    out[6] = '\0';
}

static unsigned long long pow2_int(int exp) {
    if (exp < 0 || exp > 62) {
        fail("arity too large for this runtime");
    }
    return 1ULL << exp;
}

static void cmd_verify(const char *path) {
    BridgeRow rows[BRIDGE64_ROWS];
    load_bridge64(path, rows);
    printf("bridge64 ok rows=64 bijection=dyadic<->triadic source=%s\n", path);
}

static void cmd_lookup_dyadic(const char *path, const char *dyadic) {
    BridgeRow rows[BRIDGE64_ROWS];
    int index = dyadic_index(dyadic);
    if (index < 0) {
        fail("lookup-dyadic expects six binary digits");
    }
    load_bridge64(path, rows);
    printf("dyadic=%s index=%d triadic=%s witness=%s\n",
           rows[index].dyadic, index, rows[index].triadic, rows[index].witness);
}

static void cmd_lookup_triadic(const char *path, const char *triadic) {
    BridgeRow rows[BRIDGE64_ROWS];
    int index = triadic_index(triadic);
    if (index < 0) {
        fail("lookup-triadic expects three base-4 digits");
    }
    load_bridge64(path, rows);
    printf("triadic=%s index=%d dyadic=%s witness=%s\n",
           rows[index].triadic, index, rows[index].dyadic, rows[index].witness);
}

static void cmd_project_trits(const char *path, const char *trits, const char *window) {
    BridgeRow rows[BRIDGE64_ROWS];
    char dyadic[7];
    int index;
    load_bridge64(path, rows);
    trits_to_dyadic(trits, dyadic);
    index = dyadic_index(dyadic);
    printf("trace-window=%s trits=%s occupancy=%s index=%d triadic=%s witness=%s\n",
           window, trits, dyadic, index, rows[index].triadic, rows[index].witness);
}

static void cmd_grid(const char *path) {
    BridgeRow rows[BRIDGE64_ROWS];
    load_bridge64(path, rows);
    printf("bridge64-grid source=%s\n", path);
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            int i = r * 8 + c;
            printf("%02d:%s/%s%s", i, rows[i].dyadic, rows[i].triadic, c == 7 ? "" : "  ");
        }
        printf("\n");
    }
}

static void cmd_grid_svg(const char *path) {
    BridgeRow rows[BRIDGE64_ROWS];
    load_bridge64(path, rows);
    printf("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"960\" height=\"620\" viewBox=\"0 0 960 620\">\n");
    printf("<style><![CDATA[\n");
    printf(".bridge-cell rect{transition:fill .12s ease,stroke-width .12s ease}.bridge-cell:hover rect,.bridge-cell:focus rect{fill:#dff7ff;stroke:#0369a1;stroke-width:2}.bridge-cell text{pointer-events:none}.bridge-cell{cursor:pointer;outline:none}\n");
    printf("]]></style>\n");
    printf("<script><![CDATA[\n");
    printf("function selectCell(g){var p=document.getElementById('bridge-selection');p.textContent='selected index='+g.dataset.index+' dyadic='+g.dataset.dyadic+' triadic='+g.dataset.triadic+' witness='+g.dataset.witness;}\n");
    printf("]]></script>\n");
    printf("<rect width=\"960\" height=\"620\" fill=\"#f8fafc\"/>\n");
    printf("<text x=\"24\" y=\"36\" font-family=\"ui-monospace, Menlo, monospace\" font-size=\"22\" font-weight=\"700\">bridge64 dyadic/triadic codebook</text>\n");
    printf("<text x=\"24\" y=\"62\" font-family=\"ui-monospace, Menlo, monospace\" font-size=\"13\" fill=\"#475569\">source=%s</text>\n", path);
    printf("<text id=\"bridge-selection\" x=\"24\" y=\"600\" font-family=\"ui-monospace, Menlo, monospace\" font-size=\"13\" fill=\"#0f172a\">hover or click a cell to inspect bridge coordinates</text>\n");
    for (int i = 0; i < BRIDGE64_ROWS; i++) {
        int row = i / 8;
        int col = i % 8;
        int x = 24 + col * 114;
        int y = 88 + row * 62;
        printf("<g class=\"bridge-cell\" tabindex=\"0\" data-index=\"%02d\" data-dyadic=\"%s\" data-triadic=\"%s\" data-witness=\"%s\" onclick=\"selectCell(this)\" onfocus=\"selectCell(this)\">\n",
               i, rows[i].dyadic, rows[i].triadic, rows[i].witness);
        printf("<title>index=%02d dyadic=%s triadic=%s witness=%s</title>\n", i, rows[i].dyadic, rows[i].triadic, rows[i].witness);
        printf("<rect x=\"%d\" y=\"%d\" width=\"102\" height=\"50\" rx=\"6\" fill=\"#ffffff\" stroke=\"#334155\"/>\n", x, y);
        printf("<text x=\"%d\" y=\"%d\" font-family=\"ui-monospace, Menlo, monospace\" font-size=\"11\" fill=\"#64748b\">%02d</text>\n", x + 8, y + 15, i);
        printf("<text x=\"%d\" y=\"%d\" font-family=\"ui-monospace, Menlo, monospace\" font-size=\"14\" font-weight=\"700\" fill=\"#0f172a\">%s</text>\n", x + 8, y + 32, rows[i].dyadic);
        printf("<text x=\"%d\" y=\"%d\" font-family=\"ui-monospace, Menlo, monospace\" font-size=\"14\" fill=\"#0369a1\">%s</text>\n", x + 62, y + 32, rows[i].triadic);
        printf("</g>\n");
    }
    printf("</svg>\n");
}

static void cmd_codebook(const char *arity_text) {
    int arity = atoi(arity_text);
    int digit_bits;
    unsigned long long base;
    unsigned long long states;
    if (arity <= 0 || arity % 3 != 0) {
        fail("codebook arity must be a positive multiple of 3");
    }
    digit_bits = arity / 3;
    base = pow2_int(digit_bits);
    states = pow2_int(arity);
    printf("codebook arity=%d dyadic=2^%d slots=3 base=%llu states=%llu equality=2^%d=%llu^3\n",
           arity, arity, base, states, arity, base);
    printf("row-format dyadic=<%d binary digits> triadic=<3 base-%llu digits>\n", arity, base);
}

static void parse_codebook_arity(const char *arity_text, int *arity, int *base, int *states) {
    int digit_bits;
    *arity = atoi(arity_text);
    if (*arity <= 0 || *arity % 3 != 0 || *arity > MAX_CODEBOOK_ARITY) {
        fail("codebook arity must be a positive multiple of 3 within runtime bounds");
    }
    digit_bits = *arity / 3;
    *base = 1 << digit_bits;
    *states = 1 << *arity;
    if (*base > 16) {
        fail("codebook base too large for compact row digits");
    }
}

static void cmd_emit_codebook(const char *arity_text) {
    int arity;
    int base;
    int states;
    char dyadic[32];
    char triadic[4];
    parse_codebook_arity(arity_text, &arity, &base, &states);
    printf("# bridge%d.cdc -- generated full higher-arity dyadic/triadic codebook.\n", states);
    printf("# Generated by runtime/cdc_bridge_runtime.c emit-codebook %d.\n\n", arity);
    for (int i = 0; i < states; i++) {
        expected_dyadic_n(i, arity, dyadic, sizeof(dyadic));
        expected_triadic_n(i, arity, triadic);
        printf("witness bridge%d-%s-%s invariant=dyadic-triadic-closure row=codebook arity=%d slots=3 base=%d states=%d index=%d dyadic=%s triadic=%s claim=\"generated full codebook row\"\n",
               states, dyadic, triadic, arity, base, states, i, dyadic, triadic);
    }
    printf("\nexpect witness bridge%d-%0*d-000\n", states, arity, 0);
    expected_dyadic_n(states - 1, arity, dyadic, sizeof(dyadic));
    expected_triadic_n(states - 1, arity, triadic);
    printf("expect witness bridge%d-%s-%s\n", states, dyadic, triadic);
}

static void cmd_verify_codebook(const char *path, const char *arity_text) {
    cdc_unit unit;
    size_t s;
    int arity;
    int base;
    int states;
    int count = 0;
    int *dyadic_seen;
    int *triadic_seen;

    load_unit(path, &unit);
    parse_codebook_arity(arity_text, &arity, &base, &states);
    dyadic_seen = calloc((size_t)states, sizeof(int));
    triadic_seen = calloc((size_t)states, sizeof(int));
    if (!dyadic_seen || !triadic_seen) {
        fail("could not allocate generated codebook census");
    }

    for (s = 0; s < unit.count; s++) {
        const cdc_stmt *stmt = &unit.stmts[s];
        char row[32];
        char dyadic[32];
        char triadic[32];
        char attr[32];
        char index_text[32];
        char expected_d[32];
        char expected_t[4];
        int index;
        int d_index;
        int t_index;

        if (!witness_id_with_prefix(stmt, "bridge")) {
            continue;
        }
        if (!stmt_attr_copy(stmt, "row", row, sizeof(row)) ||
            strcmp(row, "codebook") != 0) {
            continue;
        }
        if (!stmt_attr_copy(stmt, "arity", attr, sizeof(attr)) ||
            atoi(attr) != arity) {
            fail("generated codebook row has wrong arity");
        }
        if (!stmt_attr_copy(stmt, "dyadic", dyadic, sizeof(dyadic)) ||
            !stmt_attr_copy(stmt, "triadic", triadic, sizeof(triadic)) ||
            !stmt_attr_copy(stmt, "index", index_text, sizeof(index_text))) {
            fail("generated codebook row missing dyadic, triadic, or index");
        }
        index = atoi(index_text);
        if (index < 0 || index >= states) {
            fail("generated codebook index out of range");
        }
        d_index = dyadic_index_n(dyadic, arity);
        t_index = triadic_index_n(triadic, base);
        if (d_index < 0 || t_index < 0) {
            fail("generated codebook row has malformed coordinate");
        }
        if (d_index != index || t_index != index) {
            fail("generated codebook row does not match numeric index");
        }
        expected_dyadic_n(index, arity, expected_d, sizeof(expected_d));
        expected_triadic_n(index, arity, expected_t);
        cdc_expect_string(dyadic, expected_d, "generated codebook dyadic row does not match canonical coordinate");
        cdc_expect_string(triadic, expected_t, "generated codebook triadic row does not match canonical coordinate");
        if (dyadic_seen[d_index] || triadic_seen[t_index]) {
            fail("generated codebook duplicate dyadic or triadic coordinate");
        }
        dyadic_seen[d_index] = 1;
        triadic_seen[t_index] = 1;
        count++;
    }
    cdc_unit_free(&unit);
    cdc_expect_int(count, states, "generated codebook row count mismatch");
    for (int i = 0; i < states; i++) {
        if (!dyadic_seen[i] || !triadic_seen[i]) {
            fail("generated codebook has an unfilled row");
        }
    }
    free(dyadic_seen);
    free(triadic_seen);
    printf("generated-codebook ok arity=%d rows=%d base=%d source=%s\n", arity, states, base, path);
}

static void cmd_run_jobs(const char *bridge_path, const char *jobs_path) {
    BridgeRow rows[BRIDGE64_ROWS];
    cdc_unit unit;
    size_t s;
    int count = 0;

    load_unit(jobs_path, &unit);
    load_bridge64(bridge_path, rows);

    for (s = 0; s < unit.count; s++) {
        const cdc_stmt *stmt = &unit.stmts[s];
        const char *id = witness_id_with_prefix(stmt, "");
        char witness[64];
        char trits[32];
        char expected_dyadic_attr[16];
        char expected_triadic_attr[16];
        char window[64];
        char job[64];
        char actual_dyadic[7];
        int index;

        if (!id) {
            continue;
        }
        if (!stmt_attr_copy(stmt, "job", job, sizeof(job)) ||
            strcmp(job, "bridge-coordinate") != 0) {
            continue;
        }

        snprintf(witness, sizeof(witness), "%s", id);
        if (!stmt_attr_copy(stmt, "trits", trits, sizeof(trits)) ||
            !stmt_attr_copy(stmt, "expect-dyadic", expected_dyadic_attr,
                            sizeof(expected_dyadic_attr)) ||
            !stmt_attr_copy(stmt, "expect-triadic", expected_triadic_attr,
                            sizeof(expected_triadic_attr))) {
            fail("bridge-coordinate job missing trits or expected coordinate");
        }
        if (!stmt_attr_copy(stmt, "window", window, sizeof(window))) {
            snprintf(window, sizeof(window), "%s", "trace");
        }

        trits_to_dyadic(trits, actual_dyadic);
        index = dyadic_index(actual_dyadic);
        cdc_expect_string(actual_dyadic, expected_dyadic_attr, "bridge-coordinate job dyadic expectation mismatch");
        cdc_expect_string(rows[index].triadic, expected_triadic_attr, "bridge-coordinate job triadic expectation mismatch");

        printf("job=%s window=%s trits=%s dyadic=%s triadic=%s witness=%s\n",
               witness, window, trits, actual_dyadic, rows[index].triadic, rows[index].witness);
        count++;
    }

    cdc_unit_free(&unit);
    if (count == 0) {
        fail("no bridge-coordinate jobs found");
    }
    printf("bridge-coordinate jobs ok count=%d source=%s\n", count, jobs_path);
}

static void usage(void) {
    fprintf(stderr, "usage:\n");
    fprintf(stderr, "  cdc_bridge_runtime verify bridge64.cdc\n");
    fprintf(stderr, "  cdc_bridge_runtime lookup-dyadic bridge64.cdc 101011\n");
    fprintf(stderr, "  cdc_bridge_runtime lookup-triadic bridge64.cdc 223\n");
    fprintf(stderr, "  cdc_bridge_runtime project-trits bridge64.cdc +-0+0- [window]\n");
    fprintf(stderr, "  cdc_bridge_runtime grid bridge64.cdc\n");
    fprintf(stderr, "  cdc_bridge_runtime grid-svg bridge64.cdc\n");
    fprintf(stderr, "  cdc_bridge_runtime codebook 9\n");
    fprintf(stderr, "  cdc_bridge_runtime emit-codebook 9\n");
    fprintf(stderr, "  cdc_bridge_runtime verify-codebook bridge512.cdc 9\n");
    fprintf(stderr, "  cdc_bridge_runtime run-jobs bridge64.cdc bridge_jobs.cdc\n");
    exit(2);
}

/* CDC_BRIDGE_NO_MAIN (Amendment A11): mirrors CDC_NATIVE_NO_MAIN in the
 * native runtime so the unified cdc driver can link both runtimes into one
 * binary. With the guard defined, the same entry point compiles as
 * cdc_bridge_main for byte-identical passthrough (gate CT2); the
 * standalone bridge CLI keeps byte-identical behavior. */
#ifdef CDC_BRIDGE_NO_MAIN
#define CDC_BRIDGE_ENTRY cdc_bridge_main
int cdc_bridge_main(int argc, char **argv);
#else
#define CDC_BRIDGE_ENTRY main
#endif
int CDC_BRIDGE_ENTRY(int argc, char **argv) {
    if (argc < 2) {
        usage();
    }
    if (strcmp(argv[1], "verify") == 0 && argc == 3) {
        cmd_verify(argv[2]);
    } else if (strcmp(argv[1], "lookup-dyadic") == 0 && argc == 4) {
        cmd_lookup_dyadic(argv[2], argv[3]);
    } else if (strcmp(argv[1], "lookup-triadic") == 0 && argc == 4) {
        cmd_lookup_triadic(argv[2], argv[3]);
    } else if (strcmp(argv[1], "project-trits") == 0 && (argc == 4 || argc == 5)) {
        cmd_project_trits(argv[2], argv[3], argc == 5 ? argv[4] : "trace");
    } else if (strcmp(argv[1], "grid") == 0 && argc == 3) {
        cmd_grid(argv[2]);
    } else if (strcmp(argv[1], "grid-svg") == 0 && argc == 3) {
        cmd_grid_svg(argv[2]);
    } else if (strcmp(argv[1], "codebook") == 0 && argc == 3) {
        cmd_codebook(argv[2]);
    } else if (strcmp(argv[1], "emit-codebook") == 0 && argc == 3) {
        cmd_emit_codebook(argv[2]);
    } else if (strcmp(argv[1], "verify-codebook") == 0 && argc == 4) {
        cmd_verify_codebook(argv[2], argv[3]);
    } else if (strcmp(argv[1], "run-jobs") == 0 && argc == 4) {
        cmd_run_jobs(argv[2], argv[3]);
    } else {
        usage();
    }
    return 0;
}
