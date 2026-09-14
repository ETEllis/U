#define _POSIX_C_SOURCE 200809L

#include "cdc_ast.h"
#include "cdc_barrier.h"
#include "cdc_diagnostic.h"
#include "cdc_digest.h"
#include "cdc_parser.h"
#include "cdc_receipt.h"
#include "cdc_source.h"
#include "cdc_store.h"
#include "cdc_variational.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>

#define MAX_FIELDS 16
#define MAX_MODULES 64
#define MAX_CELLS 256
#define MAX_CHANNELS 256
#define MAX_STEPS 128
#define MAX_COMPILE_JOBS 32
#define MAX_PROOF_JOBS 32
#define MAX_COUNCILS 32
#define MAX_DELIBERATIONS 32
#define MAX_EVOLUTIONS 32
#define MAX_GUARDS 32
#define MAX_TRACES 32
#define MAX_MEASURES 32
#define MAX_POLICIES 32
#define MAX_BRIDGES 32
#define MAX_COUNTERS 32
#define MAX_UNIVERSALS 8
#define MAX_ORBITS 32
#define MAX_VARIATIONALS 32
#define MAX_SPECTRA 32
#define MAX_U2_COORDINATE_TEXT 512
#define MAX_U2_DIMENSION (MAX_CELLS + 2 * MAX_MODULES)
#define MAX_WITNESSES 64
#define MAX_STORES 8
#define MAX_PERSIST_JOBS 32
#define LINE_MAX_BYTES 1024
#define PI 3.14159265358979323846

typedef enum {
    STEP_FLOW,
    STEP_COMMIT,
    STEP_NEST
} StepKind;

typedef struct {
    char name[64];
    double dt;
    double gain;
    double deadband;
} Field;

typedef struct {
    char name[64];
    char field[64];
    char parent[64];
    double belief;
    double prior;
    double precision;
    double action_gain;
} Module;

typedef struct {
    char name[64];
    char module[64];
    double theta;
    double amplitude;
    double omega;
    char latch;
    int has_latch;
} Cell;

typedef struct {
    char source[64];
    char target[64];
    char id[64];
    char cone[16];
    char pair[64];
    double weight;
    double delay;
    double angle;
    char lines[64];
} Channel;

typedef struct {
    StepKind kind;
    char id[64];
    char field[64];
    char module[64];
    char parent[64];
    char child[64];
    double duration;
    double tolerance;
    char expect_theta_cell[128];
    double expect_theta;
    char expect_trits[64];
    char expect_balance[32];
    char expect_status[32];
    char expect_reason[32];
    int has_expect_theta;
    int has_expect_parent_belief;
    int has_expect_child_prior;
    double expect_parent_belief;
    double expect_child_prior;
} Step;

typedef struct {
    char id[64];
    char field[64];
    double duration;
    int has_theta;
    char theta_cell[64];
    double theta;
} FlowResult;

typedef struct {
    char id[64];
    char module[64];
    char trits[128];
    char balance[32];
    char status[32];
    char reason[32];
} CommitResult;

typedef struct {
    char id[64];
    char parent[64];
    char child[64];
    double up;
    double parent_belief;
    double child_prior;
} NestResult;

typedef struct {
    char theta_display[32];
    char theta_raw[32];
    char channel_weight[32];
    CommitResult accepted;
    CommitResult held;
    NestResult nest;
    char trace_trits[128];
    int trace_events;
    char bridge_dyadic[32];
    char bridge_triadic[32];
    int bridge_index;
} ReplayData;

typedef struct {
    char id[64];
    char source[128];
    int expect_ops;
    int expect_flow;
    int expect_commit;
    int expect_nest;
} CompileJob;

typedef struct {
    char id[64];
    char carrier[64];
    int arity;
    int expect_total;
    int expect_admissible;
    int expect_localized;
    int expect_saturated;
    int expect_catalan;
} ProofJob;

typedef struct {
    char id[64];
    char field[64];
    char members[256];
    int quorum;
    char expect_decision[32];
    char expect_dyadic[32];
    char expect_triadic[32];
} Council;

typedef struct {
    char id[64];
    char council[64];
} Deliberation;

typedef struct {
    char id[64];
    char source[128];
    char output[128];
    char coordinate[32];
    char append_witness[64];
    char expect_contains[64];
} EvolutionJob;

typedef struct {
    char id[64];
    char cell[64];
    char expect_state[32];
} Guard;

typedef struct {
    char id[64];
    char field[64];
    char expect_trits[128];
    int expect_events;
} TraceJob;

typedef struct {
    char id[64];
    char observer[64];
    char target[64];
    char mode[32];
    char expect_outcome[128];
    char expect_potential[32];
} MeasureJob;

typedef struct {
    char id[64];
    char window[64];
    char sampling[32];
    char commit[32];
    char adapt[32];
    char expect_sampling[32];
    char expect_commit[32];
    char expect_adapt[32];
} PolicyJob;

typedef struct {
    char id[64];
    char trace[64];
    char via[64];
    char expect_dyadic[32];
    char expect_triadic[32];
} SurfaceBridgeJob;

typedef struct {
    char id[64];
    int value;
    int increment;
    int decrement;
    int expect_value;
} CounterJob;

typedef struct {
    char id[64];
    char frame[64];
    char cover_cell[64];
    char cover[16];
    char half_step[64];
    char full_step[64];
    char receptive[64];
    char radiant[64];
    char record[64];
    char decision[64];
    char enact[64];
    double expect_holonomy;
    int has_expect_holonomy;
    char expect_half_projection[16];
    char expect_half_sheet[16];
    char expect_full_projection[16];
    char expect_full_sheet[16];
    char expect_coordinate[32];
    char expect_status[16];
    char expect_reason[40];
    double tolerance;
} UniversalJob;

/* U2 forms are derived analysis declarations.  They do not add a fourth
 * reduction: orbit qualifies recurrence of one Universal execution,
 * variational binds its tangent, and spectrum requests characteristic
 * multipliers only after recurrence authorizes a return tangent. */
typedef struct {
    char id[64];
    char universal[64];
    char coordinates[MAX_U2_COORDINATE_TEXT];
    char exclude[MAX_U2_COORDINATE_TEXT];
    char quotient[32];
    double absolute_tolerance;
    double relative_tolerance;
} OrbitJob;

typedef struct {
    char id[64];
    char orbit[64];
} VariationalJob;

typedef struct {
    char id[64];
    char variational[64];
    double neutral_tolerance;
    double schur_tolerance;
} SpectrumJob;

typedef struct {
    char frame[64];
    char receptive_angle[32];
    char radiant_angle[32];
    char holonomy[32];
    char half_projection[16];
    char half_sheet[16];
    char full_projection[16];
    char full_sheet[16];
    int winding;
    char record_coordinate[32];
    char decision_coordinate[32];
    char enacted_coordinate[32];
    char status[16];
    char reason[40];
} UniversalResult;

/* Persistence (gate CT4, capability H6). `store` declares a durable log the
 * way `field` declares a continuum; `persist` is a job over it the way
 * `commit` is a job over a module. Durable mutation is NOT a host service
 * call: an `op=append` runs the identical balanced-ternary barrier
 * `execute_commit` runs, and only an accepted decision is allowed to reach
 * the log. A violated prefix balance HOLDS — nothing is staged, nothing is
 * committed, and the sealed bytes are provably unchanged (`durable=no`).
 * That is the language-level statement of latch-or-hold: equilibrium is not
 * a failed write, it is the absence of one. */
typedef struct {
    char id[64];
    char dir[192];
    int fresh;
    int recovered;
    cdc_store *handle;
} StoreDecl;

typedef struct {
    char id[64];
    char store[64];
    char op[16];
    char module[64];
    int seal;
    int has_seal;
    char expect_trits[64];
    char expect_balance[32];
    char expect_status[32];
    char expect_reason[40];
    char expect_durable[8];
    char expect_replay[16];
    int expect_sealed;
    int has_expect_sealed;
    int expect_events;
    int has_expect_events;
} PersistJob;

/* A witness binding: the declared claim about one executed job. The
 * runtime does not evaluate witnesses — the contract checker owns that —
 * but it must be able to NAME the witness an effect discharges, so the
 * effect and the claim can be compared rather than assumed to match. */
typedef struct {
    char id[64];
    char job[64];
    char link[24];  /* the attribute that binds it: reducer, persistence, ... */
    char text[LINE_MAX_BYTES]; /* canonical statement, digested into the
                                * receipt; truncation would silently yield a
                                * WRONG digest, so it is refused instead */
} WitnessDecl;

typedef struct {
    /* Digest of the exact grammar-1 canonical statement stream used to
     * populate this runtime.  It is captured before the AST is released, so
     * receipts never re-read a path that may have changed after parsing. */
    char source_digest[72];
    Field fields[MAX_FIELDS];
    Module modules[MAX_MODULES];
    Cell cells[MAX_CELLS];
    Channel channels[MAX_CHANNELS];
    Step steps[MAX_STEPS];
    CompileJob compile_jobs[MAX_COMPILE_JOBS];
    ProofJob proof_jobs[MAX_PROOF_JOBS];
    Council councils[MAX_COUNCILS];
    Deliberation deliberations[MAX_DELIBERATIONS];
    EvolutionJob evolutions[MAX_EVOLUTIONS];
    Guard guards[MAX_GUARDS];
    TraceJob traces[MAX_TRACES];
    MeasureJob measures[MAX_MEASURES];
    PolicyJob policies[MAX_POLICIES];
    SurfaceBridgeJob bridges[MAX_BRIDGES];
    CounterJob counters[MAX_COUNTERS];
    UniversalJob universals[MAX_UNIVERSALS];
    OrbitJob orbits[MAX_ORBITS];
    VariationalJob variationals[MAX_VARIATIONALS];
    SpectrumJob spectra[MAX_SPECTRA];
    WitnessDecl witnesses[MAX_WITNESSES];
    StoreDecl stores[MAX_STORES];
    PersistJob persists[MAX_PERSIST_JOBS];
    int field_count;
    int module_count;
    int cell_count;
    int channel_count;
    int step_count;
    int compile_job_count;
    int proof_job_count;
    int council_count;
    int deliberation_count;
    int evolution_count;
    int guard_count;
    int trace_count;
    int measure_count;
    int policy_count;
    int bridge_count;
    int counter_count;
    int universal_count;
    int orbit_count;
    int variational_count;
    int spectrum_count;
    int store_count;
    int persist_count;
    int witness_count;
} Runtime;

static void fail(const char *message) {
    fprintf(stderr, "cdc-native-runtime: %s\n", message);
    exit(1);
}

static Field *find_field(Runtime *rt, const char *name) {
    for (int i = 0; i < rt->field_count; i++) {
        if (strcmp(rt->fields[i].name, name) == 0) {
            return &rt->fields[i];
        }
    }
    return NULL;
}

static Module *find_module(Runtime *rt, const char *name) {
    for (int i = 0; i < rt->module_count; i++) {
        if (strcmp(rt->modules[i].name, name) == 0) {
            return &rt->modules[i];
        }
    }
    return NULL;
}

static Council *find_council(Runtime *rt, const char *name) {
    for (int i = 0; i < rt->council_count; i++) {
        if (strcmp(rt->councils[i].id, name) == 0) {
            return &rt->councils[i];
        }
    }
    return NULL;
}

static TraceJob *find_trace(Runtime *rt, const char *name) {
    for (int i = 0; i < rt->trace_count; i++) {
        if (strcmp(rt->traces[i].id, name) == 0) {
            return &rt->traces[i];
        }
    }
    return NULL;
}

static Cell *find_cell(Runtime *rt, const char *name) {
    for (int i = 0; i < rt->cell_count; i++) {
        if (strcmp(rt->cells[i].name, name) == 0) {
            return &rt->cells[i];
        }
    }
    return NULL;
}

static int find_cell_index(Runtime *rt, const char *name) {
    for (int i = 0; i < rt->cell_count; i++) {
        if (strcmp(rt->cells[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int modules_share_field(Runtime *rt, const char *module_name, const char *field_name) {
    Module *module = find_module(rt, module_name);
    return module && strcmp(module->field, field_name) == 0;
}

static int cell_in_field(Runtime *rt, const Cell *cell, const char *field_name) {
    return modules_share_field(rt, cell->module, field_name);
}

static char trit_from_theta(double theta, double deadband) {
    double kappa = cos(theta);
    if (kappa > deadband) {
        return '+';
    }
    if (kappa < -deadband) {
        return '-';
    }
    return '0';
}

static int trit_value(char trit) {
    if (trit == '+') {
        return 1;
    }
    if (trit == '-') {
        return -1;
    }
    return 0;
}

static int is_commit_status(const char *status) {
    return strcmp(status, "accepted") == 0 ||
           strcmp(status, "held") == 0 ||
           strcmp(status, "degraded") == 0;
}

static int is_hold_reason(const char *reason) {
    return strcmp(reason, "none") == 0 ||
           strcmp(reason, "energy-increase") == 0 ||
           strcmp(reason, "balance-violation") == 0 ||
           strcmp(reason, "deadband-jitter") == 0 ||
           /* persistence holds (capability H6): a stale compare-and-set
            * view, and a compaction whose snapshot does not cover the
            * sealed prefix it would discard. Both hold rather than write. */
           strcmp(reason, "fence-violation") == 0 ||
           strcmp(reason, "compact-uncovered") == 0;
}

/* ---- grammar-1 source access (deletion gate step 1) ------------------
 *
 * The declaration readers below used to take a raw line and scan it. They
 * now take the parsed statement. These shims keep the LEGACY CONTRACTS
 * exactly — absent returns 0, overflow fails closed, a missing attribute
 * falls back — so every reader body is unchanged and the diff is only the
 * reading, not the logic.
 *
 * Values agree with the old scanner because no attribute these runtimes
 * consume is ever quoted; that precondition is gated in scripts/verify.sh
 * (D23), which is what makes this migration behaviour-preserving rather
 * than merely plausible. */
static void stmt_arg0_copy(const cdc_stmt *stmt, char *out, size_t out_size) {
    const char *arg = cdc_stmt_arg(stmt, 0);
    int written = snprintf(out, out_size, "%s", arg ? arg : "");
    if (written < 0 || (size_t)written >= out_size) {
        fail("declaration id too long");
    }
}

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

static void stmt_copy_attr(const cdc_stmt *stmt, const char *key, char *out,
                           size_t out_size, const char *fallback) {
    if (!stmt_attr_copy(stmt, key, out, out_size)) {
        snprintf(out, out_size, "%s", fallback);
    }
}

static int stmt_int_attr(const cdc_stmt *stmt, const char *key, int fallback) {
    char value[64];
    if (!stmt_attr_copy(stmt, key, value, sizeof(value))) {
        return fallback;
    }
    return atoi(value);
}

static double stmt_double_attr(const cdc_stmt *stmt, const char *key,
                               double fallback) {
    char value[64];
    if (!stmt_attr_copy(stmt, key, value, sizeof(value))) {
        return fallback;
    }
    return atof(value);
}

/* The canonical text of a statement, used as the witness digest surface.
 * Digesting the CANONICAL form rather than the raw line makes a closure
 * witness stable under incidental whitespace, which is what canonicalization
 * is for. Closure digest VALUES therefore change with this migration; no
 * gate pins a specific one, and the property they carry is unchanged. */
static void stmt_canonical_copy(const cdc_stmt *stmt, char *out,
                                size_t out_size) {
    char *buffer = NULL;
    size_t length = 0;
    FILE *mem = open_memstream(&buffer, &length);
    int written;
    if (!mem) {
        fail("could not render canonical statement");
    }
    cdc_stmt_canonical(stmt, mem);
    fclose(mem);
    written = snprintf(out, out_size, "%s", buffer ? buffer : "");
    free(buffer);
    if (written < 0 || (size_t)written >= out_size) {
        fail("witness statement too long to digest without truncation");
    }
}

static void unit_canonical_digest(const cdc_unit *unit, char *out,
                                  size_t out_size) {
    char *buffer = NULL;
    size_t length = 0;
    FILE *mem = open_memstream(&buffer, &length);
    uint8_t digest[CDC_DIGEST_SIZE];
    int write_failed;
    int close_failed;
    if (!mem) {
        fail("could not render canonical source identity");
    }
    cdc_unit_canonical(unit, mem);
    write_failed = fflush(mem) != 0 || ferror(mem);
    close_failed = fclose(mem) != 0;
    if (write_failed || close_failed) {
        free(buffer);
        fail("could not seal canonical source identity");
    }
    cdc_digest(buffer ? buffer : "", length, digest);
    cdc_digest_hex(digest, out, out_size);
    free(buffer);
    if (out[0] == '\0') {
        fail("canonical source identity was not rendered");
    }
}

static void add_field(Runtime *rt, const cdc_stmt *stmt) {
    Field *field;
    if (rt->field_count >= MAX_FIELDS) {
        fail("too many fields");
    }
    field = &rt->fields[rt->field_count++];
    stmt_arg0_copy(stmt, field->name, sizeof(field->name));
    field->dt = stmt_double_attr(stmt, "dt", 0.01);
    field->gain = stmt_double_attr(stmt, "gain", 1.0);
    field->deadband = stmt_double_attr(stmt, "deadband", 0.5);
}

static void add_module(Runtime *rt, const cdc_stmt *stmt) {
    Module *module;
    if (rt->module_count >= MAX_MODULES) {
        fail("too many modules");
    }
    module = &rt->modules[rt->module_count++];
    stmt_arg0_copy(stmt, module->name, sizeof(module->name));
    stmt_copy_attr(stmt, "field", module->field, sizeof(module->field), "");
    stmt_copy_attr(stmt, "parent", module->parent, sizeof(module->parent), "");
    module->belief = stmt_double_attr(stmt, "belief", 0.0);
    module->prior = stmt_double_attr(stmt, "prior", 0.0);
    module->precision = stmt_double_attr(stmt, "precision", 1.0);
    module->action_gain = stmt_double_attr(stmt, "action-gain", 1.0);
    if (!find_field(rt, module->field)) {
        fail("module references unknown field");
    }
}

static void add_cell(Runtime *rt, const cdc_stmt *stmt) {
    Cell *cell;
    if (rt->cell_count >= MAX_CELLS) {
        fail("too many cells");
    }
    cell = &rt->cells[rt->cell_count++];
    stmt_arg0_copy(stmt, cell->name, sizeof(cell->name));
    stmt_copy_attr(stmt, "module", cell->module, sizeof(cell->module), "");
    cell->theta = stmt_double_attr(stmt, "theta", 0.0);
    cell->amplitude = stmt_double_attr(stmt, "amplitude", 1.0);
    cell->omega = stmt_double_attr(stmt, "omega", 0.0);
    cell->latch = '0';
    cell->has_latch = 0;
    if (!find_module(rt, cell->module)) {
        fail("cell references unknown module");
    }
}

static void add_channel(Runtime *rt, const cdc_stmt *stmt) {
    Channel *channel;
    char arrow[16];
    if (rt->channel_count >= MAX_CHANNELS) {
        fail("too many channels");
    }
    channel = &rt->channels[rt->channel_count++];
    /* `channel <source> -> <target>`: three arguments, and "->" is an
     * argument because it carries no '='. sscanf over the raw line did the
     * same split; taking them from the statement removes the last raw-text
     * read in this file. */
    {
        const char *source = cdc_stmt_arg(stmt, 0);
        const char *sep = cdc_stmt_arg(stmt, 1);
        const char *target = cdc_stmt_arg(stmt, 2);
        if (!source || !sep || !target || strcmp(sep, "->") != 0) {
            fail("channel syntax must be: channel source -> target");
        }
        snprintf(arrow, sizeof(arrow), "%s", sep);
        if ((size_t)snprintf(channel->source, sizeof(channel->source), "%s",
                             source) >= sizeof(channel->source) ||
            (size_t)snprintf(channel->target, sizeof(channel->target), "%s",
                             target) >= sizeof(channel->target)) {
            fail("channel endpoint name too long");
        }
    }
    channel->weight = stmt_double_attr(stmt, "weight", 1.0);
    channel->delay = stmt_double_attr(stmt, "delay", 0.0);
    channel->angle = stmt_double_attr(stmt, "angle", 0.0);
    stmt_copy_attr(stmt, "id", channel->id, sizeof(channel->id), "");
    stmt_copy_attr(stmt, "cone", channel->cone, sizeof(channel->cone), "");
    stmt_copy_attr(stmt, "pair", channel->pair, sizeof(channel->pair), "");
    stmt_copy_attr(stmt, "lines", channel->lines, sizeof(channel->lines), "*");
    if (!find_cell(rt, channel->source) || !find_cell(rt, channel->target)) {
        fail("channel references unknown source or target cell");
    }
}

static void parse_expect_theta(Step *step, const cdc_stmt *stmt) {
    char value[128];
    char *colon;
    if (!stmt_attr_copy(stmt, "expect-theta", value, sizeof(value))) {
        return;
    }
    colon = strchr(value, ':');
    if (!colon) {
        fail("expect-theta must be cell:value");
    }
    *colon = '\0';
    snprintf(step->expect_theta_cell, sizeof(step->expect_theta_cell), "%s", value);
    step->expect_theta = atof(colon + 1);
    step->has_expect_theta = 1;
}

static void add_step(Runtime *rt, const cdc_stmt *stmt, StepKind kind) {
    Step *step;
    char value[64];
    if (rt->step_count >= MAX_STEPS) {
        fail("too many reducer steps");
    }
    step = &rt->steps[rt->step_count++];
    memset(step, 0, sizeof(*step));
    step->kind = kind;
    stmt_arg0_copy(stmt, step->id, sizeof(step->id));
    step->duration = stmt_double_attr(stmt, "duration", 0.0);
    step->tolerance = stmt_double_attr(stmt, "tolerance", 0.000001);
    stmt_copy_attr(stmt, "field", step->field, sizeof(step->field), "");
    stmt_copy_attr(stmt, "module", step->module, sizeof(step->module), "");
    stmt_copy_attr(stmt, "parent", step->parent, sizeof(step->parent), "");
    stmt_copy_attr(stmt, "child", step->child, sizeof(step->child), "");
    stmt_copy_attr(stmt, "expect-trits", step->expect_trits, sizeof(step->expect_trits), "");
    stmt_copy_attr(stmt, "expect-balance", step->expect_balance, sizeof(step->expect_balance), "");
    stmt_copy_attr(stmt, "expect-status", step->expect_status, sizeof(step->expect_status), "");
    stmt_copy_attr(stmt, "expect-reason", step->expect_reason, sizeof(step->expect_reason), "");
    parse_expect_theta(step, stmt);
    if (stmt_attr_copy(stmt, "expect-parent-belief", value, sizeof(value))) {
        step->expect_parent_belief = atof(value);
        step->has_expect_parent_belief = 1;
    }
    if (stmt_attr_copy(stmt, "expect-child-prior", value, sizeof(value))) {
        step->expect_child_prior = atof(value);
        step->has_expect_child_prior = 1;
    }
}

static void add_compile_job(Runtime *rt, const cdc_stmt *stmt) {
    CompileJob *job;
    if (rt->compile_job_count >= MAX_COMPILE_JOBS) {
        fail("too many compile jobs");
    }
    job = &rt->compile_jobs[rt->compile_job_count++];
    memset(job, 0, sizeof(*job));
    stmt_arg0_copy(stmt, job->id, sizeof(job->id));
    stmt_copy_attr(stmt, "source", job->source, sizeof(job->source), "");
    job->expect_ops = stmt_int_attr(stmt, "expect-ops", -1);
    job->expect_flow = stmt_int_attr(stmt, "expect-flow", -1);
    job->expect_commit = stmt_int_attr(stmt, "expect-commit", -1);
    job->expect_nest = stmt_int_attr(stmt, "expect-nest", -1);
}

static void add_proof_job(Runtime *rt, const cdc_stmt *stmt) {
    ProofJob *job;
    if (rt->proof_job_count >= MAX_PROOF_JOBS) {
        fail("too many proof jobs");
    }
    job = &rt->proof_jobs[rt->proof_job_count++];
    memset(job, 0, sizeof(*job));
    stmt_arg0_copy(stmt, job->id, sizeof(job->id));
    stmt_copy_attr(stmt, "carrier", job->carrier, sizeof(job->carrier), "");
    job->arity = stmt_int_attr(stmt, "arity", 0);
    job->expect_total = stmt_int_attr(stmt, "expect-total", -1);
    job->expect_admissible = stmt_int_attr(stmt, "expect-admissible", -1);
    job->expect_localized = stmt_int_attr(stmt, "expect-localized", -1);
    job->expect_saturated = stmt_int_attr(stmt, "expect-saturated", -1);
    job->expect_catalan = stmt_int_attr(stmt, "expect-catalan", -1);
}

static void add_council(Runtime *rt, const cdc_stmt *stmt) {
    Council *council;
    if (rt->council_count >= MAX_COUNCILS) {
        fail("too many councils");
    }
    council = &rt->councils[rt->council_count++];
    memset(council, 0, sizeof(*council));
    stmt_arg0_copy(stmt, council->id, sizeof(council->id));
    stmt_copy_attr(stmt, "field", council->field, sizeof(council->field), "");
    stmt_copy_attr(stmt, "members", council->members, sizeof(council->members), "");
    stmt_copy_attr(stmt, "expect-decision", council->expect_decision, sizeof(council->expect_decision), "");
    stmt_copy_attr(stmt, "expect-dyadic", council->expect_dyadic, sizeof(council->expect_dyadic), "");
    stmt_copy_attr(stmt, "expect-triadic", council->expect_triadic, sizeof(council->expect_triadic), "");
    council->quorum = stmt_int_attr(stmt, "quorum", 1);
}

static void add_deliberation(Runtime *rt, const cdc_stmt *stmt) {
    Deliberation *deliberation;
    if (rt->deliberation_count >= MAX_DELIBERATIONS) {
        fail("too many deliberations");
    }
    deliberation = &rt->deliberations[rt->deliberation_count++];
    memset(deliberation, 0, sizeof(*deliberation));
    stmt_arg0_copy(stmt, deliberation->id, sizeof(deliberation->id));
    stmt_copy_attr(stmt, "council", deliberation->council, sizeof(deliberation->council), "");
}

static void add_evolution(Runtime *rt, const cdc_stmt *stmt) {
    EvolutionJob *job;
    if (rt->evolution_count >= MAX_EVOLUTIONS) {
        fail("too many evolution jobs");
    }
    job = &rt->evolutions[rt->evolution_count++];
    memset(job, 0, sizeof(*job));
    stmt_arg0_copy(stmt, job->id, sizeof(job->id));
    stmt_copy_attr(stmt, "source", job->source, sizeof(job->source), "");
    stmt_copy_attr(stmt, "output", job->output, sizeof(job->output), "");
    stmt_copy_attr(stmt, "coordinate", job->coordinate, sizeof(job->coordinate), "");
    stmt_copy_attr(stmt, "append-witness", job->append_witness, sizeof(job->append_witness), "");
    stmt_copy_attr(stmt, "expect-contains", job->expect_contains, sizeof(job->expect_contains), "");
}

static void add_guard(Runtime *rt, const cdc_stmt *stmt) {
    Guard *guard;
    if (rt->guard_count >= MAX_GUARDS) {
        fail("too many guards");
    }
    guard = &rt->guards[rt->guard_count++];
    memset(guard, 0, sizeof(*guard));
    stmt_arg0_copy(stmt, guard->id, sizeof(guard->id));
    stmt_copy_attr(stmt, "cell", guard->cell, sizeof(guard->cell), "");
    stmt_copy_attr(stmt, "expect-state", guard->expect_state, sizeof(guard->expect_state), "");
}

static void add_trace(Runtime *rt, const cdc_stmt *stmt) {
    TraceJob *trace;
    if (rt->trace_count >= MAX_TRACES) {
        fail("too many traces");
    }
    trace = &rt->traces[rt->trace_count++];
    memset(trace, 0, sizeof(*trace));
    stmt_arg0_copy(stmt, trace->id, sizeof(trace->id));
    stmt_copy_attr(stmt, "field", trace->field, sizeof(trace->field), "");
    stmt_copy_attr(stmt, "expect-trits", trace->expect_trits, sizeof(trace->expect_trits), "");
    trace->expect_events = stmt_int_attr(stmt, "expect-events", -1);
}

static void add_measure(Runtime *rt, const cdc_stmt *stmt) {
    MeasureJob *measure;
    if (rt->measure_count >= MAX_MEASURES) {
        fail("too many measurements");
    }
    measure = &rt->measures[rt->measure_count++];
    memset(measure, 0, sizeof(*measure));
    stmt_arg0_copy(stmt, measure->id, sizeof(measure->id));
    stmt_copy_attr(stmt, "observer", measure->observer, sizeof(measure->observer), "");
    stmt_copy_attr(stmt, "target", measure->target, sizeof(measure->target), "");
    stmt_copy_attr(stmt, "mode", measure->mode, sizeof(measure->mode), "passive");
    stmt_copy_attr(stmt, "expect-outcome", measure->expect_outcome, sizeof(measure->expect_outcome), "");
    stmt_copy_attr(stmt, "expect-potential", measure->expect_potential, sizeof(measure->expect_potential), "");
}

static void add_policy(Runtime *rt, const cdc_stmt *stmt) {
    PolicyJob *policy;
    if (rt->policy_count >= MAX_POLICIES) {
        fail("too many policies");
    }
    policy = &rt->policies[rt->policy_count++];
    memset(policy, 0, sizeof(*policy));
    stmt_arg0_copy(stmt, policy->id, sizeof(policy->id));
    stmt_copy_attr(stmt, "window", policy->window, sizeof(policy->window), "");
    stmt_copy_attr(stmt, "sampling", policy->sampling, sizeof(policy->sampling), "");
    stmt_copy_attr(stmt, "commit", policy->commit, sizeof(policy->commit), "");
    stmt_copy_attr(stmt, "adapt", policy->adapt, sizeof(policy->adapt), "");
    stmt_copy_attr(stmt, "expect-sampling", policy->expect_sampling, sizeof(policy->expect_sampling), "");
    stmt_copy_attr(stmt, "expect-commit", policy->expect_commit, sizeof(policy->expect_commit), "");
    stmt_copy_attr(stmt, "expect-adapt", policy->expect_adapt, sizeof(policy->expect_adapt), "");
}

static void add_surface_bridge(Runtime *rt, const cdc_stmt *stmt) {
    SurfaceBridgeJob *bridge;
    if (rt->bridge_count >= MAX_BRIDGES) {
        fail("too many surface bridge jobs");
    }
    bridge = &rt->bridges[rt->bridge_count++];
    memset(bridge, 0, sizeof(*bridge));
    stmt_arg0_copy(stmt, bridge->id, sizeof(bridge->id));
    stmt_copy_attr(stmt, "trace", bridge->trace, sizeof(bridge->trace), "");
    stmt_copy_attr(stmt, "via", bridge->via, sizeof(bridge->via), "");
    stmt_copy_attr(stmt, "expect-dyadic", bridge->expect_dyadic, sizeof(bridge->expect_dyadic), "");
    stmt_copy_attr(stmt, "expect-triadic", bridge->expect_triadic, sizeof(bridge->expect_triadic), "");
}

static void add_universal(Runtime *rt, const cdc_stmt *stmt) {
    UniversalJob *job;
    char holonomy_text[64];
    if (rt->universal_count >= MAX_UNIVERSALS) {
        fail("too many universal jobs");
    }
    job = &rt->universals[rt->universal_count++];
    memset(job, 0, sizeof(*job));
    stmt_arg0_copy(stmt, job->id, sizeof(job->id));
    stmt_copy_attr(stmt, "frame", job->frame, sizeof(job->frame), "");
    stmt_copy_attr(stmt, "cover-cell", job->cover_cell, sizeof(job->cover_cell), "");
    stmt_copy_attr(stmt, "cover", job->cover, sizeof(job->cover), "double");
    stmt_copy_attr(stmt, "half-step", job->half_step, sizeof(job->half_step), "");
    stmt_copy_attr(stmt, "full-step", job->full_step, sizeof(job->full_step), "");
    stmt_copy_attr(stmt, "receptive", job->receptive, sizeof(job->receptive), "");
    stmt_copy_attr(stmt, "radiant", job->radiant, sizeof(job->radiant), "");
    stmt_copy_attr(stmt, "record", job->record, sizeof(job->record), "");
    stmt_copy_attr(stmt, "decision", job->decision, sizeof(job->decision), "");
    stmt_copy_attr(stmt, "enact", job->enact, sizeof(job->enact), "");
    job->has_expect_holonomy = stmt_attr_copy(stmt, "expect-holonomy", holonomy_text, sizeof(holonomy_text));
    job->expect_holonomy = stmt_double_attr(stmt, "expect-holonomy", 0.0);
    stmt_copy_attr(stmt, "expect-half-projection", job->expect_half_projection, sizeof(job->expect_half_projection), "");
    stmt_copy_attr(stmt, "expect-half-sheet", job->expect_half_sheet, sizeof(job->expect_half_sheet), "");
    stmt_copy_attr(stmt, "expect-full-projection", job->expect_full_projection, sizeof(job->expect_full_projection), "");
    stmt_copy_attr(stmt, "expect-full-sheet", job->expect_full_sheet, sizeof(job->expect_full_sheet), "");
    stmt_copy_attr(stmt, "expect-coordinate", job->expect_coordinate, sizeof(job->expect_coordinate), "");
    stmt_copy_attr(stmt, "expect-status", job->expect_status, sizeof(job->expect_status), "");
    stmt_copy_attr(stmt, "expect-reason", job->expect_reason, sizeof(job->expect_reason), "");
    job->tolerance = stmt_double_attr(stmt, "tolerance", 0.000001);
}

static void add_orbit(Runtime *rt, const cdc_stmt *stmt) {
    OrbitJob *job;
    double legacy_tolerance;
    if (rt->orbit_count >= MAX_ORBITS) {
        fail("too many orbit jobs");
    }
    job = &rt->orbits[rt->orbit_count++];
    memset(job, 0, sizeof(*job));
    stmt_arg0_copy(stmt, job->id, sizeof(job->id));
    stmt_copy_attr(stmt, "universal", job->universal,
                   sizeof(job->universal), "");
    stmt_copy_attr(stmt, "coordinates", job->coordinates,
                   sizeof(job->coordinates), "all");
    stmt_copy_attr(stmt, "exclude", job->exclude,
                   sizeof(job->exclude), "");
    stmt_copy_attr(stmt, "quotient", job->quotient,
                   sizeof(job->quotient), "none");
    legacy_tolerance = stmt_double_attr(stmt, "tolerance", 0.000001);
    job->absolute_tolerance = stmt_double_attr(
        stmt, "absolute-tolerance", legacy_tolerance);
    job->relative_tolerance = stmt_double_attr(
        stmt, "relative-tolerance", 0.0);
    if (job->id[0] == '\0' || job->universal[0] == '\0') {
        fail("orbit requires an id and universal=");
    }
    if (!isfinite(job->absolute_tolerance) ||
        job->absolute_tolerance <= 0.0 ||
        !isfinite(job->relative_tolerance) ||
        job->relative_tolerance < 0.0) {
        fail("orbit tolerances must be finite with absolute > 0 and relative >= 0");
    }
}

static void add_variational(Runtime *rt, const cdc_stmt *stmt) {
    VariationalJob *job;
    if (rt->variational_count >= MAX_VARIATIONALS) {
        fail("too many variational jobs");
    }
    job = &rt->variationals[rt->variational_count++];
    memset(job, 0, sizeof(*job));
    stmt_arg0_copy(stmt, job->id, sizeof(job->id));
    stmt_copy_attr(stmt, "orbit", job->orbit, sizeof(job->orbit), "");
    if (job->id[0] == '\0' || job->orbit[0] == '\0') {
        fail("variational requires an id and orbit=");
    }
}

static void add_spectrum(Runtime *rt, const cdc_stmt *stmt) {
    SpectrumJob *job;
    if (rt->spectrum_count >= MAX_SPECTRA) {
        fail("too many spectrum jobs");
    }
    job = &rt->spectra[rt->spectrum_count++];
    memset(job, 0, sizeof(*job));
    stmt_arg0_copy(stmt, job->id, sizeof(job->id));
    stmt_copy_attr(stmt, "variational", job->variational,
                   sizeof(job->variational), "");
    job->neutral_tolerance = stmt_double_attr(stmt, "neutral-tolerance", 1e-9);
    job->schur_tolerance = stmt_double_attr(stmt, "schur-tolerance", 1e-10);
    if (job->id[0] == '\0' || job->variational[0] == '\0') {
        fail("spectrum requires an id and variational=");
    }
    if (!isfinite(job->neutral_tolerance) || job->neutral_tolerance <= 0.0 ||
        !isfinite(job->schur_tolerance) || job->schur_tolerance <= 0.0) {
        fail("spectrum tolerances must be finite and positive");
    }
}

static void add_counter(Runtime *rt, const cdc_stmt *stmt) {
    CounterJob *counter;
    if (rt->counter_count >= MAX_COUNTERS) {
        fail("too many counters");
    }
    counter = &rt->counters[rt->counter_count++];
    memset(counter, 0, sizeof(*counter));
    stmt_arg0_copy(stmt, counter->id, sizeof(counter->id));
    counter->value = stmt_int_attr(stmt, "value", 0);
    counter->increment = stmt_int_attr(stmt, "increment", 0);
    counter->decrement = stmt_int_attr(stmt, "decrement", 0);
    counter->expect_value = stmt_int_attr(stmt, "expect-value", counter->value);
}

/* ---- lifecycle contract (gate CT3) -----------------------------------
 *
 * An executor that cannot be bounded or stopped is not embeddable: a host
 * needs to cap the work a source may do, and to stop a run without being
 * left holding a half-applied effect.
 *
 * Both are enforced at EFFECT BOUNDARIES — between whole effects, never
 * inside one. That placement is the whole guarantee: an effect either runs
 * completely or does not begin. For durable effects it is what makes a
 * stopped run safe, because the store's staged-but-uncommitted transaction
 * is exactly "nothing happened".
 *
 * A stop is a HOLD, not a failure. `budget-exhausted` and `cancelled` are
 * typed hold reasons in the same vocabulary as `balance-violation`: the
 * work did not happen, and the run says so in the carrier's own terms
 * rather than dying. Exit code 4 distinguishes a lifecycle stop from a
 * violated expectation (1).
 *
 * Configuration arrives by environment, not argv, so the legacy CLIs keep
 * byte-identical argument handling and the unified driver's passthrough
 * parity gate is untouched:
 *   CDC_MAX_EFFECTS   bound on executed effects (0 = unbounded)
 *   CDC_CANCEL_AFTER  test hook: request cancellation after N effects
 *   CDC_DETERMINISTIC declare the run must consult no nondeterministic
 *                     source it can avoid
 */
#define CDC_EXIT_LIFECYCLE_STOP 4

static volatile sig_atomic_t cancel_requested;
static long lifecycle_max_effects;
static long lifecycle_cancel_after;
static long lifecycle_effects_done;
static int lifecycle_deterministic;
static int lifecycle_ready;
static const char *lifecycle_stop_reason;

static void on_cancel_signal(int signum) {
    (void)signum;
    cancel_requested = 1; /* only an assignment: async-signal-safe */
}

static long env_long(const char *name, long fallback) {
    const char *value = getenv(name);
    char *end;
    long parsed;
    if (!value || value[0] == '\0') {
        return fallback;
    }
    parsed = strtol(value, &end, 10);
    if (*end != '\0' || parsed < 0) {
        fail("lifecycle: environment value must be a nonnegative integer");
    }
    return parsed;
}

static void lifecycle_init(void) {
    if (lifecycle_ready) {
        return;
    }
    lifecycle_ready = 1;
    lifecycle_max_effects = env_long("CDC_MAX_EFFECTS", 0);
    lifecycle_cancel_after = env_long("CDC_CANCEL_AFTER", 0);
    lifecycle_deterministic = getenv("CDC_DETERMINISTIC") != NULL &&
                              getenv("CDC_DETERMINISTIC")[0] != '\0';
    signal(SIGINT, on_cancel_signal);
    signal(SIGTERM, on_cancel_signal);
}

/* Called immediately BEFORE each effect. Returns a typed hold reason when
 * the run must stop, or NULL to proceed. The effect that would have run
 * has not started, so nothing it would have done is half-done. */
static const char *lifecycle_gate(void) {
    lifecycle_init();
    if (cancel_requested) {
        return "cancelled";
    }
    if (lifecycle_cancel_after > 0 &&
        lifecycle_effects_done >= lifecycle_cancel_after) {
        /* Test hook: the same boundary the real signal is observed at, so
         * the counterexample is deterministic. It proves the BOUNDARY
         * semantics; that the handler itself runs is proven separately by
         * delivering a real signal. */
        cancel_requested = 1;
        return "cancelled";
    }
    if (lifecycle_max_effects > 0 &&
        lifecycle_effects_done >= lifecycle_max_effects) {
        return "budget-exhausted";
    }
    return NULL;
}

static void lifecycle_count(void) { lifecycle_effects_done++; }

/* Reports the stop once, in the carrier's vocabulary, and exits with the
 * lifecycle code. Anything already committed stays committed — a stop
 * rewinds nothing, it only declines to continue. */
static void lifecycle_stop(const char *kind, const char *job,
                           const char *reason);

int cdc_native_deterministic(void) {
    lifecycle_init();
    return lifecycle_deterministic;
}

/* ---- typed effect receipts (gate CT3) -------------------------------- *
 *
 * Every effect below renders BOTH its report line and its receipt from the
 * same result struct. A consumer that wants the outcome reads the receipt;
 * nothing has to infer an effect by matching prose. scripts/verify.sh gates
 * that the two channels agree, so they cannot drift apart silently.
 *
 * The stream is opened once per process from CDC_RECEIPTS and is a no-op
 * when that variable is unset, so the interactive surface is unchanged. */
static void *receipt_stream;
static int receipt_stream_ready;

static void *receipts(void) {
    if (!receipt_stream_ready) {
        receipt_stream = cdc_receipt_open_env();
        receipt_stream_ready = 1;
    }
    return receipt_stream;
}

static void emit_receipt(const cdc_receipt *receipt) {
    void *stream = receipts();
    if (!stream) {
        return;
    }
    if (cdc_receipt_emit(stream, receipt) < 0) {
        /* A rejected receipt means the producer built an ill-formed record.
         * That is a defect in this file, never something a source can
         * cause, so it fails loudly rather than being dropped. */
        fail("effect receipt rejected: ill-formed record");
    }
}

static void lifecycle_stop(const char *kind, const char *job,
                           const char *reason) {
    cdc_receipt receipt;
    cdc_receipt_init(&receipt);
    snprintf(receipt.kind, sizeof(receipt.kind), "lifecycle");
    snprintf(receipt.job, sizeof(receipt.job), "%s", job && job[0] ? job : "-");
    snprintf(receipt.op, sizeof(receipt.op), "%s", kind);
    receipt.outcome = CDC_OUTCOME_HELD;
    snprintf(receipt.reason, sizeof(receipt.reason), "%s", reason);
    receipt.declared_hold = 0;
    emit_receipt(&receipt);
    lifecycle_stop_reason = reason;
    printf("lifecycle=stop kind=%s job=%s status=held reason=%s effects=%ld\n",
           kind, job && job[0] ? job : "-", reason, lifecycle_effects_done);
    fflush(stdout);
    exit(CDC_EXIT_LIFECYCLE_STOP);
}

/* Did the declaring statement carry expect-status=held? This is read from
 * the parsed job, not from the printed line (review B3: authorization is
 * bound to the typed statement identity). */
static int step_declares_hold(const Step *step) {
    return step->expect_status[0] &&
           strcmp(step->expect_status, "held") == 0;
}

/* The job-link attributes a witness may carry, in the bootloader's
 * declaration order. Exactly one must be present for the binding to be
 * executable; a witness with none is a claim about something else and is
 * simply not bound here. */
static const char *const WITNESS_LINKS[] = {
    "reducer", "guard",   "trace",     "measure", "policy",
    "bridge",  "counter", "compile",   "interpret", "council",
    "evolution", "universal", "store",  "persistence",
};

static void add_witness(Runtime *rt, const cdc_stmt *stmt) {
    WitnessDecl *witness;
    size_t i;
    int links = 0;
    if (rt->witness_count >= MAX_WITNESSES) {
        return; /* the contract checker owns completeness; this is a cache */
    }
    witness = &rt->witnesses[rt->witness_count];
    memset(witness, 0, sizeof(*witness));
    stmt_arg0_copy(stmt, witness->id, sizeof(witness->id));
    for (i = 0; i < sizeof(WITNESS_LINKS) / sizeof(WITNESS_LINKS[0]); i++) {
        char value[64];
        if (stmt_attr_copy(stmt, WITNESS_LINKS[i], value, sizeof(value))) {
            snprintf(witness->job, sizeof(witness->job), "%s", value);
            snprintf(witness->link, sizeof(witness->link), "%s",
                     WITNESS_LINKS[i]);
            links++;
        }
    }
    if (links != 1 || witness->id[0] == '\0' || witness->job[0] == '\0') {
        return; /* not an executable binding */
    }
    stmt_canonical_copy(stmt, witness->text, sizeof(witness->text));
    rt->witness_count++;
}

/* The witness bound to `job`, or NULL. */
static const WitnessDecl *find_witness(Runtime *rt, const char *job) {
    for (int i = 0; i < rt->witness_count; i++) {
        if (strcmp(rt->witnesses[i].job, job) == 0) {
            return &rt->witnesses[i];
        }
    }
    return NULL;
}

/* Attaches the closure witness for `job` to a receipt, if one is declared. */
static void attach_closure(Runtime *rt, cdc_receipt *receipt,
                           const char *job) {
    const WitnessDecl *witness = find_witness(rt, job);
    uint8_t digest[CDC_DIGEST_SIZE];
    if (!witness) {
        return;
    }
    snprintf(receipt->witness, sizeof(receipt->witness), "%s", witness->id);
    cdc_digest(witness->text, strlen(witness->text), digest);
    cdc_digest_hex(digest, receipt->closure, sizeof(receipt->closure));
}

static void add_store(Runtime *rt, const cdc_stmt *stmt) {
    StoreDecl *store;
    char mode[16];
    if (rt->store_count >= MAX_STORES) {
        fail("too many stores");
    }
    store = &rt->stores[rt->store_count++];
    memset(store, 0, sizeof(*store));
    stmt_arg0_copy(stmt, store->id, sizeof(store->id));
    stmt_copy_attr(stmt, "dir", store->dir, sizeof(store->dir), "");
    if (store->dir[0] == '\0') {
        fail("store requires dir=");
    }
    stmt_copy_attr(stmt, "mode", mode, sizeof(mode), "open");
    if (strcmp(mode, "fresh") == 0) {
        store->fresh = 1;
    } else if (strcmp(mode, "open") != 0) {
        fail("store mode must be fresh or open");
    }
}

static void add_persist(Runtime *rt, const cdc_stmt *stmt) {
    PersistJob *job;
    if (rt->persist_count >= MAX_PERSIST_JOBS) {
        fail("too many persistence jobs");
    }
    job = &rt->persists[rt->persist_count++];
    memset(job, 0, sizeof(*job));
    stmt_arg0_copy(stmt, job->id, sizeof(job->id));
    stmt_copy_attr(stmt, "store", job->store, sizeof(job->store), "");
    stmt_copy_attr(stmt, "op", job->op, sizeof(job->op), "");
    stmt_copy_attr(stmt, "module", job->module, sizeof(job->module), "");
    stmt_copy_attr(stmt, "expect-trits", job->expect_trits,
                  sizeof(job->expect_trits), "");
    stmt_copy_attr(stmt, "expect-balance", job->expect_balance,
                  sizeof(job->expect_balance), "");
    stmt_copy_attr(stmt, "expect-status", job->expect_status,
                  sizeof(job->expect_status), "");
    stmt_copy_attr(stmt, "expect-reason", job->expect_reason,
                  sizeof(job->expect_reason), "");
    stmt_copy_attr(stmt, "expect-durable", job->expect_durable,
                  sizeof(job->expect_durable), "");
    stmt_copy_attr(stmt, "expect-replay", job->expect_replay,
                  sizeof(job->expect_replay), "");
    job->seal = stmt_int_attr(stmt, "seal", -1);
    job->has_seal = job->seal >= 0;
    job->expect_sealed = stmt_int_attr(stmt, "expect-sealed", -1);
    job->has_expect_sealed = job->expect_sealed >= 0;
    job->expect_events = stmt_int_attr(stmt, "expect-events", -1);
    job->has_expect_events = job->expect_events >= 0;
}

static void parse_source(Runtime *rt, const char *path) {
    cdc_unit unit;
    cdc_diag_list diags;
    size_t s;

    cdc_diag_list_init(&diags);
    cdc_unit_init(&unit);
    if (!cdc_unit_parse_file(path, &unit, &diags) || diags.errors > 0) {
        /* The scanner used to skip anything it did not recognise and fail
         * later on a downstream symptom. The parser reports the cause. */
        const char *detail =
            diags.count > 0 ? diags.items[0].message : "parse failed";
        fprintf(stderr, "cdc-native-runtime: %s: %s\n", path, detail);
        cdc_diag_list_free(&diags);
        cdc_unit_free(&unit);
        exit(1);
    }
    cdc_diag_list_free(&diags);

    memset(rt, 0, sizeof(*rt));
    unit_canonical_digest(&unit, rt->source_digest,
                          sizeof(rt->source_digest));
    for (s = 0; s < unit.count; s++) {
        const cdc_stmt *stmt = &unit.stmts[s];
        const char *directive = cdc_stmt_directive(stmt);
        if (!directive || directive[0] == '\0') {
            continue;
        }
        if (strcmp(directive, "field") == 0) {
            add_field(rt, stmt);
        } else if (strcmp(directive, "module") == 0) {
            add_module(rt, stmt);
        } else if (strcmp(directive, "cell") == 0) {
            add_cell(rt, stmt);
        } else if (strcmp(directive, "channel") == 0) {
            add_channel(rt, stmt);
        } else if (strcmp(directive, "guard") == 0) {
            add_guard(rt, stmt);
        } else if (strcmp(directive, "flow") == 0) {
            add_step(rt, stmt, STEP_FLOW);
        } else if (strcmp(directive, "commit") == 0) {
            add_step(rt, stmt, STEP_COMMIT);
        } else if (strcmp(directive, "nest") == 0) {
            add_step(rt, stmt, STEP_NEST);
        } else if (strcmp(directive, "counter") == 0) {
            add_counter(rt, stmt);
        } else if (strcmp(directive, "trace") == 0) {
            add_trace(rt, stmt);
        } else if (strcmp(directive, "measure") == 0) {
            add_measure(rt, stmt);
        } else if (strcmp(directive, "policy") == 0) {
            add_policy(rt, stmt);
        } else if (strcmp(directive, "bridge") == 0) {
            add_surface_bridge(rt, stmt);
        } else if (strcmp(directive, "compile") == 0) {
            add_compile_job(rt, stmt);
        } else if (strcmp(directive, "proof") == 0) {
            add_proof_job(rt, stmt);
        } else if (strcmp(directive, "council") == 0) {
            add_council(rt, stmt);
        } else if (strcmp(directive, "deliberate") == 0) {
            add_deliberation(rt, stmt);
        } else if (strcmp(directive, "evolve") == 0) {
            add_evolution(rt, stmt);
        } else if (strcmp(directive, "universal") == 0) {
            add_universal(rt, stmt);
        } else if (strcmp(directive, "orbit") == 0) {
            add_orbit(rt, stmt);
        } else if (strcmp(directive, "variational") == 0) {
            add_variational(rt, stmt);
        } else if (strcmp(directive, "spectrum") == 0) {
            add_spectrum(rt, stmt);
        } else if (strcmp(directive, "store") == 0) {
            add_store(rt, stmt);
        } else if (strcmp(directive, "persist") == 0) {
            add_persist(rt, stmt);
        } else if (strcmp(directive, "witness") == 0) {
            add_witness(rt, stmt);
        }
    }
    cdc_unit_free(&unit);
}

static void execute_flow(Runtime *rt, Step *step, FlowResult *result) {
    Field *field = find_field(rt, step->field);
    double next_theta[MAX_CELLS];
    Cell *expected;
    memset(result, 0, sizeof(*result));
    if (!field) {
        fail("flow references unknown field");
    }
    snprintf(result->id, sizeof(result->id), "%s", step->id);
    snprintf(result->field, sizeof(result->field), "%s", field->name);
    result->duration = step->duration;
    for (int i = 0; i < rt->cell_count; i++) {
        next_theta[i] = rt->cells[i].theta;
        if (cell_in_field(rt, &rt->cells[i], field->name)) {
            next_theta[i] += rt->cells[i].omega * step->duration;
        }
    }
    for (int i = 0; i < rt->channel_count; i++) {
        Channel *channel = &rt->channels[i];
        int source_index = find_cell_index(rt, channel->source);
        int target_index = find_cell_index(rt, channel->target);
        Cell *source;
        Cell *target;
        double phase_delta;
        if (source_index < 0 || target_index < 0) {
            fail("flow channel references unknown cell");
        }
        source = &rt->cells[source_index];
        target = &rt->cells[target_index];
        if (!cell_in_field(rt, source, field->name) || !cell_in_field(rt, target, field->name)) {
            continue;
        }
        phase_delta = sin(source->theta + channel->angle - target->theta);
        next_theta[target_index] += field->gain * channel->weight * phase_delta * step->duration;
    }
    for (int i = 0; i < rt->cell_count; i++) {
        if (cell_in_field(rt, &rt->cells[i], field->name)) {
            rt->cells[i].theta = next_theta[i];
        }
    }
    if (step->has_expect_theta) {
        expected = find_cell(rt, step->expect_theta_cell);
        if (!expected) {
            fail("flow expectation references unknown cell");
        }
        cdc_expect_double(expected->theta, step->expect_theta, step->tolerance, "flow expectation mismatch");
        result->has_theta = 1;
        snprintf(result->theta_cell, sizeof(result->theta_cell), "%s", expected->name);
        result->theta = expected->theta;
    }
}

static void run_flow(Runtime *rt, Step *step) {
    FlowResult result;
    execute_flow(rt, step, &result);
    if (result.has_theta) {
        printf("flow=%s field=%s duration=%.6f theta %s=%.6f\n",
               result.id, result.field, result.duration, result.theta_cell, result.theta);
    } else {
        printf("flow=%s field=%s duration=%.6f\n", result.id, result.field, result.duration);
    }
}

static void execute_commit(Runtime *rt, Step *step, CommitResult *result) {
    Module *module = find_module(rt, step->module);
    Field *field;
    char trits[128];
    int cell_indexes[MAX_CELLS];
    char cell_trits[MAX_CELLS];
    int trit_count = 0;
    cdc_barrier_result barrier;
    int admissible;
    const char *status;
    const char *reason;
    memset(result, 0, sizeof(*result));
    if (!module) {
        fail("commit references unknown module");
    }
    field = find_field(rt, module->field);
    if (!field) {
        fail("commit module references unknown field");
    }
    for (int i = 0; i < rt->cell_count; i++) {
        Cell *cell = &rt->cells[i];
        char trit;
        if (strcmp(cell->module, module->name) != 0) {
            continue;
        }
        trit = trit_from_theta(cell->theta, field->deadband);
        if (trit_count + 1 >= (int)sizeof(trits)) {
            fail("commit trit vector too long");
        }
        cell_indexes[trit_count] = i;
        cell_trits[trit_count] = trit;
        trits[trit_count++] = trit;
    }
    trits[trit_count] = '\0';
    if (trit_count == 0) {
        fail("commit module has no cells");
    }
    if (!cdc_barrier_evaluate(trits, &barrier)) {
        fail("commit barrier rejected its internal carrier");
    }
    admissible = barrier.admissible;
    status = admissible ? "accepted" : "held";
    reason = admissible ? "none" : "balance-violation";
    if (admissible) {
        for (int i = 0; i < trit_count; i++) {
            Cell *cell = &rt->cells[cell_indexes[i]];
            cell->latch = cell_trits[i];
            cell->has_latch = 1;
        }
    }
    if (step->expect_trits[0]) {
        cdc_expect_string(trits, step->expect_trits, "commit trit expectation mismatch");
    }
    if (step->expect_balance[0]) {
        if (strcmp(step->expect_balance, "admissible") == 0) {
            if (!admissible) {
                fail("commit balance expectation mismatch");
            }
        } else if (strcmp(step->expect_balance, "violated") == 0) {
            if (admissible) {
                fail("commit balance expectation mismatch");
            }
        } else {
            fail("unknown commit balance expectation");
        }
    }
    if (step->expect_status[0]) {
        if (!is_commit_status(step->expect_status)) {
            fail("unknown commit status expectation");
        }
        cdc_expect_string(status, step->expect_status, "commit status expectation mismatch");
    }
    if (step->expect_reason[0]) {
        if (!is_hold_reason(step->expect_reason)) {
            fail("unknown commit reason expectation");
        }
        cdc_expect_string(reason, step->expect_reason, "commit reason expectation mismatch");
    }
    snprintf(result->id, sizeof(result->id), "%s", step->id);
    snprintf(result->module, sizeof(result->module), "%s", module->name);
    snprintf(result->trits, sizeof(result->trits), "%s", trits);
    snprintf(result->balance, sizeof(result->balance), "%s", admissible ? "admissible" : "violated");
    snprintf(result->status, sizeof(result->status), "%s", status);
    snprintf(result->reason, sizeof(result->reason), "%s", reason);
}

static void run_commit(Runtime *rt, Step *step) {
    CommitResult result;
    cdc_receipt receipt;
    execute_commit(rt, step, &result);
    cdc_receipt_init(&receipt);
    snprintf(receipt.kind, sizeof(receipt.kind), "commit");
    snprintf(receipt.job, sizeof(receipt.job), "%s", result.id);
    receipt.outcome = strcmp(result.status, "accepted") == 0
                          ? CDC_OUTCOME_ACCEPTED
                          : CDC_OUTCOME_HELD;
    snprintf(receipt.reason, sizeof(receipt.reason), "%s", result.reason);
    receipt.declared_hold = step_declares_hold(step);
    snprintf(receipt.trits, sizeof(receipt.trits), "%s", result.trits);
    snprintf(receipt.balance, sizeof(receipt.balance), "%s", result.balance);
    attach_closure(rt, &receipt, result.id);
    emit_receipt(&receipt);
    printf("commit=%s module=%s trits=%s balance=%s status=%s reason=%s\n",
           result.id, result.module, result.trits, result.balance, result.status, result.reason);
}

static double module_mean_trit(Runtime *rt, Module *module) {
    Field *field = find_field(rt, module->field);
    int count = 0;
    double total = 0.0;
    if (!field) {
        fail("module references unknown field");
    }
    for (int i = 0; i < rt->cell_count; i++) {
        Cell *cell = &rt->cells[i];
        char trit;
        if (strcmp(cell->module, module->name) != 0) {
            continue;
        }
        trit = cell->has_latch ? cell->latch : trit_from_theta(cell->theta, field->deadband);
        total += (double)trit_value(trit);
        count++;
    }
    if (count == 0) {
        fail("module has no cells");
    }
    return total / (double)count;
}

static void execute_nest(Runtime *rt, Step *step, NestResult *result) {
    Module *parent = find_module(rt, step->parent);
    Module *child = find_module(rt, step->child);
    Field *field;
    double up;
    memset(result, 0, sizeof(*result));
    if (!parent || !child) {
        fail("nest references unknown parent or child");
    }
    field = find_field(rt, parent->field);
    if (!field || strcmp(parent->field, child->field) != 0) {
        fail("nest modules must share a field");
    }
    up = module_mean_trit(rt, child);
    parent->belief += field->gain * up;
    child->prior = parent->belief;
    if (step->has_expect_parent_belief) {
        cdc_expect_double(
            parent->belief,
            step->expect_parent_belief,
            step->tolerance,
            "nest parent belief expectation mismatch");
    }
    if (step->has_expect_child_prior) {
        cdc_expect_double(
            child->prior,
            step->expect_child_prior,
            step->tolerance,
            "nest child prior expectation mismatch");
    }
    snprintf(result->id, sizeof(result->id), "%s", step->id);
    snprintf(result->parent, sizeof(result->parent), "%s", parent->name);
    snprintf(result->child, sizeof(result->child), "%s", child->name);
    result->up = up;
    result->parent_belief = parent->belief;
    result->child_prior = child->prior;
}

static void run_nest(Runtime *rt, Step *step) {
    NestResult result;
    cdc_receipt receipt;
    execute_nest(rt, step, &result);
    cdc_receipt_init(&receipt);
    snprintf(receipt.kind, sizeof(receipt.kind), "nest");
    snprintf(receipt.job, sizeof(receipt.job), "%s", result.id);
    /* A nest that completes is an accepted integration; it has no hold
     * path, and saying so explicitly keeps the ternary honest. */
    receipt.outcome = CDC_OUTCOME_ACCEPTED;
    snprintf(receipt.reason, sizeof(receipt.reason), "none");
    attach_closure(rt, &receipt, result.id);
    emit_receipt(&receipt);
    printf("nest=%s parent=%s child=%s up=%.6f parent-belief=%.6f child-prior=%.6f\n",
           result.id, result.parent, result.child, result.up, result.parent_belief, result.child_prior);
}

static void run_steps(Runtime *rt, const char *path) {
    int flow_count = 0;
    int commit_count = 0;
    int nest_count = 0;
    if (rt->step_count == 0) {
        fail("native reducer source has no steps");
    }
    for (int i = 0; i < rt->step_count; i++) {
        Step *step = &rt->steps[i];
        const char *stop = lifecycle_gate();
        if (stop) {
            lifecycle_stop("reducer", step->id, stop);
        }
        if (step->kind == STEP_FLOW) {
            run_flow(rt, step);
            flow_count++;
        } else if (step->kind == STEP_COMMIT) {
            run_commit(rt, step);
            commit_count++;
        } else if (step->kind == STEP_NEST) {
            run_nest(rt, step);
            nest_count++;
        }
        lifecycle_count();
    }
    if (flow_count == 0 || commit_count == 0 || nest_count == 0) {
        fail("native reducer source must exercise flow, commit, and nest");
    }
    printf("native reducer ok steps=%d flow=%d commit=%d nest=%d source=%s\n",
           rt->step_count, flow_count, commit_count, nest_count, path);
}

static void count_step_kinds(Runtime *rt, int *flow_count, int *commit_count, int *nest_count) {
    *flow_count = 0;
    *commit_count = 0;
    *nest_count = 0;
    for (int i = 0; i < rt->step_count; i++) {
        if (rt->steps[i].kind == STEP_FLOW) {
            (*flow_count)++;
        } else if (rt->steps[i].kind == STEP_COMMIT) {
            (*commit_count)++;
        } else if (rt->steps[i].kind == STEP_NEST) {
            (*nest_count)++;
        }
    }
}

static const char *step_kind_name(StepKind kind) {
    if (kind == STEP_FLOW) {
        return "flow";
    }
    if (kind == STEP_COMMIT) {
        return "commit";
    }
    return "nest";
}

static void compile_source(Runtime *rt, const char *path) {
    int flow_count;
    int commit_count;
    int nest_count;
    if (rt->compile_job_count == 0) {
        fail("native reducer source has no compile job");
    }
    count_step_kinds(rt, &flow_count, &commit_count, &nest_count);
    printf("cdc-ir source=%s fields=%d modules=%d cells=%d channels=%d ops=%d\n",
           path, rt->field_count, rt->module_count, rt->cell_count, rt->channel_count, rt->step_count);
    for (int i = 0; i < rt->step_count; i++) {
        Step *step = &rt->steps[i];
        if (step->kind == STEP_FLOW) {
            printf("op[%d]=%s id=%s field=%s duration=%.6f\n",
                   i, step_kind_name(step->kind), step->id, step->field, step->duration);
        } else if (step->kind == STEP_COMMIT) {
            printf("op[%d]=%s id=%s module=%s\n",
                   i, step_kind_name(step->kind), step->id, step->module);
        } else {
            printf("op[%d]=%s id=%s parent=%s child=%s\n",
                   i, step_kind_name(step->kind), step->id, step->parent, step->child);
        }
    }
    for (int i = 0; i < rt->compile_job_count; i++) {
        CompileJob *job = &rt->compile_jobs[i];
        if (job->expect_ops >= 0) {
            cdc_expect_int(rt->step_count, job->expect_ops, "compile job op count mismatch");
        }
        if (job->expect_flow >= 0) {
            cdc_expect_int(flow_count, job->expect_flow, "compile job flow count mismatch");
        }
        if (job->expect_commit >= 0) {
            cdc_expect_int(commit_count, job->expect_commit, "compile job commit count mismatch");
        }
        if (job->expect_nest >= 0) {
            cdc_expect_int(nest_count, job->expect_nest, "compile job nest count mismatch");
        }
        printf("compile-job=%s source=%s ops=%d flow=%d commit=%d nest=%d\n",
               job->id, job->source[0] ? job->source : path, rt->step_count, flow_count, commit_count, nest_count);
    }
    printf("native compile ok jobs=%d ops=%d source=%s\n", rt->compile_job_count, rt->step_count, path);
}

static int pow_int(int base, int exp) {
    int value = 1;
    for (int i = 0; i < exp; i++) {
        value *= base;
    }
    return value;
}

static void check_trit_walk_job(ProofJob *job) {
    int total;
    int admissible = 0;
    int localized = 0;
    int saturated = 0;
    int catalan = 0;
    if (strcmp(job->carrier, "balanced-ternary") != 0) {
        fail("unsupported proof carrier");
    }
    if (job->arity <= 0 || job->arity > 12) {
        fail("proof arity out of supported finite range");
    }
    total = pow_int(3, job->arity);
    for (int code = 0; code < total; code++) {
        int x = code;
        int balance = 0;
        int ok = 1;
        int has_zero = 0;
        for (int i = 0; i < job->arity; i++) {
            int digit = x % 3;
            int trit = digit - 1;
            x /= 3;
            if (trit == 0) {
                has_zero = 1;
            }
            balance += trit;
            if (balance < 0) {
                ok = 0;
                break;
            }
        }
        if (ok) {
            admissible++;
            if (!has_zero) {
                saturated++;
            }
            if (balance == 0) {
                localized++;
                if (!has_zero) {
                    catalan++;
                }
            }
        }
    }
    if (job->expect_total >= 0) {
        cdc_expect_int(total, job->expect_total, "proof total mismatch");
    }
    if (job->expect_admissible >= 0) {
        cdc_expect_int(admissible, job->expect_admissible, "proof admissible count mismatch");
    }
    if (job->expect_localized >= 0) {
        cdc_expect_int(localized, job->expect_localized, "proof localized count mismatch");
    }
    if (job->expect_saturated >= 0) {
        cdc_expect_int(saturated, job->expect_saturated, "proof saturated count mismatch");
    }
    if (job->expect_catalan >= 0) {
        cdc_expect_int(catalan, job->expect_catalan, "proof catalan count mismatch");
    }
    printf("proof=%s carrier=%s arity=%d total=%d admissible=%d localized=%d saturated=%d catalan=%d\n",
           job->id, job->carrier, job->arity, total, admissible, localized, saturated, catalan);
}

static void prove_source(Runtime *rt, const char *path) {
    if (rt->proof_job_count == 0) {
        fail("native reducer source has no proof job");
    }
    for (int i = 0; i < rt->proof_job_count; i++) {
        check_trit_walk_job(&rt->proof_jobs[i]);
    }
    printf("native proof ok jobs=%d source=%s\n", rt->proof_job_count, path);
}

static void interpret_source(Runtime *rt, const char *path) {
    int flow_count = 0;
    int commit_count = 0;
    int nest_count = 0;
    if (rt->step_count == 0) {
        fail("native reducer source has no IR operations");
    }
    printf("ir-interpreter source=%s ops=%d\n", path, rt->step_count);
    for (int i = 0; i < rt->step_count; i++) {
        Step *step = &rt->steps[i];
        printf("ir-exec op[%d]=%s id=%s\n", i, step_kind_name(step->kind), step->id);
        if (step->kind == STEP_FLOW) {
            run_flow(rt, step);
            flow_count++;
        } else if (step->kind == STEP_COMMIT) {
            run_commit(rt, step);
            commit_count++;
        } else if (step->kind == STEP_NEST) {
            run_nest(rt, step);
            nest_count++;
        }
    }
    if (flow_count == 0 || commit_count == 0 || nest_count == 0) {
        fail("IR interpreter must exercise flow, commit, and nest");
    }
    printf("native interpret ok ops=%d flow=%d commit=%d nest=%d source=%s\n",
           rt->step_count, flow_count, commit_count, nest_count, path);
}

static void trits_to_occupancy6(const char *trits, char out[7]) {
    if (strlen(trits) != 6) {
        fail("council bridge coordinate expects six trits");
    }
    for (int i = 0; i < 6; i++) {
        if (trits[i] == '+' || trits[i] == '-') {
            out[i] = '1';
        } else if (trits[i] == '0') {
            out[i] = '0';
        } else {
            fail("council trits must be balanced ternary");
        }
    }
    out[6] = '\0';
}

static void triadic_from_index64(int index, char out[4]) {
    out[0] = (char)('0' + ((index >> 4) & 3));
    out[1] = (char)('0' + ((index >> 2) & 3));
    out[2] = (char)('0' + (index & 3));
    out[3] = '\0';
}

static int dyadic6_to_index(const char *dyadic) {
    int value = 0;
    if (strlen(dyadic) != 6) {
        fail("dyadic bridge coordinate must have six bits");
    }
    for (int i = 0; i < 6; i++) {
        if (dyadic[i] != '0' && dyadic[i] != '1') {
            fail("dyadic bridge coordinate must be binary");
        }
        value = (value << 1) | (dyadic[i] - '0');
    }
    return value;
}

static void append_module_trits(Runtime *rt, Module *module, char *trits, size_t trits_size) {
    Field *field = find_field(rt, module->field);
    size_t used = strlen(trits);
    if (!field) {
        fail("council module references unknown field");
    }
    for (int i = 0; i < rt->cell_count; i++) {
        Cell *cell = &rt->cells[i];
        if (strcmp(cell->module, module->name) != 0) {
            continue;
        }
        if (used + 1 >= trits_size) {
            fail("council trit vector too long");
        }
        trits[used++] = trit_from_theta(cell->theta, field->deadband);
        trits[used] = '\0';
    }
}

static void append_field_trits(Runtime *rt, const char *field_name, char *trits, size_t trits_size) {
    size_t used = strlen(trits);
    Field *field = find_field(rt, field_name);
    if (!field) {
        fail("trace references unknown field");
    }
    for (int i = 0; i < rt->cell_count; i++) {
        Cell *cell = &rt->cells[i];
        if (!cell_in_field(rt, cell, field_name)) {
            continue;
        }
        if (used + 1 >= trits_size) {
            fail("trace trit vector too long");
        }
        trits[used++] = trit_from_theta(cell->theta, field->deadband);
        trits[used] = '\0';
    }
}

static int count_occupied_trits(const char *trits) {
    int occupied = 0;
    for (int i = 0; trits[i]; i++) {
        if (trits[i] == '+' || trits[i] == '-') {
            occupied++;
        } else if (trits[i] != '0') {
            fail("surface trits must be balanced ternary");
        }
    }
    return occupied;
}

static void run_guards(Runtime *rt) {
    for (int i = 0; i < rt->guard_count; i++) {
        Guard *guard = &rt->guards[i];
        Cell *cell = find_cell(rt, guard->cell);
        Module *module;
        Field *field;
        char trit;
        const char *state;
        if (!cell) {
            fail("guard references unknown cell");
        }
        module = find_module(rt, cell->module);
        if (!module) {
            fail("guard cell references unknown module");
        }
        field = find_field(rt, module->field);
        if (!field) {
            fail("guard module references unknown field");
        }
        trit = trit_from_theta(cell->theta, field->deadband);
        state = trit == '0' ? "open" : "closed";
        if (guard->expect_state[0]) {
            cdc_expect_string(state, guard->expect_state, "guard state expectation mismatch");
        }
        printf("guard=%s cell=%s trit=%c state=%s\n", guard->id, cell->name, trit, state);
    }
}

static void trace_trits(Runtime *rt, TraceJob *trace, char *trits, size_t trits_size) {
    trits[0] = '\0';
    append_field_trits(rt, trace->field, trits, trits_size);
}

static void run_traces(Runtime *rt) {
    for (int i = 0; i < rt->trace_count; i++) {
        TraceJob *trace = &rt->traces[i];
        char trits[128];
        int events;
        trace_trits(rt, trace, trits, sizeof(trits));
        events = count_occupied_trits(trits);
        if (trace->expect_trits[0]) {
            cdc_expect_string(trits, trace->expect_trits, "trace trit expectation mismatch");
        }
        if (trace->expect_events >= 0) {
            cdc_expect_int(events, trace->expect_events, "trace event-count expectation mismatch");
        }
        printf("trace=%s field=%s trits=%s events=%d\n", trace->id, trace->field, trits, events);
    }
}

static void run_measures(Runtime *rt) {
    for (int i = 0; i < rt->measure_count; i++) {
        MeasureJob *measure = &rt->measures[i];
        Module *observer = find_module(rt, measure->observer);
        Module *target = find_module(rt, measure->target);
        char outcome[128] = "";
        if (!observer || !target) {
            fail("measurement references unknown observer or target");
        }
        append_module_trits(rt, target, outcome, sizeof(outcome));
        if (measure->expect_outcome[0]) {
            cdc_expect_string(outcome, measure->expect_outcome, "measurement outcome expectation mismatch");
        }
        if (measure->expect_potential[0] && strcmp(measure->expect_potential, "nonincrease") != 0) {
            fail("unsupported measurement potential expectation");
        }
        printf("measure=%s observer=%s target=%s mode=%s outcome=%s potential=%s\n",
               measure->id, observer->name, target->name, measure->mode, outcome,
               measure->expect_potential[0] ? measure->expect_potential : "unchecked");
    }
}

static void run_policies(Runtime *rt) {
    for (int i = 0; i < rt->policy_count; i++) {
        PolicyJob *policy = &rt->policies[i];
        if (!find_trace(rt, policy->window)) {
            fail("policy references unknown trace/window");
        }
        if (policy->expect_sampling[0]) {
            cdc_expect_string(policy->sampling, policy->expect_sampling, "policy sampling expectation mismatch");
        }
        if (policy->expect_commit[0]) {
            cdc_expect_string(policy->commit, policy->expect_commit, "policy commit expectation mismatch");
        }
        if (policy->expect_adapt[0]) {
            cdc_expect_string(policy->adapt, policy->expect_adapt, "policy adapt expectation mismatch");
        }
        printf("policy=%s window=%s sampling=%s commit=%s adapt=%s\n",
               policy->id, policy->window, policy->sampling, policy->commit, policy->adapt);
    }
}

static void execute_surface_bridge(Runtime *rt, SurfaceBridgeJob *bridge,
                                   char trits[128], char dyadic[7], char triadic[4], int *index) {
    TraceJob *trace = find_trace(rt, bridge->trace);
    if (!trace) {
        fail("surface bridge references unknown trace");
    }
    if (bridge->via[0] && strcmp(bridge->via, "bridge64") != 0) {
        fail("surface bridge currently supports bridge64");
    }
    trace_trits(rt, trace, trits, 128);
    trits_to_occupancy6(trits, dyadic);
    *index = dyadic6_to_index(dyadic);
    triadic_from_index64(*index, triadic);
    if (bridge->expect_dyadic[0]) {
        cdc_expect_string(dyadic, bridge->expect_dyadic, "surface bridge dyadic expectation mismatch");
    }
    if (bridge->expect_triadic[0]) {
        cdc_expect_string(triadic, bridge->expect_triadic, "surface bridge triadic expectation mismatch");
    }
}

static void run_surface_bridges(Runtime *rt) {
    for (int i = 0; i < rt->bridge_count; i++) {
        SurfaceBridgeJob *bridge = &rt->bridges[i];
        char trits[128];
        char dyadic[7];
        char triadic[4];
        int index;
        execute_surface_bridge(rt, bridge, trits, dyadic, triadic, &index);
        printf("bridge=%s trace=%s via=%s trits=%s dyadic=%s triadic=%s\n",
               bridge->id, bridge->trace, bridge->via, trits, dyadic, triadic);
    }
}

static void run_counters(Runtime *rt) {
    for (int i = 0; i < rt->counter_count; i++) {
        CounterJob *counter = &rt->counters[i];
        int value = counter->value + counter->increment - counter->decrement;
        cdc_expect_int(value, counter->expect_value, "counter expectation mismatch");
        printf("counter=%s value=%d increment=%d decrement=%d final=%d\n",
               counter->id, counter->value, counter->increment, counter->decrement, value);
    }
}

static void run_surface(Runtime *rt, const char *path) {
    if (rt->guard_count == 0 || rt->trace_count == 0 || rt->measure_count == 0 ||
        rt->policy_count == 0 || rt->bridge_count == 0 || rt->counter_count == 0) {
        fail("surface source must exercise guard, trace, measure, policy, bridge, and counter");
    }
    run_guards(rt);
    run_traces(rt);
    run_measures(rt);
    run_policies(rt);
    run_surface_bridges(rt);
    run_counters(rt);
    printf("native surface ok guards=%d traces=%d measures=%d policies=%d bridges=%d counters=%d source=%s\n",
           rt->guard_count, rt->trace_count, rt->measure_count,
           rt->policy_count, rt->bridge_count, rt->counter_count, path);
}

static void execute_deliberation(Runtime *rt, Deliberation *deliberation,
                                 char trits[128], char dyadic[7], char triadic[4],
                                 int *occupancy, int *quorum, char decision[32]) {
    Council *council = find_council(rt, deliberation->council);
    char members[256];
    int index;
    char *cursor;
    if (!council) {
        fail("deliberation references unknown council");
    }
    trits[0] = '\0';
    *occupancy = 0;
    snprintf(members, sizeof(members), "%s", council->members);
    cursor = strtok(members, ",");
    while (cursor) {
        Module *member = find_module(rt, cursor);
        if (!member) {
            fail("council member references unknown module");
        }
        append_module_trits(rt, member, trits, 128);
        cursor = strtok(NULL, ",");
    }
    trits_to_occupancy6(trits, dyadic);
    for (int j = 0; j < 6; j++) {
        if (dyadic[j] == '1') {
            (*occupancy)++;
        }
    }
    index = dyadic6_to_index(dyadic);
    triadic_from_index64(index, triadic);
    *quorum = council->quorum;
    snprintf(decision, 32, "%s", *occupancy >= council->quorum ? "adopt" : "hold");
    if (council->expect_decision[0]) {
        cdc_expect_string(decision, council->expect_decision, "council decision expectation mismatch");
    }
    if (council->expect_dyadic[0]) {
        cdc_expect_string(dyadic, council->expect_dyadic, "council dyadic expectation mismatch");
    }
    if (council->expect_triadic[0]) {
        cdc_expect_string(triadic, council->expect_triadic, "council triadic expectation mismatch");
    }
}

static void run_council(Runtime *rt, const char *path) {
    if (rt->deliberation_count == 0) {
        fail("source has no council deliberation");
    }
    for (int i = 0; i < rt->deliberation_count; i++) {
        Deliberation *deliberation = &rt->deliberations[i];
        char trits[128];
        char dyadic[7];
        char triadic[4];
        char decision[32];
        int occupancy;
        int quorum;
        execute_deliberation(rt, deliberation, trits, dyadic, triadic, &occupancy, &quorum, decision);
        printf("council=%s deliberation=%s trits=%s dyadic=%s triadic=%s occupancy=%d quorum=%d decision=%s\n",
               deliberation->council, deliberation->id, trits, dyadic, triadic, occupancy, quorum, decision);
    }
    printf("native council ok deliberations=%d source=%s\n", rt->deliberation_count, path);
}

static int file_contains(const char *path, const char *needle) {
    FILE *fp = fopen(path, "r");
    char line[LINE_MAX_BYTES];
    if (!fp) {
        return 0;
    }
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, needle)) {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

static void run_evolution(Runtime *rt, const char *path) {
    if (rt->evolution_count == 0) {
        fail("source has no evolution job");
    }
    for (int i = 0; i < rt->evolution_count; i++) {
        EvolutionJob *job = &rt->evolutions[i];
        FILE *in = fopen(job->source, "r");
        FILE *out;
        char line[LINE_MAX_BYTES];
        if (!in) {
            fail("evolution source could not be opened");
        }
        out = fopen(job->output, "w");
        if (!out) {
            fclose(in);
            fail("evolution output could not be opened");
        }
        while (fgets(line, sizeof(line), in)) {
            fputs(line, out);
        }
        fprintf(out, "\n# evolved by bridge coordinate %s through %s\n", job->coordinate, job->id);
        fprintf(out, "witness %s invariant=dyadic-triadic-closure coordinate=%s claim=\"bridge coordinate self evolution wrote this witness\"\n",
                job->append_witness, job->coordinate);
        fclose(in);
        fclose(out);
        if (job->expect_contains[0] && !file_contains(job->output, job->expect_contains)) {
            fail("evolution output missing expected text");
        }
        printf("evolution=%s coordinate=%s source=%s output=%s appended=%s\n",
               job->id, job->coordinate, job->source, job->output, job->append_witness);
    }
    printf("native evolution ok jobs=%d source=%s\n", rt->evolution_count, path);
}

/* ---- persistence (capability H6) ------------------------------------- */

static StoreDecl *find_store_decl(Runtime *rt, const char *id) {
    for (int i = 0; i < rt->store_count; i++) {
        if (strcmp(rt->stores[i].id, id) == 0) {
            return &rt->stores[i];
        }
    }
    return NULL;
}

/* Creates each component of a declared store directory. Only directories
 * are created; nothing is ever removed here. */
static void ensure_dir_path(const char *path) {
    char work[192];
    size_t i;
    int written = snprintf(work, sizeof(work), "%s", path);
    if (written < 0 || (size_t)written >= sizeof(work)) {
        fail("store dir path too long");
    }
    for (i = 1; work[i]; i++) {
        if (work[i] != '/') {
            continue;
        }
        work[i] = '\0';
        if (mkdir(work, 0777) != 0 && errno != EEXIST) {
            fail("store dir could not be created");
        }
        work[i] = '/';
    }
    if (mkdir(work, 0777) != 0 && errno != EEXIST) {
        fail("store dir could not be created");
    }
}

static void store_or_fail(cdc_store_status status, const char *what) {
    if (status != CDC_STORE_OK) {
        fprintf(stderr, "cdc-native-runtime: %s (%s)\n", what,
                cdc_store_status_name(status));
        exit(1);
    }
}

static void persist_state(StoreDecl *decl, char *replay_out,
                          size_t replay_size, char *attest_out,
                          size_t attest_size) {
    store_or_fail(cdc_store_replay(decl->handle, replay_out, replay_size),
                  "persistence replay failed");
    store_or_fail(cdc_store_attest(decl->handle, attest_out, attest_size),
                  "persistence attest failed");
}

/* Executes one persistence job and returns its ternary outcome. The only
 * durable-mutating ops are append (gated by the commit barrier), snapshot,
 * and compact; every other op is a read of the sealed prefix. */
static void execute_persist(Runtime *rt, PersistJob *job, StoreDecl *decl,
                            const char **status_out,
                            const char **reason_out,
                            CommitResult *decision, int *is_append) {
    const char *status = "accepted";
    const char *reason = "none";
    cdc_store_status st;

    *is_append = 0;
    if (strcmp(job->op, "append") == 0) {
        Step gate;
        char payload[256];
        int size;
        *is_append = 1;
        if (job->module[0] == '\0') {
            fail("persist op=append requires module=");
        }
        /* The gate is the calculus, not a policy check: the identical
         * balanced-ternary commit that governs in-memory latching decides
         * whether these trits may become durable. Only expectations that
         * are facts ABOUT the barrier are delegated to it; the job's own
         * status/reason are asserted against the persistence outcome,
         * which can hold for reasons the barrier never produces. */
        memset(&gate, 0, sizeof(gate));
        gate.kind = STEP_COMMIT;
        snprintf(gate.id, sizeof(gate.id), "%s", job->id);
        snprintf(gate.module, sizeof(gate.module), "%s", job->module);
        snprintf(gate.expect_trits, sizeof(gate.expect_trits), "%s",
                 job->expect_trits);
        snprintf(gate.expect_balance, sizeof(gate.expect_balance), "%s",
                 job->expect_balance);
        execute_commit(rt, &gate, decision);
        if (strcmp(decision->status, "accepted") != 0) {
            /* Held: nothing is staged, so nothing can be written. The
             * absence of a durable effect is the equilibrium outcome. */
            *status_out = "held";
            *reason_out = decision->reason;
            return;
        }
        size = snprintf(payload, sizeof(payload), "%s %s", decision->module,
                        decision->trits);
        if (size < 0 || (size_t)size >= sizeof(payload)) {
            fail("persist payload too long");
        }
        store_or_fail(cdc_store_stage(decl->handle, payload, (size_t)size),
                      "persistence stage failed");
        st = cdc_store_commit(decl->handle);
        if (st == CDC_STORE_ESTATE) {
            /* An armed fence saw the log move underneath this writer. */
            store_or_fail(cdc_store_rollback(decl->handle),
                          "persistence rollback failed");
            status = "held";
            reason = "fence-violation";
        } else {
            store_or_fail(st, "persistence commit failed");
        }
    } else if (strcmp(job->op, "replay") == 0 ||
               strcmp(job->op, "attest") == 0) {
        /* Both digests are recomputed around every job; naming the op
         * makes which identity the job is about explicit in the record. */
    } else if (strcmp(job->op, "verify") == 0) {
        store_or_fail(cdc_store_verify(decl->handle),
                      "persistence verify failed");
    } else if (strcmp(job->op, "snapshot") == 0) {
        store_or_fail(cdc_store_snapshot(decl->handle),
                      "persistence snapshot failed");
    } else if (strcmp(job->op, "compact") == 0) {
        st = cdc_store_compact(decl->handle);
        if (st == CDC_STORE_ESTATE) {
            status = "held";
            reason = "compact-uncovered";
        } else {
            store_or_fail(st, "persistence compact failed");
        }
    } else if (strcmp(job->op, "fence") == 0) {
        uint64_t want = job->has_seal
                            ? (uint64_t)job->seal
                            : cdc_store_sealed_count(decl->handle);
        st = cdc_store_fence(decl->handle, want);
        if (st == CDC_STORE_ESTATE) {
            status = "held";
            reason = "fence-violation";
        } else {
            store_or_fail(st, "persistence fence failed");
        }
    } else {
        fail("unknown persist op");
    }
    *status_out = status;
    *reason_out = reason;
}

static void run_persistence(Runtime *rt, const char *path) {
    int accepted = 0;
    int held = 0;

    if (rt->store_count == 0) {
        fail("source declares no store");
    }
    if (rt->persist_count == 0) {
        fail("source has no persistence job");
    }
    for (int i = 0; i < rt->store_count; i++) {
        StoreDecl *decl = &rt->stores[i];
        ensure_dir_path(decl->dir);
        if (decl->fresh) {
            store_or_fail(cdc_store_reset(decl->dir),
                          "store reset failed");
        }
        store_or_fail(cdc_store_open(decl->dir, &decl->handle,
                                     &decl->recovered),
                      "store could not be opened");
        printf("store=%s dir=%s mode=%s recovered=%d sealed=%llu\n",
               decl->id, decl->dir, decl->fresh ? "fresh" : "open",
               decl->recovered,
               (unsigned long long)cdc_store_sealed_count(decl->handle));
    }
    for (int i = 0; i < rt->persist_count; i++) {
        PersistJob *job = &rt->persists[i];
        StoreDecl *decl = find_store_decl(rt, job->store);
        CommitResult decision;
        char before_replay[96], after_replay[96];
        char before_attest[96], after_attest[96];
        const char *status = "accepted";
        const char *reason = "none";
        const char *durable;
        const char *replay_id;
        int is_append = 0;
        unsigned long long sealed, events;

        if (!decl) {
            fail("persist references unknown store");
        }
        {
            /* Checked BEFORE the barrier runs and before anything is
             * staged, so a stopped run cannot leave a durable effect
             * half-applied: the store sees no transaction at all. */
            const char *stop = lifecycle_gate();
            if (stop) {
                for (int c = 0; c < rt->store_count; c++) {
                    cdc_store_close(rt->stores[c].handle);
                    rt->stores[c].handle = NULL;
                }
                lifecycle_stop("persist", job->id, stop);
            }
        }
        memset(&decision, 0, sizeof(decision));
        persist_state(decl, before_replay, sizeof(before_replay),
                      before_attest, sizeof(before_attest));
        execute_persist(rt, job, decl, &status, &reason, &decision,
                        &is_append);
        persist_state(decl, after_replay, sizeof(after_replay),
                      after_attest, sizeof(after_attest));

        /* Durability is OBSERVED, never asserted: the sealed log bytes
         * either changed or they did not. Replay identity is the semantic
         * state; the two diverge exactly once, under compaction. */
        durable = strcmp(before_attest, after_attest) == 0 ? "no" : "yes";
        replay_id =
            strcmp(before_replay, after_replay) == 0 ? "stable" : "changed";
        sealed = (unsigned long long)cdc_store_sealed_count(decl->handle);
        events = (unsigned long long)cdc_store_event_count(decl->handle);

        if (job->expect_status[0]) {
            if (!is_commit_status(job->expect_status)) {
                fail("unknown persist status expectation");
            }
            cdc_expect_string(status, job->expect_status,
                              "persist status expectation mismatch");
        }
        if (job->expect_reason[0]) {
            if (!is_hold_reason(job->expect_reason)) {
                fail("unknown persist reason expectation");
            }
            cdc_expect_string(reason, job->expect_reason,
                              "persist reason expectation mismatch");
        }
        if (job->expect_durable[0]) {
            cdc_expect_string(durable, job->expect_durable,
                              "persist durability expectation mismatch");
        }
        if (job->expect_replay[0]) {
            cdc_expect_string(replay_id, job->expect_replay,
                              "persist replay expectation mismatch");
        }
        if (job->has_expect_sealed) {
            cdc_expect_int((int)sealed, job->expect_sealed,
                           "persist sealed expectation mismatch");
        }
        if (job->has_expect_events) {
            cdc_expect_int((int)events, job->expect_events,
                           "persist events expectation mismatch");
        }

        {
            cdc_receipt receipt;
            cdc_receipt_init(&receipt);
            snprintf(receipt.kind, sizeof(receipt.kind), "persist");
            snprintf(receipt.job, sizeof(receipt.job), "%s", job->id);
            snprintf(receipt.op, sizeof(receipt.op), "%s", job->op);
            receipt.outcome = strcmp(status, "accepted") == 0
                                  ? CDC_OUTCOME_ACCEPTED
                                  : CDC_OUTCOME_HELD;
            snprintf(receipt.reason, sizeof(receipt.reason), "%s", reason);
            receipt.declared_hold = job->expect_status[0] &&
                                    strcmp(job->expect_status, "held") == 0;
            receipt.durable = strcmp(durable, "yes") == 0;
            receipt.replay_stable = strcmp(replay_id, "stable") == 0;
            receipt.sealed = (long)sealed;
            receipt.events = (long)events;
            receipt.generation =
                (long)cdc_store_generation(decl->handle);
            if (is_append) {
                snprintf(receipt.trits, sizeof(receipt.trits), "%s",
                         decision.trits);
                snprintf(receipt.balance, sizeof(receipt.balance), "%s",
                         decision.balance);
            }
            attach_closure(rt, &receipt, job->id);
            emit_receipt(&receipt);
        }
        if (is_append) {
            printf("persist=%s store=%s op=append module=%s trits=%s "
                   "balance=%s status=%s reason=%s sealed=%llu events=%llu "
                   "durable=%s replay=%s\n",
                   job->id, decl->id, decision.module, decision.trits,
                   decision.balance, status, reason, sealed, events, durable,
                   replay_id);
        } else {
            printf("persist=%s store=%s op=%s status=%s reason=%s "
                   "sealed=%llu events=%llu durable=%s replay=%s\n",
                   job->id, decl->id, job->op, status, reason, sealed, events,
                   durable, replay_id);
        }
        if (strcmp(status, "accepted") == 0) {
            accepted++;
        } else {
            held++;
        }
        lifecycle_count();
    }
    for (int i = 0; i < rt->store_count; i++) {
        cdc_store_close(rt->stores[i].handle);
        rt->stores[i].handle = NULL;
    }
    printf("native persistence ok stores=%d jobs=%d accepted=%d held=%d "
           "source=%s\n",
           rt->store_count, rt->persist_count, accepted, held, path);
}

static Channel *find_channel_by_id(Runtime *rt, const char *id) {
    for (int i = 0; i < rt->channel_count; i++) {
        if (strcmp(rt->channels[i].id, id) == 0) {
            return &rt->channels[i];
        }
    }
    return NULL;
}

static SurfaceBridgeJob *find_bridge_job(Runtime *rt, const char *id) {
    for (int i = 0; i < rt->bridge_count; i++) {
        if (strcmp(rt->bridges[i].id, id) == 0) {
            return &rt->bridges[i];
        }
    }
    return NULL;
}

static Deliberation *find_deliberation(Runtime *rt, const char *id) {
    for (int i = 0; i < rt->deliberation_count; i++) {
        if (strcmp(rt->deliberations[i].id, id) == 0) {
            return &rt->deliberations[i];
        }
    }
    return NULL;
}

static EvolutionJob *find_evolution_job(Runtime *rt, const char *id) {
    for (int i = 0; i < rt->evolution_count; i++) {
        if (strcmp(rt->evolutions[i].id, id) == 0) {
            return &rt->evolutions[i];
        }
    }
    return NULL;
}

static Step *find_step(Runtime *rt, const char *id) {
    for (int i = 0; i < rt->step_count; i++) {
        if (strcmp(rt->steps[i].id, id) == 0) {
            return &rt->steps[i];
        }
    }
    return NULL;
}

static void universal_hold(UniversalResult *res, const char *reason) {
    snprintf(res->status, sizeof(res->status), "held");
    snprintf(res->reason, sizeof(res->reason), "%s", reason);
}

/* The lifted frame is a double cover: projected phase lives modulo 2*pi while
   winding parity carries the Z2 sheet, so one turn returns the projection with
   an inverted sheet and only two turns restore both. */
static void evaluate_cover(double theta, double theta0, double tolerance,
                           char *projection, size_t projection_size,
                           char *sheet, size_t sheet_size, int *winding) {
    double gap = fmod(theta - theta0, 2.0 * PI);
    if (gap < 0.0) {
        gap += 2.0 * PI;
    }
    if (gap > PI) {
        gap = 2.0 * PI - gap;
    }
    *winding = (int)floor((theta - theta0) / (2.0 * PI) + 0.5);
    snprintf(projection, projection_size, "%s", gap <= tolerance ? "returned" : "displaced");
    snprintf(sheet, sheet_size, "%s", (*winding % 2 == 0) ? "restored" : "inverted");
}

static int universal_step_kind_ok(Step *step) {
    return step && step->kind == STEP_FLOW;
}

static void run_universal_enactment(EvolutionJob *enact, UniversalJob *job, UniversalResult *res) {
    FILE *in = fopen(enact->source, "r");
    FILE *out;
    char line[LINE_MAX_BYTES];
    if (!in) {
        fail("universal enactment source could not be opened");
    }
    out = fopen(enact->output, "w");
    if (!out) {
        fclose(in);
        fail("universal enactment output could not be opened");
    }
    while (fgets(line, sizeof(line), in)) {
        fputs(line, out);
    }
    fprintf(out, "\n# universal closure %s enacted computed coordinate %s\n", job->id, res->record_coordinate);
    fprintf(out,
            "witness %s invariant=universal-closure coordinate=%s winding=%d sheet=%s holonomy=%s status=%s claim=\"universal operator closed the lifted frame and enacted its computed record\"\n",
            enact->append_witness, res->record_coordinate, res->winding, res->full_sheet,
            res->holonomy, res->status);
    fclose(in);
    fclose(out);
    if (enact->expect_contains[0] && !file_contains(enact->output, enact->expect_contains)) {
        fail("universal enactment output missing expected text");
    }
    snprintf(res->enacted_coordinate, sizeof(res->enacted_coordinate), "%s", res->record_coordinate);
}

static void universal_parity_check(Runtime *snapshot, Runtime *live, double tolerance) {
    int ops = 0;
    for (int i = 0; i < snapshot->step_count; i++) {
        Step *step = &snapshot->steps[i];
        if (step->kind == STEP_FLOW) {
            FlowResult flow;
            execute_flow(snapshot, step, &flow);
        } else if (step->kind == STEP_COMMIT) {
            CommitResult commit;
            execute_commit(snapshot, step, &commit);
        } else {
            NestResult nest;
            execute_nest(snapshot, step, &nest);
        }
        ops++;
    }
    for (int i = 0; i < snapshot->cell_count; i++) {
        if (!cdc_close_enough(snapshot->cells[i].theta, live->cells[i].theta, tolerance)) {
            fail("universal interpreter parity theta mismatch");
        }
    }
    for (int i = 0; i < snapshot->module_count; i++) {
        if (!cdc_close_enough(snapshot->modules[i].belief, live->modules[i].belief, tolerance) ||
            !cdc_close_enough(snapshot->modules[i].prior, live->modules[i].prior, tolerance)) {
            fail("universal interpreter parity belief mismatch");
        }
    }
    printf("universal-parity ops=%d ok\n", ops);
}

static int execute_universal(Runtime *rt, UniversalJob *job, UniversalResult *res, Runtime *snapshot) {
    Module *frame = find_module(rt, job->frame);
    Cell *cover = find_cell(rt, job->cover_cell);
    Channel *receptive = find_channel_by_id(rt, job->receptive);
    Channel *radiant = find_channel_by_id(rt, job->radiant);
    SurfaceBridgeJob *record = find_bridge_job(rt, job->record);
    Deliberation *decision = find_deliberation(rt, job->decision);
    EvolutionJob *enact = find_evolution_job(rt, job->enact);
    Step *half = find_step(rt, job->half_step);
    Step *full = find_step(rt, job->full_step);
    double theta0;
    double holonomy;
    int frame_flow = 0;
    int winding = 0;

    memset(res, 0, sizeof(*res));
    snprintf(res->frame, sizeof(res->frame), "%s", job->frame);
    snprintf(res->status, sizeof(res->status), "accepted");
    snprintf(res->reason, sizeof(res->reason), "none");
    snprintf(res->half_projection, sizeof(res->half_projection), "unchecked");
    snprintf(res->half_sheet, sizeof(res->half_sheet), "unchecked");
    snprintf(res->full_projection, sizeof(res->full_projection), "unchecked");
    snprintf(res->full_sheet, sizeof(res->full_sheet), "unchecked");
    snprintf(res->record_coordinate, sizeof(res->record_coordinate), "-");
    snprintf(res->decision_coordinate, sizeof(res->decision_coordinate), "-");
    snprintf(res->enacted_coordinate, sizeof(res->enacted_coordinate), "-");

    if (!frame || !cover) {
        fail("universal job references unknown frame or cover cell");
    }
    if (!receptive || !radiant) {
        fail("universal job references unknown receptive or radiant channel");
    }
    if (!record || !decision || !enact) {
        fail("universal job references unknown record, decision, or enact job");
    }
    if (!universal_step_kind_ok(half) || !universal_step_kind_ok(full)) {
        fail("universal half/full steps must be declared flow steps");
    }
    if (strcmp(job->cover, "double") != 0) {
        fail("universal cover currently supports double");
    }
    snprintf(res->receptive_angle, sizeof(res->receptive_angle), "%.6f", receptive->angle);
    snprintf(res->radiant_angle, sizeof(res->radiant_angle), "%.6f", radiant->angle);
    holonomy = receptive->angle + radiant->angle;
    snprintf(res->holonomy, sizeof(res->holonomy), "%.6f", holonomy);

    if (strcmp(receptive->cone, "receptive") != 0 || strcmp(radiant->cone, "radiant") != 0 ||
        receptive->pair[0] == '\0' || strcmp(receptive->pair, radiant->pair) != 0 ||
        strcmp(receptive->source, radiant->target) != 0 ||
        strcmp(receptive->target, radiant->source) != 0 ||
        receptive->angle == 0.0 || radiant->angle == 0.0 ||
        !cell_in_field(rt, find_cell(rt, receptive->source), frame->field) ||
        !cell_in_field(rt, find_cell(rt, receptive->target), frame->field)) {
        universal_hold(res, "cone-not-reciprocal");
        return 0;
    }
    for (int i = 0; i < rt->step_count; i++) {
        if (rt->steps[i].kind == STEP_FLOW && strcmp(rt->steps[i].field, frame->field) == 0) {
            frame_flow = 1;
        }
    }
    if (!frame_flow) {
        universal_hold(res, "cone-not-reciprocal");
        return 0;
    }

    if (snapshot) {
        *snapshot = *rt;
    }
    theta0 = cover->theta;

    for (int i = 0; i < rt->step_count; i++) {
        Step *step = &rt->steps[i];
        if (step->kind == STEP_FLOW) {
            FlowResult flow;
            execute_flow(rt, step, &flow);
        } else if (step->kind == STEP_COMMIT) {
            CommitResult commit;
            execute_commit(rt, step, &commit);
            if (strcmp(commit.status, "accepted") != 0) {
                universal_hold(res, "local-commit-held");
                return 0;
            }
        } else {
            NestResult nest;
            execute_nest(rt, step, &nest);
        }
        if (strcmp(step->id, job->half_step) == 0) {
            evaluate_cover(cover->theta, theta0, job->tolerance,
                           res->half_projection, sizeof(res->half_projection),
                           res->half_sheet, sizeof(res->half_sheet), &winding);
            if (strcmp(res->half_projection, "returned") != 0) {
                universal_hold(res, "half-projection-mismatch");
                return 0;
            }
            if (strcmp(res->half_sheet, "inverted") != 0) {
                universal_hold(res, "half-sheet-mismatch");
                return 0;
            }
        }
        if (strcmp(step->id, job->full_step) == 0) {
            evaluate_cover(cover->theta, theta0, job->tolerance,
                           res->full_projection, sizeof(res->full_projection),
                           res->full_sheet, sizeof(res->full_sheet), &winding);
            res->winding = winding;
            if (strcmp(res->full_projection, "returned") != 0) {
                universal_hold(res, "full-projection-mismatch");
                return 0;
            }
            if (strcmp(res->full_sheet, "restored") != 0) {
                universal_hold(res, "full-sheet-mismatch");
                return 0;
            }
            if (winding != 2) {
                universal_hold(res, "full-projection-mismatch");
                return 0;
            }
        }
    }

    if (job->has_expect_holonomy &&
        !cdc_close_enough(holonomy, job->expect_holonomy, job->tolerance)) {
        universal_hold(res, "holonomy-mismatch");
        return 0;
    }

    {
        char trits[128];
        char record_dyadic[7];
        char record_triadic[4];
        char decision_trits[128];
        char decision_dyadic[7];
        char decision_triadic[4];
        char decision_word[32];
        int record_index;
        int occupancy;
        int quorum;
        execute_surface_bridge(rt, record, trits, record_dyadic, record_triadic, &record_index);
        snprintf(res->record_coordinate, sizeof(res->record_coordinate), "%s", record_dyadic);
        execute_deliberation(rt, decision, decision_trits, decision_dyadic, decision_triadic,
                             &occupancy, &quorum, decision_word);
        snprintf(res->decision_coordinate, sizeof(res->decision_coordinate), "%s", decision_dyadic);
        if (strcmp(decision_word, "adopt") != 0 ||
            strcmp(record_dyadic, decision_dyadic) != 0) {
            universal_hold(res, "coordinate-mismatch");
            return 0;
        }
    }

    if (job->expect_coordinate[0]) {
        cdc_expect_string(res->record_coordinate, job->expect_coordinate,
                          "universal computed coordinate expectation mismatch");
    }
    return 1;
}

static void run_universal(Runtime *rt, const char *path) {
    if (rt->universal_count == 0) {
        fail("source has no universal job");
    }
    for (int i = 0; i < rt->universal_count; i++) {
        UniversalJob *job = &rt->universals[i];
        UniversalResult res;
        Runtime snapshot;
        int accepted = execute_universal(rt, job, &res, &snapshot);
        if (accepted) {
            universal_parity_check(&snapshot, rt, job->tolerance);
            run_universal_enactment(find_evolution_job(rt, job->enact), job, &res);
            printf("universal-enactment=%s coordinate=%s output=%s appended=%s\n",
                   job->enact, res.enacted_coordinate,
                   find_evolution_job(rt, job->enact)->output,
                   find_evolution_job(rt, job->enact)->append_witness);
        }
        {
            cdc_receipt receipt;
            cdc_receipt_init(&receipt);
            snprintf(receipt.kind, sizeof(receipt.kind), "universal");
            snprintf(receipt.job, sizeof(receipt.job), "%s", job->id);
            receipt.outcome = strcmp(res.status, "accepted") == 0
                                  ? CDC_OUTCOME_ACCEPTED
                                  : CDC_OUTCOME_HELD;
            snprintf(receipt.reason, sizeof(receipt.reason), "%s", res.reason);
            receipt.declared_hold = job->expect_status[0] &&
                                    strcmp(job->expect_status, "held") == 0;
            attach_closure(rt, &receipt, job->id);
            emit_receipt(&receipt);
        }
        printf("universal=%s frame=%s receptive=%s radiant=%s holonomy=%s half-projection=%s half-sheet=%s full-projection=%s full-sheet=%s winding=%d record=%s decision=%s enacted=%s status=%s reason=%s\n",
               job->id, res.frame, res.receptive_angle, res.radiant_angle, res.holonomy,
               res.half_projection, res.half_sheet, res.full_projection, res.full_sheet,
               res.winding, res.record_coordinate, res.decision_coordinate,
               res.enacted_coordinate, res.status, res.reason);
        if (job->expect_status[0]) {
            if (!is_commit_status(job->expect_status)) {
                fail("unknown universal status expectation");
            }
            cdc_expect_string(res.status, job->expect_status, "universal status expectation mismatch");
        }
        if (strcmp(res.status, "held") == 0 && job->expect_reason[0]) {
            cdc_expect_string(res.reason, job->expect_reason, "universal reason expectation mismatch");
        }
        if (accepted) {
            if (job->expect_half_projection[0]) {
                cdc_expect_string(res.half_projection, job->expect_half_projection, "universal half projection expectation mismatch");
            }
            if (job->expect_half_sheet[0]) {
                cdc_expect_string(res.half_sheet, job->expect_half_sheet, "universal half sheet expectation mismatch");
            }
            if (job->expect_full_projection[0]) {
                cdc_expect_string(res.full_projection, job->expect_full_projection, "universal full projection expectation mismatch");
            }
            if (job->expect_full_sheet[0]) {
                cdc_expect_string(res.full_sheet, job->expect_full_sheet, "universal full sheet expectation mismatch");
            }
        }
    }
    printf("native universal ok jobs=%d source=%s\n", rt->universal_count, path);
}

typedef struct {
    cdc_u2_layout layout;
    cdc_u2_monodromy tangent;
    double initial[MAX_U2_DIMENSION];
    double final[MAX_U2_DIMENSION];
    double endpoint_time;
    char reason[48];
} NativeU2Tangent;

static UniversalJob *find_universal_job(Runtime *rt, const char *id) {
    for (int i = 0; i < rt->universal_count; i++) {
        if (strcmp(rt->universals[i].id, id) == 0) {
            return &rt->universals[i];
        }
    }
    return NULL;
}

static OrbitJob *find_orbit_job(Runtime *rt, const char *id) {
    for (int i = 0; i < rt->orbit_count; i++) {
        if (strcmp(rt->orbits[i].id, id) == 0) {
            return &rt->orbits[i];
        }
    }
    return NULL;
}

static VariationalJob *find_variational_job(Runtime *rt, const char *id) {
    for (int i = 0; i < rt->variational_count; i++) {
        if (strcmp(rt->variationals[i].id, id) == 0) {
            return &rt->variationals[i];
        }
    }
    return NULL;
}

static void u2_tangent_release(NativeU2Tangent *tangent) {
    if (!tangent) {
        return;
    }
    cdc_u2_layout_release(&tangent->layout);
    cdc_u2_monodromy_release(&tangent->tangent);
    memset(tangent, 0, sizeof(*tangent));
}

static void u2_select_module_cells(Runtime *rt, const char *module_name,
                                   unsigned char *selected_cells,
                                   unsigned char *selected_modules) {
    Module *module = find_module(rt, module_name);
    if (!module) {
        fail("U2 path closure references an unknown module");
    }
    selected_modules[(size_t)(module - rt->modules)] = 1;
    for (int i = 0; i < rt->cell_count; i++) {
        if (strcmp(rt->cells[i].module, module->name) == 0) {
            selected_cells[i] = 1;
        }
    }
}

static void u2_select_field_cells(Runtime *rt, const char *field_name,
                                  unsigned char *selected_cells,
                                  unsigned char *selected_modules) {
    if (!find_field(rt, field_name)) {
        fail("U2 path closure references an unknown field");
    }
    for (int i = 0; i < rt->cell_count; i++) {
        if (cell_in_field(rt, &rt->cells[i], field_name)) {
            selected_cells[i] = 1;
            u2_select_module_cells(rt, rt->cells[i].module, selected_cells,
                                   selected_modules);
        }
    }
}

static void u2_select_council_cells(Runtime *rt, const char *deliberation_id,
                                    unsigned char *selected_cells,
                                    unsigned char *selected_modules) {
    Deliberation *deliberation = find_deliberation(rt, deliberation_id);
    Council *council = NULL;
    char members[sizeof(((Council *)0)->members)];
    char *save = NULL;
    char *member;
    if (!deliberation || !(council = find_council(rt, deliberation->council))) {
        fail("U2 path closure references an unknown deliberation or council");
    }
    snprintf(members, sizeof(members), "%s", council->members);
    member = strtok_r(members, ",", &save);
    while (member) {
        u2_select_module_cells(rt, member, selected_cells, selected_modules);
        member = strtok_r(NULL, ",", &save);
    }
}

/* Build the executable state closure of this Universal, rather than a closure
 * of every declaration in the source file.  U1 executes the ordered step
 * stream and then reads the selected bridge and council.  The closure therefore
 * includes coordinates touched or read by those exact operations, plus the
 * lifted cover coordinate.  Selecting a cell closes over its owning module;
 * selecting a module closes over all of its cells so continuous state and the
 * discrete latch itinerary describe the same runtime object. */
static cdc_u2_status u2_build_layout(Runtime *rt, UniversalJob *universal,
                                     cdc_u2_layout *layout) {
    const char *cell_names[MAX_CELLS];
    const char *module_names[MAX_MODULES];
    size_t cell_count = 0;
    size_t module_count = 0;
    unsigned char selected_cells[MAX_CELLS] = {0};
    unsigned char selected_modules[MAX_MODULES] = {0};
    SurfaceBridgeJob *record;
    TraceJob *trace;
    int cover_source_index;
    long cover_coordinate;
    cdc_u2_status status;
    for (int i = 0; i < rt->step_count; i++) {
        Step *step = &rt->steps[i];
        if (step->kind == STEP_FLOW) {
            u2_select_field_cells(rt, step->field, selected_cells,
                                  selected_modules);
            if (step->has_expect_theta) {
                int expected = find_cell_index(rt, step->expect_theta_cell);
                if (expected < 0) {
                    return CDC_U2_INVALID_ARGUMENT;
                }
                selected_cells[expected] = 1;
                u2_select_module_cells(rt, rt->cells[expected].module,
                                       selected_cells, selected_modules);
            }
        } else if (step->kind == STEP_COMMIT) {
            u2_select_module_cells(rt, step->module, selected_cells,
                                   selected_modules);
        } else {
            u2_select_module_cells(rt, step->parent, selected_cells,
                                   selected_modules);
            u2_select_module_cells(rt, step->child, selected_cells,
                                   selected_modules);
        }
    }
    cover_source_index = find_cell_index(rt, universal->cover_cell);
    if (cover_source_index < 0) {
        return CDC_U2_INVALID_ARGUMENT;
    }
    selected_cells[cover_source_index] = 1;
    u2_select_module_cells(rt, rt->cells[cover_source_index].module,
                           selected_cells, selected_modules);
    record = find_bridge_job(rt, universal->record);
    trace = record ? find_trace(rt, record->trace) : NULL;
    if (!trace) {
        return CDC_U2_INVALID_ARGUMENT;
    }
    u2_select_field_cells(rt, trace->field, selected_cells, selected_modules);
    u2_select_council_cells(rt, universal->decision, selected_cells,
                            selected_modules);
    for (int i = 0; i < rt->cell_count; i++) {
        if (selected_cells[i]) {
            cell_names[cell_count++] = rt->cells[i].name;
        }
    }
    for (int i = 0; i < rt->module_count; i++) {
        if (selected_modules[i]) {
            module_names[module_count++] = rt->modules[i].name;
        }
    }
    status = cdc_u2_layout_build(layout, cell_names, cell_count,
                                 module_names, module_count);
    if (status != CDC_U2_OK) {
        return status;
    }
    cell_count = 0;
    for (int i = 0; i < rt->cell_count; i++) {
        if (selected_cells[i]) {
            layout->coordinates[cell_count++].source_index = (size_t)i;
        }
    }
    module_count = 0;
    for (int i = 0; i < rt->module_count; i++) {
        if (selected_modules[i]) {
            size_t coordinate = layout->cell_count + 2 * module_count++;
            layout->coordinates[coordinate].source_index = (size_t)i;
            layout->coordinates[coordinate + 1].source_index = (size_t)i;
        }
    }
    /* The U1 cover cell is an unwrapped lifted coordinate.  Its 2*pi/4*pi
     * projection may close while the executable coordinate does not. */
    {
        char name[CDC_U2_COORDINATE_NAME_MAX];
        int written = snprintf(name, sizeof(name), "%s.theta",
                               universal->cover_cell);
        cover_coordinate = written > 0 && (size_t)written < sizeof(name)
                               ? cdc_u2_layout_find(layout, name)
                               : -1;
    }
    if (cover_coordinate < 0 ||
        cdc_u2_layout_set_period(layout, (size_t)cover_coordinate, 0.0) !=
            CDC_U2_OK) {
        cdc_u2_layout_release(layout);
        return CDC_U2_INVALID_ARGUMENT;
    }
    return CDC_U2_OK;
}

static cdc_u2_status u2_pack_runtime(Runtime *rt, const cdc_u2_layout *layout,
                                     double *packed) {
    if (!layout || !layout->coordinates || !packed) {
        return CDC_U2_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < layout->dimension; i++) {
        const cdc_u2_coordinate *coordinate = &layout->coordinates[i];
        if (coordinate->kind == CDC_U2_COORDINATE_THETA) {
            if (coordinate->source_index >= (size_t)rt->cell_count) {
                return CDC_U2_DIMENSION_MISMATCH;
            }
            packed[i] = rt->cells[coordinate->source_index].theta;
        } else {
            if (coordinate->source_index >= (size_t)rt->module_count) {
                return CDC_U2_DIMENSION_MISMATCH;
            }
            packed[i] = coordinate->kind == CDC_U2_COORDINATE_BELIEF
                            ? rt->modules[coordinate->source_index].belief
                            : rt->modules[coordinate->source_index].prior;
        }
        if (!isfinite(packed[i])) {
            return CDC_U2_NONFINITE;
        }
    }
    return CDC_U2_OK;
}

static int u2_discrete_state_equal(const cdc_u2_layout *layout,
                                   const Runtime *initial,
                                   const Runtime *final) {
    if (!layout || initial->cell_count != final->cell_count) {
        return 0;
    }
    for (size_t i = 0; i < layout->dimension; i++) {
        const cdc_u2_coordinate *coordinate = &layout->coordinates[i];
        size_t source_index = coordinate->source_index;
        if (coordinate->kind != CDC_U2_COORDINATE_THETA) {
            continue;
        }
        if (source_index >= (size_t)initial->cell_count ||
            initial->cells[source_index].has_latch !=
                final->cells[source_index].has_latch ||
            initial->cells[source_index].latch !=
                final->cells[source_index].latch) {
            return 0;
        }
    }
    return 1;
}

static int u2_commit_has_smooth_itinerary(Runtime *rt, Step *step,
                                          double tolerance) {
    Module *module = find_module(rt, step->module);
    Field *field;
    if (!module || !(field = find_field(rt, module->field))) {
        return 0;
    }
    for (int i = 0; i < rt->cell_count; i++) {
        double kappa;
        double margin;
        if (strcmp(rt->cells[i].module, module->name) != 0) {
            continue;
        }
        kappa = cos(rt->cells[i].theta);
        margin = fmin(fabs(kappa - field->deadband),
                      fabs(kappa + field->deadband));
        if (!isfinite(margin) || margin <= tolerance) {
            return 0;
        }
    }
    return 1;
}

static cdc_u2_status u2_flow_local_map(Runtime *rt, Step *step,
                                       const cdc_u2_layout *layout,
                                       cdc_matrix *local,
                                       char *reason, size_t reason_size) {
    Field *field = find_field(rt, step->field);
    cdc_u2_flow_coupling couplings[MAX_CHANNELS];
    double state[MAX_U2_DIMENSION];
    size_t count = 0;
    cdc_u2_status status;
    if (!field) {
        return CDC_U2_INVALID_ARGUMENT;
    }
    status = u2_pack_runtime(rt, layout, state);
    if (status != CDC_U2_OK) {
        return status;
    }
    for (int i = 0; i < rt->channel_count; i++) {
        Channel *channel = &rt->channels[i];
        int source_index = find_cell_index(rt, channel->source);
        int target_index = find_cell_index(rt, channel->target);
        char source_name[CDC_U2_COORDINATE_NAME_MAX];
        char target_name[CDC_U2_COORDINATE_NAME_MAX];
        long source_coordinate;
        long target_coordinate;
        if (source_index < 0 || target_index < 0) {
            return CDC_U2_INVALID_ARGUMENT;
        }
        if (!cell_in_field(rt, &rt->cells[source_index], field->name) ||
            !cell_in_field(rt, &rt->cells[target_index], field->name)) {
            continue;
        }
        if (channel->delay != 0.0) {
            snprintf(reason, reason_size, "unsupported-delay-state");
            return CDC_U2_VALIDATION_FAILED;
        }
        if (snprintf(source_name, sizeof(source_name), "%s.theta",
                     channel->source) <= 0 ||
            snprintf(target_name, sizeof(target_name), "%s.theta",
                     channel->target) <= 0 ||
            (source_coordinate = cdc_u2_layout_find(layout, source_name)) < 0 ||
            (target_coordinate = cdc_u2_layout_find(layout, target_name)) < 0) {
            return CDC_U2_DIMENSION_MISMATCH;
        }
        couplings[count].source_theta = (size_t)source_coordinate;
        couplings[count].target_theta = (size_t)target_coordinate;
        couplings[count].coefficient =
            field->gain * channel->weight * step->duration;
        couplings[count].angle = channel->angle;
        count++;
    }
    return cdc_u2_flow_jacobian(layout, state, layout->dimension,
                                couplings, count, local);
}

static cdc_u2_status u2_execute_tangent(Runtime *initial, UniversalJob *universal,
                                        double tolerance,
                                        NativeU2Tangent *out) {
    Runtime execution = *initial;
    cdc_matrix local;
    cdc_u2_status status;
    double time = 0.0;
    memset(out, 0, sizeof(*out));
    memset(&local, 0, sizeof(local));
    snprintf(out->reason, sizeof(out->reason), "none");
    status = u2_build_layout(&execution, universal, &out->layout);
    if (status != CDC_U2_OK) {
        snprintf(out->reason, sizeof(out->reason), "state-manifest-mismatch");
        return status;
    }
    status = u2_pack_runtime(&execution, &out->layout, out->initial);
    if (status != CDC_U2_OK) {
        snprintf(out->reason, sizeof(out->reason), "%s",
                 cdc_u2_status_reason(status));
        return status;
    }
    status = cdc_u2_monodromy_init(&out->tangent, out->layout.dimension);
    if (status != CDC_U2_OK ||
        cdc_matrix_init(&local, out->layout.dimension,
                       out->layout.dimension) != CDC_LINALG_OK) {
        snprintf(out->reason, sizeof(out->reason), "allocation-failed");
        cdc_matrix_release(&local);
        return CDC_U2_ALLOCATION_FAILED;
    }
    for (int i = 0; i < execution.step_count; i++) {
        Step *step = &execution.steps[i];
        cdc_u2_event_kind kind;
        int transverse = -1;
        if (step->kind == STEP_FLOW) {
            FlowResult flow;
            if (!isfinite(step->duration) || step->duration < 0.0) {
                status = CDC_U2_INVALID_ARGUMENT;
                snprintf(out->reason, sizeof(out->reason),
                         "unsupported-primal-semantic");
                goto done;
            }
            status = u2_flow_local_map(&execution, step, &out->layout,
                                       &local, out->reason,
                                       sizeof(out->reason));
            if (status != CDC_U2_OK) {
                goto done;
            }
            execute_flow(&execution, step, &flow);
            time += step->duration;
            kind = CDC_U2_EVENT_FLOW;
        } else if (step->kind == STEP_COMMIT) {
            CommitResult commit;
            if (!u2_commit_has_smooth_itinerary(&execution, step, tolerance)) {
                status = CDC_U2_MODE_DIVERGENCE;
                snprintf(out->reason, sizeof(out->reason),
                         "nondifferentiable-quantization");
                goto done;
            }
            execute_commit(&execution, step, &commit);
            if (strcmp(commit.status, "accepted") != 0) {
                status = CDC_U2_MODE_DIVERGENCE;
                snprintf(out->reason, sizeof(out->reason), "primal-held");
                goto done;
            }
            status = cdc_u2_commit_jacobian(out->layout.dimension,
                                            CDC_U2_COMMIT_SCHEDULED, 1,
                                            &local);
            if (status != CDC_U2_OK) {
                goto done;
            }
            kind = CDC_U2_EVENT_SCHEDULED_COMMIT;
        } else {
            NestResult nest;
            Module *parent = find_module(&execution, step->parent);
            Module *child = find_module(&execution, step->child);
            char parent_belief[CDC_U2_COORDINATE_NAME_MAX];
            char child_belief[CDC_U2_COORDINATE_NAME_MAX];
            char child_prior[CDC_U2_COORDINATE_NAME_MAX];
            long parent_index;
            long child_index;
            long child_prior_index;
            if (!parent || !child) {
                status = CDC_U2_INVALID_ARGUMENT;
                goto done;
            }
            if (snprintf(parent_belief, sizeof(parent_belief), "%s.belief",
                         parent->name) <= 0 ||
                snprintf(child_belief, sizeof(child_belief), "%s.belief",
                         child->name) <= 0 ||
                snprintf(child_prior, sizeof(child_prior), "%s.prior",
                         child->name) <= 0 ||
                (parent_index = cdc_u2_layout_find(&out->layout,
                                                   parent_belief)) < 0 ||
                (child_index = cdc_u2_layout_find(&out->layout,
                                                  child_belief)) < 0 ||
                (child_prior_index = cdc_u2_layout_find(&out->layout,
                                                        child_prior)) < 0) {
                status = CDC_U2_DIMENSION_MISMATCH;
                goto done;
            }
            status = cdc_u2_nest_jacobian(
                out->layout.dimension,
                (size_t)parent_index, (size_t)child_index,
                (size_t)child_prior_index,
                find_field(&execution, parent->field)->gain, &local);
            if (status != CDC_U2_OK) {
                goto done;
            }
            execute_nest(&execution, step, &nest);
            kind = CDC_U2_EVENT_NEST;
        }
        status = cdc_u2_monodromy_append(&out->tangent, &local, kind, time,
                                         transverse);
        if (status != CDC_U2_OK) {
            snprintf(out->reason, sizeof(out->reason), "%s",
                     cdc_u2_status_reason(status));
            goto done;
        }
    }
    status = u2_pack_runtime(&execution, &out->layout, out->final);
    if (status == CDC_U2_OK) {
        out->endpoint_time = time;
    }

done:
    if (status != CDC_U2_OK && strcmp(out->reason, "none") == 0) {
        snprintf(out->reason, sizeof(out->reason), "%s",
                 cdc_u2_status_reason(status));
    }
    cdc_matrix_release(&local);
    return status;
}

static long u2_coordinate_index(Runtime *rt, const cdc_u2_layout *layout,
                                const char *name) {
    long exact = cdc_u2_layout_find(layout, name);
    char theta_name[CDC_U2_COORDINATE_NAME_MAX];
    int written;
    if (exact >= 0) {
        return exact;
    }
    if (find_cell(rt, name)) {
        written = snprintf(theta_name, sizeof(theta_name), "%s.theta", name);
        if (written > 0 && (size_t)written < sizeof(theta_name)) {
            return cdc_u2_layout_find(layout, theta_name);
        }
    }
    return -1;
}

static int u2_apply_coordinate_list(Runtime *rt, const cdc_u2_layout *layout,
                                    const char *text, unsigned char *mask,
                                    int value) {
    char copy[MAX_U2_COORDINATE_TEXT];
    char *save = NULL;
    char *token;
    if (!text || text[0] == '\0') {
        return 1;
    }
    if ((size_t)snprintf(copy, sizeof(copy), "%s", text) >= sizeof(copy)) {
        return 0;
    }
    token = strtok_r(copy, ",", &save);
    while (token) {
        long index = u2_coordinate_index(rt, layout, token);
        if (index < 0) {
            return 0;
        }
        mask[index] = (unsigned char)value;
        token = strtok_r(NULL, ",", &save);
    }
    return 1;
}

static int u2_coordinate_mask(Runtime *rt, const cdc_u2_layout *layout,
                              OrbitJob *orbit, unsigned char *mask,
                              int *projected) {
    if (strcmp(orbit->coordinates, "all") == 0) {
        memset(mask, 1, layout->dimension);
    } else {
        memset(mask, 0, layout->dimension);
        if (!u2_apply_coordinate_list(rt, layout, orbit->coordinates, mask, 1)) {
            return 0;
        }
    }
    if (!u2_apply_coordinate_list(rt, layout, orbit->exclude, mask, 0)) {
        return 0;
    }
    *projected = 0;
    for (size_t i = 0; i < layout->dimension; i++) {
        if (!mask[i]) {
            *projected = 1;
        }
    }
    return 1;
}

/* U2 receipts cross macOS and Linux, whose libm implementations may return
 * adjacent IEEE-754 values for the same transcendental operation.  Fifteen
 * significant decimal digits preserve the meaningful double-precision
 * result while collapsing that sub-receipt, one-ULP platform noise.  The
 * same formatter feeds both JSON artifacts and their digests. */
static int u2_format_canonical_float(char *buffer, size_t size, double value) {
    return snprintf(buffer, size, "%.15g", value == 0.0 ? 0.0 : value);
}

static void u2_emit_canonical_float(double value) {
    char buffer[64];
    int written = u2_format_canonical_float(buffer, sizeof(buffer), value);
    if (written > 0 && (size_t)written < sizeof(buffer)) {
        fwrite(buffer, 1, (size_t)written, stdout);
    }
}

static void u2_digest_layout(const cdc_u2_layout *layout, char *hex,
                             size_t hex_size) {
    cdc_digest_ctx context;
    uint8_t digest[CDC_DIGEST_SIZE];
    char line[256];
    cdc_digest_init(&context);
    for (size_t i = 0; i < layout->dimension; i++) {
        char period[64];
        int period_written = u2_format_canonical_float(
            period, sizeof(period), layout->coordinates[i].period);
        int written = period_written > 0 &&
                              (size_t)period_written < sizeof(period)
                          ? snprintf(line, sizeof(line), "%zu|%s|%d|%zu|%s\n",
                                     i, layout->coordinates[i].name,
                                     (int)layout->coordinates[i].kind,
                                     layout->coordinates[i].source_index, period)
                          : -1;
        if (written > 0 && (size_t)written < sizeof(line)) {
            cdc_digest_update(&context, line, (size_t)written);
        }
    }
    cdc_digest_final(&context, digest);
    cdc_digest_hex(digest, hex, hex_size);
}

static void u2_digest_matrix(const cdc_matrix *matrix, char *hex,
                             size_t hex_size) {
    cdc_digest_ctx context;
    uint8_t digest[CDC_DIGEST_SIZE];
    char line[96];
    int written;
    if (!matrix || !matrix->data || matrix->rows == 0 || matrix->cols == 0) {
        if (hex_size) {
            hex[0] = '\0';
        }
        return;
    }
    cdc_digest_init(&context);
    written = snprintf(line, sizeof(line), "%zu|%zu\n", matrix->rows,
                       matrix->cols);
    if (written > 0 && (size_t)written < sizeof(line)) {
        cdc_digest_update(&context, line, (size_t)written);
    }
    for (size_t i = 0; i < matrix->rows * matrix->cols; i++) {
        char value[64];
        int value_written =
            u2_format_canonical_float(value, sizeof(value), matrix->data[i]);
        written = value_written > 0 && (size_t)value_written < sizeof(value)
                      ? snprintf(line, sizeof(line), "%s\n", value)
                      : -1;
        if (written > 0 && (size_t)written < sizeof(line)) {
            cdc_digest_update(&context, line, (size_t)written);
        }
    }
    cdc_digest_final(&context, digest);
    cdc_digest_hex(digest, hex, hex_size);
}

static void u2_digest_universal(const UniversalJob *job,
                                const UniversalResult *result,
                                char *hex, size_t hex_size) {
    cdc_digest_ctx context;
    uint8_t digest[CDC_DIGEST_SIZE];
    char line[1024];
    int written;
    cdc_digest_init(&context);
    written = snprintf(
        line, sizeof(line),
        "%s|%s|%s|%s|%s|%s|%s|%s|%s|%d|%s|%s|%s|%s|%s\n",
        job->id, result->frame, result->receptive_angle,
        result->radiant_angle, result->holonomy, result->half_projection,
        result->half_sheet, result->full_projection, result->full_sheet,
        result->winding, result->record_coordinate,
        result->decision_coordinate, result->enacted_coordinate,
        result->status, result->reason);
    if (written > 0 && (size_t)written < sizeof(line)) {
        cdc_digest_update(&context, line, (size_t)written);
    }
    cdc_digest_final(&context, digest);
    cdc_digest_hex(digest, hex, hex_size);
}

static void u2_json_string(const char *value) {
    const unsigned char *cursor = (const unsigned char *)(value ? value : "");
    putchar('"');
    while (*cursor) {
        if (*cursor == '"' || *cursor == '\\') {
            putchar('\\');
            putchar((int)*cursor);
        } else if (*cursor < 0x20) {
            printf("\\u%04x", (unsigned int)*cursor);
        } else {
            putchar((int)*cursor);
        }
        cursor++;
    }
    putchar('"');
}

typedef struct {
    size_t cover_coordinate;
    double displacement;
    int field_cell_count;
    int incident_channel_count;
    int mutating_step_count;
    int winding;
    int equivariance_verified;
    int section_verified;
    char projection[16];
    char sheet[16];
} U2PhaseRestoration;

static const char *u2_latch_mode_name(int has_latch, char latch) {
    if (!has_latch) {
        return "unlatched";
    }
    switch (latch) {
    case '+':
        return "latched:+";
    case '0':
        return "latched:0";
    case '-':
        return "latched:-";
    default:
        return "latched:unknown";
    }
}

static void u2_digest_discrete_state(const cdc_u2_layout *layout,
                                     const Runtime *rt, char *hex,
                                     size_t hex_size) {
    cdc_digest_ctx context;
    unsigned char digest[32];
    size_t theta_count = 0;
    if (!layout || !layout->coordinates || !rt || !hex || hex_size == 0) {
        if (hex && hex_size) {
            hex[0] = '\0';
        }
        return;
    }
    cdc_digest_init(&context);
    for (size_t i = 0; i < layout->dimension; i++) {
        const cdc_u2_coordinate *coordinate = &layout->coordinates[i];
        const Cell *cell;
        char source_index_text[32];
        if (coordinate->kind != CDC_U2_COORDINATE_THETA ||
            coordinate->source_index >= (size_t)rt->cell_count) {
            continue;
        }
        cell = &rt->cells[coordinate->source_index];
        theta_count++;
        cdc_digest_update(&context, coordinate->name, strlen(coordinate->name));
        snprintf(source_index_text, sizeof(source_index_text), "%zu",
                 coordinate->source_index);
        cdc_digest_update(&context, source_index_text,
                          strlen(source_index_text));
        cdc_digest_update(&context, cell->name, strlen(cell->name));
        cdc_digest_update(&context, cell->module, strlen(cell->module));
        cdc_digest_update(&context,
                          cell->has_latch ? "1" : "0", 1);
        cdc_digest_update(&context, &cell->latch, 1);
        cdc_digest_update(&context,
                          u2_latch_mode_name(cell->has_latch, cell->latch),
                          strlen(u2_latch_mode_name(cell->has_latch,
                                                    cell->latch)));
    }
    if (theta_count == 0) {
        cdc_digest_update(&context, "empty-discrete-state",
                          strlen("empty-discrete-state"));
    }
    cdc_digest_final(&context, digest);
    cdc_digest_hex(digest, hex, hex_size);
}

static void u2_emit_discrete_state_entries(const cdc_u2_layout *layout,
                                           const Runtime *rt) {
    int first = 1;
    if (!layout || !layout->coordinates || !rt) {
        printf("null");
        return;
    }
    putchar('[');
    for (size_t i = 0; i < layout->dimension; i++) {
        const cdc_u2_coordinate *coordinate = &layout->coordinates[i];
        const Cell *cell;
        if (coordinate->kind != CDC_U2_COORDINATE_THETA ||
            coordinate->source_index >= (size_t)rt->cell_count) {
            continue;
        }
        cell = &rt->cells[coordinate->source_index];
        if (!first) {
            putchar(',');
        }
        first = 0;
        printf("{\"cell\":");
        u2_json_string(cell->name);
        printf(",\"coordinate\":");
        u2_json_string(coordinate->name);
        printf(",\"sourceIndex\":%zu,\"hasLatch\":%s,\"latch\":",
               coordinate->source_index, cell->has_latch ? "true" : "false");
        if (cell->has_latch) {
            char latch[2] = {cell->latch, '\0'};
            u2_json_string(latch);
        } else {
            printf("null");
        }
        printf(",\"mode\":");
        u2_json_string(u2_latch_mode_name(cell->has_latch, cell->latch));
        putchar('}');
    }
    putchar(']');
}

static void u2_emit_json(const char *source_digest, OrbitJob *orbit,
                         UniversalJob *universal,
                         const UniversalResult *universal_result,
                         const Runtime *initial_runtime,
                         const Runtime *final_runtime,
                         const SpectrumJob *spectrum_job,
                         NativeU2Tangent *tangent,
                         const cdc_matrix *path_tangent,
                         int tangent_ready,
                         const cdc_u2_recurrence_result *recurrence,
                         const U2PhaseRestoration *restoration,
                         const cdc_matrix *restoration_derivative,
                         const cdc_u2_spectrum *spectrum,
                         const char *status, const char *reason) {
    char layout_hex[72] = "";
    char universal_hex[72] = "";
    char path_tangent_hex[72] = "";
    char monodromy_hex[72] = "";
    char restoration_derivative_hex[72] = "";
    char initial_discrete_hex[72] = "";
    char final_discrete_hex[72] = "";
    int monodromy_ready = tangent_ready && recurrence &&
                          recurrence->authorizes_monodromy;
    const char *analysis = strcmp(status, "accepted") == 0 || monodromy_ready
                               ? "monodromy"
                               : tangent_ready ? "tangent" : "held";
    const char *scope = recurrence->projected ? "projected" :
                        recurrence->kind == CDC_U2_RECURRENCE_RELATIVE
                            ? "relative" : "full";
    int layout_ready = tangent->layout.dimension > 0 &&
                       tangent->layout.coordinates != NULL;
    int restoration_ready =
        layout_ready && restoration && restoration_derivative &&
        restoration->equivariance_verified && restoration->section_verified &&
        restoration->cover_coordinate < tangent->layout.dimension &&
        restoration_derivative->data &&
        restoration_derivative->rows == tangent->layout.dimension &&
        restoration_derivative->cols == tangent->layout.dimension;
    if (layout_ready) {
        u2_digest_layout(&tangent->layout, layout_hex, sizeof(layout_hex));
    }
    u2_digest_universal(universal, universal_result, universal_hex,
                        sizeof(universal_hex));
    if (tangent_ready) {
        u2_digest_matrix(path_tangent, path_tangent_hex,
                         sizeof(path_tangent_hex));
    }
    if (monodromy_ready) {
        u2_digest_matrix(&tangent->tangent.matrix, monodromy_hex,
                         sizeof(monodromy_hex));
    }
    if (restoration_ready) {
        u2_digest_matrix(restoration_derivative, restoration_derivative_hex,
                         sizeof(restoration_derivative_hex));
    }
    if (tangent_ready && initial_runtime && final_runtime) {
        u2_digest_discrete_state(&tangent->layout, initial_runtime,
                                 initial_discrete_hex,
                                 sizeof(initial_discrete_hex));
        u2_digest_discrete_state(&tangent->layout, final_runtime,
                                 final_discrete_hex,
                                 sizeof(final_discrete_hex));
    }
    printf("u2-json={\"schema\":\"cdc.u2.stability.v1\",\"orbit\":");
    u2_json_string(orbit->id);
    printf(",\"status\":");
    u2_json_string(status);
    printf(",\"reason\":");
    u2_json_string(reason);
    printf(",\"analysis\":");
    u2_json_string(analysis);
    printf(",\"runtimeIdentity\":\"cdc-native-u2-v1\"");
    printf(",\"sourceDigest\":");
    u2_json_string(source_digest);
    printf(",\"sourceDigestSurface\":\"grammar-1-canonical\"");
    printf(",\"universal\":{\"id\":");
    u2_json_string(universal->id);
    printf(",\"status\":");
    u2_json_string(universal_result->status);
    printf(",\"reason\":");
    u2_json_string(universal_result->reason);
    printf(",\"resultDigest\":");
    u2_json_string(universal_hex);
    printf(",\"receptiveAngle\":");
    u2_json_string(universal_result->receptive_angle);
    printf(",\"radiantAngle\":");
    u2_json_string(universal_result->radiant_angle);
    printf(",\"holonomy\":");
    u2_json_string(universal_result->holonomy);
    printf(",\"winding\":%d}", universal_result->winding);
    printf(",\"manifestDigest\":");
    if (layout_ready) {
        u2_json_string(layout_hex);
    } else {
        printf("null");
    }
    printf(",\"methods\":{\"flow\":\"explicit-euler-map\","
           "\"commit\":\"scheduled-fixed-mode-reset\","
           "\"nest\":\"fixed-trit-overwrite\","
           "\"guard\":\"unbound-no-saltation\","
           "\"spectrum\":\"validated-real-schur\"}");
    printf(",\"methodDetail\":");
    if (!tangent_ready) {
        printf("null");
    } else {
        printf("{\"flow\":{\"localMap\":\"explicit-euler-synchronous\","
               "\"jacobian\":\"analytic-exact\",\"finiteDifferenceOracle\":null},"
               "\"commit\":{\"kind\":\"scheduled-fixed-mode-reset\","
               "\"continuousDerivative\":\"identity-within-fixed-mode\","
               "\"saltation\":\"not-applicable\"},"
               "\"nest\":{\"localMap\":\"fixed-trit-overwrite\","
               "\"jacobian\":\"analytic-exact\","
               "\"childPrior\":\"overwrite-parent-belief\"},"
               "\"guard\":{\"binding\":null,\"eventLocalization\":null,"
               "\"saltation\":null},"
               "\"spectrum\":{\"solver\":\"validated-real-schur\","
               "\"ordering\":\"sorted-multipliers\"}}");
    }
    printf(",\"tolerances\":{\"recurrenceAbsolute\":");
    u2_emit_canonical_float(orbit->absolute_tolerance);
    printf(",\"recurrenceRelative\":");
    u2_emit_canonical_float(orbit->relative_tolerance);
    printf(",\"neutral\":");
    u2_emit_canonical_float(spectrum_job->neutral_tolerance);
    printf(",\"schur\":");
    u2_emit_canonical_float(spectrum_job->schur_tolerance);
    putchar('}');
    printf(",\"finiteDifference\":");
    if (!tangent_ready) {
        printf("null");
    } else {
        printf("{\"status\":\"not-run\",\"step\":null,\"residual\":null}");
    }
    printf(",\"eventBudget\":");
    if (!tangent_ready) {
        printf("null");
    } else {
        printf("{\"status\":\"not-applicable\",\"limit\":null,\"used\":null}");
    }
    printf(",\"neutralModeRemoval\":");
    if (!tangent_ready) {
        printf("null");
    } else {
        printf("{\"status\":\"not-requested\",\"generatorDigest\":null,"
               "\"removedModes\":null}");
    }
    printf(",\"determinism\":"
           "\"source-order,row-major,canonical-float,sorted-multipliers\"");
    printf(",\"recurrence\":");
    if (!tangent_ready) {
        printf("null,");
    } else {
        printf("{\"kind\":");
        u2_json_string(cdc_u2_recurrence_kind_name(recurrence->kind));
        printf(",\"scope\":");
        u2_json_string(scope);
        printf(",\"residual\":");
        u2_emit_canonical_float(recurrence->residual);
        printf(",\"absoluteTolerance\":");
        u2_emit_canonical_float(recurrence->absolute_tolerance);
        printf(",\"relativeTolerance\":");
        u2_emit_canonical_float(recurrence->relative_tolerance);
        printf(",\"normalizedResidual\":");
        u2_emit_canonical_float(recurrence->normalized_residual);
        printf(",\"verified\":%s,\"authorizesMonodromy\":%s,"
               "\"discreteStateVerified\":%s,"
               "\"restorationDerivativeApplied\":%s,\"restoration\":",
               recurrence->verified ? "true" : "false",
               recurrence->authorizes_monodromy ? "true" : "false",
               recurrence->discrete_state_verified ? "true" : "false",
               recurrence->restoration_derivative_applied ? "true" : "false");
        if (!restoration_ready) {
            printf("null},");
        } else {
            printf("{\"action\":\"cover-phase-translation\","
                   "\"coordinateIndex\":%zu,\"coordinate\":",
                   restoration->cover_coordinate);
            u2_json_string(
                tangent->layout.coordinates[restoration->cover_coordinate].name);
            printf(",\"displacement\":");
            u2_emit_canonical_float(restoration->displacement);
            printf(",\"equivarianceWitness\":{"
                   "\"id\":\"isolated-affine-two-turn-cover\","
                   "\"verified\":%s,\"fieldGain\":0,"
                   "\"fieldCellCount\":%d,\"incidentChannelCount\":%d,"
                   "\"mutatingStepCount\":%d},\"sectionWitness\":{"
                   "\"id\":\"u1-two-turn-returned-restored\","
                   "\"verified\":%s,\"winding\":%d,\"projection\":",
                   restoration->equivariance_verified ? "true" : "false",
                   restoration->field_cell_count,
                   restoration->incident_channel_count,
                   restoration->mutating_step_count,
                   restoration->section_verified ? "true" : "false",
                   restoration->winding);
            u2_json_string(restoration->projection);
            printf(",\"sheet\":");
            u2_json_string(restoration->sheet);
            printf("},\"derivativeRows\":%zu,\"derivativeColumns\":%zu,"
                   "\"derivativeDigest\":",
                   restoration_derivative->rows,
                   restoration_derivative->cols);
            u2_json_string(restoration_derivative_hex);
            printf(",\"derivative\":[");
            for (size_t i = 0;
                 i < restoration_derivative->rows * restoration_derivative->cols;
                 i++) {
                if (i) {
                    putchar(',');
                }
                u2_emit_canonical_float(restoration_derivative->data[i]);
            }
            printf("]}},");
        }
    }
    printf("\"discreteState\":");
    if (!tangent_ready || !initial_runtime || !final_runtime) {
        printf("null,");
    } else {
        printf("{\"verified\":%s,\"initialDigest\":",
               recurrence->discrete_state_verified ? "true" : "false");
        u2_json_string(initial_discrete_hex);
        printf(",\"finalDigest\":");
        u2_json_string(final_discrete_hex);
        printf(",\"initial\":");
        u2_emit_discrete_state_entries(&tangent->layout, initial_runtime);
        printf(",\"final\":");
        u2_emit_discrete_state_entries(&tangent->layout, final_runtime);
        printf("},");
    }
    printf("\"dimension\":");
    if (layout_ready) {
        printf("%zu,\"coordinates\":[", tangent->layout.dimension);
        for (size_t i = 0; i < tangent->layout.dimension; i++) {
            if (i) {
                putchar(',');
            }
            printf("{\"name\":");
            u2_json_string(tangent->layout.coordinates[i].name);
            printf(",\"period\":");
            u2_emit_canonical_float(tangent->layout.coordinates[i].period);
            putchar('}');
        }
        printf("],\"initialState\":");
    } else {
        printf("null,\"coordinates\":null,\"initialState\":");
    }
    if (!tangent_ready) {
        printf("null,\"finalState\":null,\"pathTangentDigest\":null,"
               "\"pathTangent\":null,");
    } else {
        putchar('[');
        for (size_t i = 0; i < tangent->layout.dimension; i++) {
            if (i) {
                putchar(',');
            }
            u2_emit_canonical_float(tangent->initial[i]);
        }
        printf("],\"finalState\":[");
        for (size_t i = 0; i < tangent->layout.dimension; i++) {
            if (i) {
                putchar(',');
            }
            u2_emit_canonical_float(tangent->final[i]);
        }
        printf("],\"pathTangentDigest\":");
        u2_json_string(path_tangent_hex);
        printf(",\"pathTangent\":[");
        for (size_t i = 0; i < path_tangent->rows * path_tangent->cols; i++) {
            if (i) {
                putchar(',');
            }
            u2_emit_canonical_float(path_tangent->data[i]);
        }
        printf("],");
    }
    printf("\"events\":[");
    for (size_t i = 0; tangent_ready && i < tangent->tangent.event_count; i++) {
        cdc_u2_event *event = &tangent->tangent.events[i];
        if (i) {
            putchar(',');
        }
        printf("{\"kind\":");
        u2_json_string(cdc_u2_event_kind_name(event->kind));
        printf(",\"time\":");
        u2_emit_canonical_float(event->time);
        printf(",\"transverse\":");
        if (event->transverse < 0) {
            printf("null");
        } else {
            printf("%s", event->transverse ? "true" : "false");
        }
        putchar('}');
    }
    printf("],");
    if (!monodromy_ready) {
        printf("\"monodromyDigest\":null,\"monodromy\":null,"
               "\"multipliers\":null,"
               "\"spectrumDiagnostics\":null,"
               "\"backend\":null,\"classification\":\"held\"}");
    } else if (strcmp(status, "accepted") != 0 || !spectrum) {
        printf("\"monodromyDigest\":");
        u2_json_string(monodromy_hex);
        printf(",\"monodromy\":[");
        for (size_t i = 0; i < tangent->tangent.matrix.rows *
                               tangent->tangent.matrix.cols; i++) {
            if (i) {
                putchar(',');
            }
            u2_emit_canonical_float(tangent->tangent.matrix.data[i]);
        }
        printf("],\"multipliers\":null,"
               "\"spectrumDiagnostics\":null,"
               "\"backend\":null,\"classification\":\"held\"}");
    } else {
        printf("\"monodromyDigest\":");
        u2_json_string(monodromy_hex);
        printf(",\"monodromy\":[");
        for (size_t i = 0; i < tangent->tangent.matrix.rows *
                               tangent->tangent.matrix.cols; i++) {
            if (i) {
                putchar(',');
            }
            u2_emit_canonical_float(tangent->tangent.matrix.data[i]);
        }
        printf("],\"multipliers\":[");
        for (size_t i = 0; i < spectrum->dimension; i++) {
            if (i) {
                putchar(',');
            }
            printf("{\"real\":");
            u2_emit_canonical_float(spectrum->multipliers[i].real);
            printf(",\"imag\":");
            u2_emit_canonical_float(spectrum->multipliers[i].imag);
            printf(",\"modulus\":");
            u2_emit_canonical_float(spectrum->multipliers[i].modulus);
            printf(",\"mode\":\"%s\"}",
                   spectrum->multipliers[i].mode == CDC_U2_MULTIPLIER_GAUGE
                       ? "gauge" : "physical");
        }
        printf("],\"backend\":");
        u2_json_string(spectrum->backend);
        printf(",\"spectrumDiagnostics\":{\"spectralRadius\":");
        u2_emit_canonical_float(spectrum->spectral_radius);
        printf(",\"schur\":{\"reconstructionResidual\":");
        u2_emit_canonical_float(spectrum->schur_reconstruction_residual);
        printf(",\"orthogonalityResidual\":");
        u2_emit_canonical_float(spectrum->schur_orthogonality_residual);
        printf(",\"triangularResidual\":");
        u2_emit_canonical_float(spectrum->schur_triangular_residual);
        printf(",\"validationTolerance\":");
        u2_emit_canonical_float(spectrum_job->schur_tolerance);
        printf("}}");
        printf(",\"classification\":");
        u2_json_string(cdc_u2_stability_class_name(spectrum->classification));
        putchar('}');
    }
    putchar('\n');
}

static cdc_u2_status u2_restore_verified_cover_phase(
    const double *final_state, size_t dimension, double *restored_state,
    void *opaque) {
    U2PhaseRestoration *restoration = (U2PhaseRestoration *)opaque;
    if (!final_state || !restored_state || !restoration ||
        restoration->cover_coordinate >= dimension) {
        return CDC_U2_INVALID_ARGUMENT;
    }
    memcpy(restored_state, final_state, dimension * sizeof(double));
    restored_state[restoration->cover_coordinate] -= restoration->displacement;
    return CDC_U2_OK;
}

/* V1's executable phase quotient is intentionally narrow.  It is admitted
 * only for the verified two-turn U1 cover when that cover is an isolated
 * affine phase: one cell in a zero-gain field, no incident channels, and no
 * commit/nest mutation of its module.  Under exactly these executable maps,
 * theta -> theta + 4*pi is an equivariance and D(rho)=I. */
static int u2_cover_phase_quotient_is_executable(
    Runtime *rt, UniversalJob *universal, const UniversalResult *result,
    const cdc_u2_layout *layout,
    U2PhaseRestoration *restoration) {
    Cell *cover = find_cell(rt, universal->cover_cell);
    Module *module;
    Field *field;
    char coordinate_name[CDC_U2_COORDINATE_NAME_MAX];
    long index;
    int field_cells = 0;
    if (!restoration || !cover ||
        !(module = find_module(rt, cover->module)) ||
        !(field = find_field(rt, module->field)) || field->gain != 0.0 ||
        result->winding != 2 || strcmp(result->full_projection, "returned") != 0 ||
        strcmp(result->full_sheet, "restored") != 0) {
        return 0;
    }
    if (snprintf(coordinate_name, sizeof(coordinate_name), "%s.theta",
                 cover->name) <= 0 ||
        (index = cdc_u2_layout_find(layout, coordinate_name)) < 0) {
        return 0;
    }
    for (int i = 0; i < rt->cell_count; i++) {
        if (cell_in_field(rt, &rt->cells[i], field->name)) {
            field_cells++;
        }
    }
    if (field_cells != 1) {
        return 0;
    }
    for (int i = 0; i < rt->channel_count; i++) {
        if (strcmp(rt->channels[i].source, cover->name) == 0 ||
            strcmp(rt->channels[i].target, cover->name) == 0) {
            return 0;
        }
    }
    for (int i = 0; i < rt->step_count; i++) {
        Step *step = &rt->steps[i];
        if ((step->kind == STEP_COMMIT &&
             strcmp(step->module, module->name) == 0) ||
            (step->kind == STEP_NEST &&
             (strcmp(step->parent, module->name) == 0 ||
              strcmp(step->child, module->name) == 0))) {
            return 0;
        }
    }
    restoration->cover_coordinate = (size_t)index;
    restoration->displacement = 4.0 * PI;
    restoration->field_cell_count = field_cells;
    restoration->incident_channel_count = 0;
    restoration->mutating_step_count = 0;
    restoration->winding = result->winding;
    restoration->equivariance_verified = 1;
    restoration->section_verified = 1;
    snprintf(restoration->projection, sizeof(restoration->projection), "%s",
             result->full_projection);
    snprintf(restoration->sheet, sizeof(restoration->sheet), "%s",
             result->full_sheet);
    return 1;
}

static void run_stability(Runtime *source, const char *path) {
    (void)path;
    if (source->spectrum_count == 0) {
        fail("stability source has no spectrum job");
    }
    for (int i = 0; i < source->spectrum_count; i++) {
        SpectrumJob *spectrum_job = &source->spectra[i];
        VariationalJob *variational = find_variational_job(
            source, spectrum_job->variational);
        OrbitJob *orbit = NULL;
        UniversalJob *declared_universal;
        Runtime primal;
        Runtime initial;
        UniversalResult universal_result;
        NativeU2Tangent tangent;
        cdc_u2_recurrence_spec recurrence_spec;
        cdc_u2_recurrence_result recurrence;
        cdc_u2_spectrum spectrum;
        cdc_matrix path_tangent;
        cdc_matrix restoration_derivative;
        U2PhaseRestoration phase_restoration;
        unsigned char include[MAX_U2_DIMENSION];
        cdc_u2_status status;
        const char *reason = "none";
        int projected = 0;
        int primal_accepted;
        int tangent_ready = 0;
        memset(&tangent, 0, sizeof(tangent));
        memset(&recurrence_spec, 0, sizeof(recurrence_spec));
        memset(&recurrence, 0, sizeof(recurrence));
        memset(&spectrum, 0, sizeof(spectrum));
        memset(&path_tangent, 0, sizeof(path_tangent));
        memset(&restoration_derivative, 0, sizeof(restoration_derivative));
        memset(&phase_restoration, 0, sizeof(phase_restoration));
        if (!variational || !(orbit = find_orbit_job(source, variational->orbit)) ||
            !(declared_universal = find_universal_job(source, orbit->universal))) {
            fail("spectrum references an unknown variational, orbit, or universal job");
            continue;
        }
        /* Each analysis starts from an identical in-memory snapshot.  The
         * U1 executor below stops before run_universal_enactment, so stability
         * never writes the declared evolve output. */
        primal = *source;
        declared_universal = find_universal_job(&primal, orbit->universal);
        primal_accepted = execute_universal(&primal, declared_universal,
                                            &universal_result, &initial);
        if (!primal_accepted) {
            reason = "primal-held";
            status = CDC_U2_MODE_DIVERGENCE;
        } else {
            status = u2_execute_tangent(
                &initial, find_universal_job(&initial, orbit->universal),
                orbit->absolute_tolerance, &tangent);
        }
        if (status == CDC_U2_OK) {
            if (cdc_matrix_init(&path_tangent, tangent.layout.dimension,
                                tangent.layout.dimension) != CDC_LINALG_OK ||
                cdc_matrix_copy(&path_tangent, &tangent.tangent.matrix) !=
                    CDC_LINALG_OK) {
                status = CDC_U2_ALLOCATION_FAILED;
                reason = "allocation-failed";
            } else {
                tangent_ready = 1;
            }
        }
        if (!primal_accepted) {
            /* Preserve the typed U1 failure. No tangent may be derived from a
             * snapshot the Universal executor never admitted. */
        } else if (status != CDC_U2_OK) {
            if (strcmp(reason, "none") == 0) {
                reason = tangent.reason;
            }
        } else if (!u2_coordinate_mask(&initial, &tangent.layout, orbit,
                                       include, &projected)) {
            fail("orbit coordinates reference an unknown state coordinate");
        } else {
            int quotient_ready = 1;
            recurrence_spec.kind = CDC_U2_RECURRENCE_FULL;
            recurrence_spec.absolute_tolerance = orbit->absolute_tolerance;
            recurrence_spec.relative_tolerance = orbit->relative_tolerance;
            recurrence_spec.include = include;
            recurrence_spec.discrete_state_verified =
                u2_discrete_state_equal(&tangent.layout, &initial, &primal);
            if (strcmp(orbit->quotient, "phase") == 0 && !projected &&
                u2_cover_phase_quotient_is_executable(
                    &initial, find_universal_job(&initial, orbit->universal),
                    &universal_result, &tangent.layout, &phase_restoration) &&
                cdc_matrix_init(&restoration_derivative,
                                tangent.layout.dimension,
                                tangent.layout.dimension) == CDC_LINALG_OK &&
                cdc_matrix_identity(&restoration_derivative) == CDC_LINALG_OK) {
                recurrence_spec.kind = CDC_U2_RECURRENCE_RELATIVE;
                recurrence_spec.restore_endpoint =
                    u2_restore_verified_cover_phase;
                recurrence_spec.restoration_context = &phase_restoration;
                recurrence_spec.restoration_equivariant = 1;
                recurrence_spec.restoration_derivative =
                    &restoration_derivative;
            } else if (strcmp(orbit->quotient, "none") != 0) {
                quotient_ready = 0;
                recurrence.kind = CDC_U2_RECURRENCE_RELATIVE;
                recurrence.projected = projected;
                recurrence.absolute_tolerance = orbit->absolute_tolerance;
                recurrence.relative_tolerance = orbit->relative_tolerance;
                reason = "undeclared-quotient";
            }
            if (quotient_ready) {
                status = cdc_u2_recurrence_check(
                    &tangent.layout, tangent.initial, tangent.final,
                    tangent.layout.dimension, &recurrence_spec, &recurrence);
                if (projected) {
                    reason = "undeclared-quotient";
                } else if (status != CDC_U2_OK) {
                    reason = cdc_u2_status_reason(status);
                } else {
                    status = cdc_u2_monodromy_bind_recurrence(
                        &tangent.tangent, &recurrence_spec, &recurrence,
                        tangent.endpoint_time);
                    if (status != CDC_U2_OK) {
                        reason = cdc_u2_status_reason(status);
                    } else {
                        status = cdc_u2_spectrum_compute(
                            &tangent.tangent.matrix, NULL, &recurrence, NULL,
                            spectrum_job->neutral_tolerance,
                            spectrum_job->schur_tolerance, &spectrum);
                        if (status != CDC_U2_OK) {
                            reason = cdc_u2_status_reason(status);
                        }
                    }
                }
            }
        }
        u2_emit_json(source->source_digest, orbit, declared_universal,
                     &universal_result, &initial, &primal,
                     spectrum_job, &tangent, &path_tangent, tangent_ready,
                     &recurrence,
                     phase_restoration.equivariance_verified
                         ? &phase_restoration
                         : NULL,
                     restoration_derivative.data ? &restoration_derivative
                                                 : NULL,
                     strcmp(reason, "none") == 0 ? &spectrum : NULL,
                     strcmp(reason, "none") == 0 ? "accepted" : "held",
                     reason);
        cdc_u2_spectrum_release(&spectrum);
        cdc_matrix_release(&path_tangent);
        cdc_matrix_release(&restoration_derivative);
        u2_tangent_release(&tangent);
    }
}

static void collect_replay_data(const char *reducer_path, const char *surface_path, ReplayData *data) {
    Runtime reducer;
    Runtime surface;
    int has_flow = 0;
    int has_accepted = 0;
    int has_held = 0;
    int has_nest = 0;
    TraceJob *trace;
    SurfaceBridgeJob *bridge;
    char trits[128];
    char dyadic[7];
    char triadic[4];
    int index;

    memset(data, 0, sizeof(*data));
    parse_source(&reducer, reducer_path);
    if (reducer.channel_count == 0) {
        fail("replay reducer source has no channel");
    }
    snprintf(data->channel_weight, sizeof(data->channel_weight), "%g", reducer.channels[0].weight);
    for (int i = 0; i < reducer.step_count; i++) {
        Step *step = &reducer.steps[i];
        if (step->kind == STEP_FLOW) {
            FlowResult flow;
            execute_flow(&reducer, step, &flow);
            if (!has_flow && flow.has_theta) {
                snprintf(data->theta_raw, sizeof(data->theta_raw), "%.6f", flow.theta);
                snprintf(data->theta_display, sizeof(data->theta_display), "%g", flow.theta);
                has_flow = 1;
            }
        } else if (step->kind == STEP_COMMIT) {
            CommitResult commit;
            execute_commit(&reducer, step, &commit);
            if (!has_accepted && strcmp(commit.status, "accepted") == 0) {
                data->accepted = commit;
                has_accepted = 1;
            } else if (!has_held && strcmp(commit.status, "held") == 0) {
                data->held = commit;
                has_held = 1;
            }
        } else if (step->kind == STEP_NEST) {
            NestResult nest;
            execute_nest(&reducer, step, &nest);
            if (!has_nest) {
                data->nest = nest;
                has_nest = 1;
            }
        }
    }
    if (!has_flow || !has_accepted || !has_held || !has_nest) {
        fail("replay reducer source missing flow, accepted commit, held commit, or nest result");
    }

    parse_source(&surface, surface_path);
    if (surface.trace_count == 0 || surface.bridge_count == 0) {
        fail("replay surface source missing trace or bridge");
    }
    trace = &surface.traces[0];
    trace_trits(&surface, trace, trits, sizeof(trits));
    data->trace_events = count_occupied_trits(trits);
    if (trace->expect_trits[0]) {
        cdc_expect_string(trits, trace->expect_trits, "replay trace trit expectation mismatch");
    }
    if (trace->expect_events >= 0) {
        cdc_expect_int(data->trace_events, trace->expect_events, "replay trace event-count expectation mismatch");
    }
    snprintf(data->trace_trits, sizeof(data->trace_trits), "%s", trits);

    bridge = &surface.bridges[0];
    trace = find_trace(&surface, bridge->trace);
    if (!trace) {
        fail("replay surface bridge references unknown trace");
    }
    if (bridge->via[0] && strcmp(bridge->via, "bridge64") != 0) {
        fail("replay surface bridge currently supports bridge64");
    }
    trace_trits(&surface, trace, trits, sizeof(trits));
    trits_to_occupancy6(trits, dyadic);
    index = dyadic6_to_index(dyadic);
    triadic_from_index64(index, triadic);
    if (bridge->expect_dyadic[0]) {
        cdc_expect_string(dyadic, bridge->expect_dyadic, "replay bridge dyadic expectation mismatch");
    }
    if (bridge->expect_triadic[0]) {
        cdc_expect_string(triadic, bridge->expect_triadic, "replay bridge triadic expectation mismatch");
    }
    snprintf(data->bridge_dyadic, sizeof(data->bridge_dyadic), "%s", dyadic);
    snprintf(data->bridge_triadic, sizeof(data->bridge_triadic), "%s", triadic);
    data->bridge_index = index;
}

int cdc_native_replay_json(const char *reducer_path, const char *surface_path,
                           const char *universal_path, char *out, size_t out_size) {
    ReplayData data;
    UniversalResult universal;
    int has_universal = 0;
    int written;
    int appended;
    if (!out || out_size == 0) {
        return -1;
    }
    collect_replay_data(reducer_path, surface_path, &data);
    if (universal_path && universal_path[0]) {
        Runtime universal_runtime;
        parse_source(&universal_runtime, universal_path);
        if (universal_runtime.universal_count == 0) {
            fail("replay universal source has no universal job");
        }
        if (execute_universal(&universal_runtime, &universal_runtime.universals[0], &universal, NULL)) {
            snprintf(universal.enacted_coordinate, sizeof(universal.enacted_coordinate),
                     "%s", universal.record_coordinate);
        }
        has_universal = 1;
    }
    written = snprintf(
        out,
        out_size,
        "{\n"
        "  \"flow\": {\n"
        "    \"thetaCouncilB\": \"%s\",\n"
        "    \"thetaCouncilBRaw\": \"%s\",\n"
        "    \"channelWeight\": \"%s\"\n"
        "  },\n"
        "  \"commit\": {\n"
        "    \"trits\": \"%s\",\n"
        "    \"balance\": \"%s\",\n"
        "    \"status\": \"%s\",\n"
        "    \"reason\": \"%s\"\n"
        "  },\n"
        "  \"hold\": {\n"
        "    \"trits\": \"%s\",\n"
        "    \"balance\": \"%s\",\n"
        "    \"status\": \"%s\",\n"
        "    \"reason\": \"%s\"\n"
        "  },\n"
        "  \"nest\": {\n"
        "    \"up\": \"%.6f\",\n"
        "    \"parentBelief\": \"%.6f\",\n"
        "    \"childPrior\": \"%.6f\"\n"
        "  },\n"
        "  \"trace\": {\n"
        "    \"trits\": \"%s\",\n"
        "    \"events\": \"%d\"\n"
        "  },\n"
        "  \"bridge\": {\n"
        "    \"dyadic\": \"%s\",\n"
        "    \"triadic\": \"%s\",\n"
        "    \"index\": \"%d\"\n"
        "  }",
        data.theta_display,
        data.theta_raw,
        data.channel_weight,
        data.accepted.trits,
        data.accepted.balance,
        data.accepted.status,
        data.accepted.reason,
        data.held.trits,
        data.held.balance,
        data.held.status,
        data.held.reason,
        data.nest.up,
        data.nest.parent_belief,
        data.nest.child_prior,
        data.trace_trits,
        data.trace_events,
        data.bridge_dyadic,
        data.bridge_triadic,
        data.bridge_index);
    if (written < 0 || (size_t)written >= out_size) {
        return -1;
    }
    if (has_universal) {
        appended = snprintf(
            out + written,
            out_size - (size_t)written,
            ",\n"
            "  \"universal\": {\n"
            "    \"operator\": \"U720\",\n"
            "    \"frame\": \"%s\",\n"
            "    \"receptiveAngle\": \"%s\",\n"
            "    \"radiantAngle\": \"%s\",\n"
            "    \"holonomy\": \"%s\",\n"
            "    \"halfProjection\": \"%s\",\n"
            "    \"halfSheet\": \"%s\",\n"
            "    \"fullProjection\": \"%s\",\n"
            "    \"fullSheet\": \"%s\",\n"
            "    \"recordCoordinate\": \"%s\",\n"
            "    \"decisionCoordinate\": \"%s\",\n"
            "    \"enactedCoordinate\": \"%s\",\n"
            "    \"status\": \"%s\",\n"
            "    \"reason\": \"%s\"\n"
            "  }",
            universal.frame,
            universal.receptive_angle,
            universal.radiant_angle,
            universal.holonomy,
            universal.half_projection,
            universal.half_sheet,
            universal.full_projection,
            universal.full_sheet,
            universal.record_coordinate,
            universal.decision_coordinate,
            universal.enacted_coordinate,
            universal.status,
            universal.reason);
        if (appended < 0 || (size_t)(written + appended) >= out_size) {
            return -1;
        }
        written += appended;
    }
    appended = snprintf(out + written, out_size - (size_t)written, "\n}");
    if (appended < 0 || (size_t)(written + appended) >= out_size) {
        return -1;
    }
    return written + appended;
}

static void run_replay(const char *reducer_path, const char *surface_path, const char *universal_path) {
    char json[6144];
    if (cdc_native_replay_json(reducer_path, surface_path, universal_path, json, sizeof(json)) < 0) {
        fail("replay JSON buffer too small");
    }
    printf("%s\n", json);
}

static void usage(void) {
    fprintf(stderr, "usage:\n");
    fprintf(stderr, "  cdc_native_runtime run native_reducer.cdc\n");
    fprintf(stderr, "  cdc_native_runtime compile native_reducer.cdc\n");
    fprintf(stderr, "  cdc_native_runtime interpret native_reducer.cdc\n");
    fprintf(stderr, "  cdc_native_runtime prove native_reducer.cdc\n");
    fprintf(stderr, "  cdc_native_runtime surface native_surface.cdc\n");
    fprintf(stderr, "  cdc_native_runtime council council_bridge.cdc\n");
    fprintf(stderr, "  cdc_native_runtime evolve council_bridge.cdc\n");
    fprintf(stderr, "  cdc_native_runtime universal framework_loop.cdc\n");
    fprintf(stderr, "  cdc_native_runtime stability u2_analysis.cdc\n");
    fprintf(stderr, "  cdc_native_runtime persist framework_persistence.cdc\n");
    fprintf(stderr, "  cdc_native_runtime replay native_reducer.cdc native_surface.cdc [framework_loop.cdc]\n");
    exit(2);
}

/* Fused single-process executor (gate CT3): one parse, one live Runtime,
 * every stage family the source declares, in kernel order. This discharges
 * the queued "single-process executor fusing all modes" obligation:
 * - a source with a universal job takes the universal closure path, which
 *   already chains reduce -> surface -> council -> closure -> enactment
 *   over one live state with interpreter parity;
 * - any other source runs its reducer chain first (only when all three
 *   foundational kinds are present, mirroring run mode), and the surface,
 *   council, and evolution stages then operate on the MUTATED state.
 * cycles=N re-execution stays queued: inline expect-* values pin
 * first-cycle state, so unbounded iteration needs per-cycle expectation
 * families (see BUILD_STATE honest boundaries). */
static void run_fused(Runtime *rt, const char *path) {
    int stages = 0;
    if (rt->universal_count > 0) {
        run_universal(rt, path);
        printf("cdc fused ok stages=1 mode=universal source=%s\n", path);
        return;
    }
    if (rt->step_count > 0) {
        int flow_count, commit_count, nest_count;
        count_step_kinds(rt, &flow_count, &commit_count, &nest_count);
        if (flow_count == 0 || commit_count == 0 || nest_count == 0) {
            /* Review B5: a declared-but-incomplete reducer family is a
             * typed error, never silently skipped. */
            fail("fused run: reducer family incomplete (declares steps "
                 "but not all of flow, commit, nest)");
        }
        run_steps(rt, path);
        stages++;
    }
    if (rt->guard_count + rt->trace_count + rt->measure_count +
            rt->policy_count + rt->bridge_count + rt->counter_count >
        0) {
        run_surface(rt, path);
        stages++;
    }
    if (rt->deliberation_count > 0) {
        run_council(rt, path);
        stages++;
    }
    if (rt->evolution_count > 0) {
        run_evolution(rt, path);
        stages++;
    }
    if (rt->persist_count > 0) {
        /* Persistence runs last: it records the state the earlier stages
         * left behind, through the same barrier that produced it. */
        run_persistence(rt, path);
        stages++;
    }
    if (stages == 0) {
        fail("fused run: source declares no executable stage");
    }
    printf("cdc fused ok stages=%d source=%s\n", stages, path);
}

/* With CDC_NATIVE_NO_MAIN the same entry point compiles as
 * cdc_native_main so the unified cdc driver can pass legacy verbs through
 * with byte-identical behavior (gate CT2). */
#ifdef CDC_NATIVE_NO_MAIN
#define CDC_NATIVE_ENTRY cdc_native_main
int cdc_native_main(int argc, char **argv);
#else
#define CDC_NATIVE_ENTRY main
#endif
int CDC_NATIVE_ENTRY(int argc, char **argv) {
    Runtime runtime;
    /* Open the receipt stream eagerly. A mode with no effects must still
     * produce an EMPTY stream rather than no stream, so a consumer can tell
     * "this run had nothing to report" apart from "this runtime does not
     * emit receipts at all". The second is a contract violation and must be
     * detectable. */
    (void)receipts();
    if ((argc == 4 || argc == 5) && strcmp(argv[1], "replay") == 0) {
        run_replay(argv[2], argv[3], argc == 5 ? argv[4] : NULL);
        return 0;
    }
    if (argc != 3) {
        usage();
    }
    parse_source(&runtime, argv[2]);
    if (strcmp(argv[1], "run") == 0) {
        run_steps(&runtime, argv[2]);
    } else if (strcmp(argv[1], "compile") == 0) {
        compile_source(&runtime, argv[2]);
    } else if (strcmp(argv[1], "interpret") == 0) {
        interpret_source(&runtime, argv[2]);
    } else if (strcmp(argv[1], "prove") == 0) {
        prove_source(&runtime, argv[2]);
    } else if (strcmp(argv[1], "surface") == 0) {
        run_surface(&runtime, argv[2]);
    } else if (strcmp(argv[1], "council") == 0) {
        run_council(&runtime, argv[2]);
    } else if (strcmp(argv[1], "evolve") == 0) {
        run_evolution(&runtime, argv[2]);
    } else if (strcmp(argv[1], "universal") == 0) {
        run_universal(&runtime, argv[2]);
    } else if (strcmp(argv[1], "stability") == 0) {
        run_stability(&runtime, argv[2]);
    } else if (strcmp(argv[1], "persist") == 0) {
        run_persistence(&runtime, argv[2]);
    } else if (strcmp(argv[1], "fused") == 0) {
        run_fused(&runtime, argv[2]);
    } else {
        usage();
    }
    return 0;
}
