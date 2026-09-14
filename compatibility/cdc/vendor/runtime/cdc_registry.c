#define _POSIX_C_SOURCE 200809L

#include "cdc_registry.h"

#include "cdc_receipt.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ---- small dynamic containers -------------------------------------- */

typedef struct {
    char **items;
    size_t count;
    size_t cap;
} strlist;

static int strlist_push(strlist *list, const char *value) {
    char *copy;
    if (list->count == list->cap) {
        size_t next = list->cap ? list->cap * 2 : 16;
        void *grown = cdc_frontend_alloc(list->items, next * sizeof(char *));
        if (!grown) {
            return 0;
        }
        list->items = grown;
        list->cap = next;
    }
    copy = cdc_frontend_alloc(NULL, strlen(value) + 1);
    if (!copy) {
        return 0;
    }
    memcpy(copy, value, strlen(value) + 1);
    list->items[list->count++] = copy;
    return 1;
}

static int strlist_contains(const strlist *list, const char *value) {
    size_t i;
    for (i = 0; i < list->count; i++) {
        if (strcmp(list->items[i], value) == 0) {
            return 1;
        }
    }
    return 0;
}

static int strlist_add_unique(strlist *list, const char *value) {
    if (strlist_contains(list, value)) {
        return 1;
    }
    return strlist_push(list, value);
}

static void strlist_free(strlist *list) {
    size_t i;
    for (i = 0; i < list->count; i++) {
        cdc_frontend_alloc(list->items[i], 0);
    }
    cdc_frontend_alloc(list->items, 0);
    memset(list, 0, sizeof(*list));
}

typedef struct {
    char *key;
    const cdc_stmt *stmt;
} keyed;

typedef struct {
    keyed *items;
    size_t count;
    size_t cap;
} keyedlist;

static keyed *keyedlist_find(keyedlist *list, const char *key) {
    size_t i;
    for (i = 0; i < list->count; i++) {
        if (strcmp(list->items[i].key, key) == 0) {
            return &list->items[i];
        }
    }
    return NULL;
}

static int keyedlist_put(keyedlist *list, const char *key,
                         const cdc_stmt *stmt, int overwrite) {
    keyed *found = keyedlist_find(list, key);
    if (found) {
        if (overwrite) {
            found->stmt = stmt;
        }
        return 1;
    }
    if (list->count == list->cap) {
        size_t next = list->cap ? list->cap * 2 : 16;
        void *grown = cdc_frontend_alloc(list->items, next * sizeof(keyed));
        if (!grown) {
            return 0;
        }
        list->items = grown;
        list->cap = next;
    }
    list->items[list->count].key = cdc_frontend_alloc(NULL, strlen(key) + 1);
    if (!list->items[list->count].key) {
        return 0;
    }
    memcpy(list->items[list->count].key, key, strlen(key) + 1);
    list->items[list->count].stmt = stmt;
    list->count++;
    return 1;
}

static void keyedlist_free(keyedlist *list) {
    size_t i;
    for (i = 0; i < list->count; i++) {
        cdc_frontend_alloc(list->items[i].key, 0);
    }
    cdc_frontend_alloc(list->items, 0);
    memset(list, 0, sizeof(*list));
}

/* ---- registry ------------------------------------------------------- */

static const char *const FORM_NAMES[] = {
    "field",   "module",  "cell",    "channel", "guard",
    "counter", "flow",    "commit",  "nest",    "trace",
    "measure", "policy",  "bridge",  "compile", "interpret",
    "proof",   "council", "deliberate", "evolve", "universal",
    "orbit",   "variational", "spectrum",
    "store",   "persist", "frame",   "reduce",  "complex",
    "topology", "authority", "transport",
};
enum { FORM_NAME_COUNT = sizeof(FORM_NAMES) / sizeof(FORM_NAMES[0]) };

typedef struct {
    char *source; /* "file.cdc:line" */
    const cdc_stmt *stmt;
} expectation;

struct cdc_registry {
    const cdc_stmt *kernel;
    strlist terms, rules, provides, bootsteps;
    strlist invariants;   /* unique keys */
    strlist capabilities; /* unique keys */
    keyedlist frameworks; /* duplicate = load error */
    keyedlist witnesses;  /* overwrite on re-declare */
    strlist form_ids[FORM_NAME_COUNT];
    expectation *expects;
    size_t expect_count, expect_cap;
};

cdc_registry *cdc_registry_create(void) {
    cdc_registry *registry = cdc_frontend_alloc(NULL, sizeof(*registry));
    if (registry) {
        memset(registry, 0, sizeof(*registry));
    }
    return registry;
}

void cdc_registry_destroy(cdc_registry *registry) {
    size_t i;
    if (!registry) {
        return;
    }
    strlist_free(&registry->terms);
    strlist_free(&registry->rules);
    strlist_free(&registry->provides);
    strlist_free(&registry->bootsteps);
    strlist_free(&registry->invariants);
    strlist_free(&registry->capabilities);
    keyedlist_free(&registry->frameworks);
    keyedlist_free(&registry->witnesses);
    for (i = 0; i < FORM_NAME_COUNT; i++) {
        strlist_free(&registry->form_ids[i]);
    }
    for (i = 0; i < registry->expect_count; i++) {
        cdc_frontend_alloc(registry->expects[i].source, 0);
    }
    cdc_frontend_alloc(registry->expects, 0);
    cdc_frontend_alloc(registry, 0);
}

static const char *unit_basename(const cdc_unit *unit) {
    const char *slash = unit->file ? strrchr(unit->file, '/') : NULL;
    if (slash) {
        return slash + 1;
    }
    return unit->file ? unit->file : "<unit>";
}

static int form_index(const char *directive) {
    int i;
    for (i = 0; i < FORM_NAME_COUNT; i++) {
        if (strcmp(directive, FORM_NAMES[i]) == 0) {
            return i;
        }
    }
    return -1;
}

static int push_expectation(cdc_registry *registry, const char *source,
                            const cdc_stmt *stmt) {
    if (registry->expect_count == registry->expect_cap) {
        size_t next = registry->expect_cap ? registry->expect_cap * 2 : 64;
        void *grown = cdc_frontend_alloc(registry->expects,
                                         next * sizeof(expectation));
        if (!grown) {
            return 0;
        }
        registry->expects = grown;
        registry->expect_cap = next;
    }
    registry->expects[registry->expect_count].source =
        cdc_frontend_alloc(NULL, strlen(source) + 1);
    if (!registry->expects[registry->expect_count].source) {
        return 0;
    }
    memcpy(registry->expects[registry->expect_count].source, source,
           strlen(source) + 1);
    registry->expects[registry->expect_count].stmt = stmt;
    registry->expect_count++;
    return 1;
}

int cdc_registry_load(cdc_registry *registry, const cdc_unit *unit,
                      cdc_diag_list *diags) {
    size_t s, t;
    char source[512];
    const char *base = unit_basename(unit);

    for (s = 0; s < unit->count; s++) {
        const cdc_stmt *stmt = &unit->stmts[s];
        const char *directive = cdc_stmt_directive(stmt);
        snprintf(source, sizeof(source), "%s:%d", base, stmt->line);

        switch (stmt->kind) {
        case CDC_STMT_KERNEL:
            registry->kernel = stmt;
            break;
        case CDC_STMT_TERM:
        case CDC_STMT_RULE:
        case CDC_STMT_PROVIDES:
        case CDC_STMT_BOOTLOADER: {
            strlist *target =
                stmt->kind == CDC_STMT_TERM
                    ? &registry->terms
                    : stmt->kind == CDC_STMT_RULE
                          ? &registry->rules
                          : stmt->kind == CDC_STMT_PROVIDES
                                ? &registry->provides
                                : &registry->bootsteps;
            for (t = 0; t < cdc_stmt_arg_count(stmt); t++) {
                if (!strlist_add_unique(target, cdc_stmt_arg(stmt, t))) {
                    return 0;
                }
            }
            break;
        }
        case CDC_STMT_INVARIANT:
            if (!strlist_add_unique(&registry->invariants,
                                    cdc_stmt_arg(stmt, 0))) {
                return 0;
            }
            break;
        case CDC_STMT_CAPABILITY:
            if (!strlist_add_unique(&registry->capabilities,
                                    cdc_stmt_arg(stmt, 0))) {
                return 0;
            }
            break;
        case CDC_STMT_FRAMEWORK: {
            const char *key = cdc_stmt_arg(stmt, 0);
            if (keyedlist_find(&registry->frameworks, key)) {
                cdc_span span = {unit->file, stmt->line, 1, 0};
                cdc_diag_add(diags, CDC_DIAG_ERROR, "CDC023", span,
                             "duplicate framework '%s' across load", key);
                return 0;
            }
            if (!keyedlist_put(&registry->frameworks, key, stmt, 0)) {
                return 0;
            }
            break;
        }
        case CDC_STMT_WITNESS:
            if (!keyedlist_put(&registry->witnesses, cdc_stmt_arg(stmt, 0),
                               stmt, 1)) {
                return 0;
            }
            break;
        case CDC_STMT_FORM: {
            int index = form_index(directive);
            if (index >= 0 &&
                !strlist_add_unique(&registry->form_ids[index],
                                    cdc_stmt_arg(stmt, 0))) {
                return 0;
            }
            break;
        }
        case CDC_STMT_EXPECT:
            if (!push_expectation(registry, source, stmt)) {
                return 0;
            }
            break;
        default:
            break;
        }
    }
    return 1;
}

/* ---- python-file scan (python-files / bootloader expectations) ------ */

static int compare_names(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static int python_files_in(const char *root, strlist *out) {
    DIR *dir = opendir(root);
    struct dirent *entry;
    if (!dir) {
        return 1; /* no directory -> no python files */
    }
    while ((entry = readdir(dir)) != NULL) {
        size_t n = strlen(entry->d_name);
        char full[1024];
        struct stat st;
        if (n < 3 || strcmp(entry->d_name + n - 3, ".py") != 0) {
            continue;
        }
        snprintf(full, sizeof(full), "%s/%s", root, entry->d_name);
        if (stat(full, &st) != 0 || !S_ISREG(st.st_mode)) {
            continue;
        }
        if (!strlist_push(out, entry->d_name)) {
            closedir(dir);
            return 0;
        }
    }
    closedir(dir);
    qsort(out->items, out->count, sizeof(char *), compare_names);
    return 1;
}

/* ---- label construction --------------------------------------------- */

/* Python list-repr of strings: ['a', 'b'] / [] */
static void repr_list(FILE *mem, char *const *items, size_t count) {
    size_t i;
    fputc('[', mem);
    for (i = 0; i < count; i++) {
        if (i > 0) {
            fputs(", ", mem);
        }
        fprintf(mem, "'%s'", items[i]);
    }
    fputc(']', mem);
}

static const char *stmt_token(const cdc_stmt *stmt, size_t index) {
    if (index < stmt->token_count) {
        return stmt->tokens[index].text;
    }
    return NULL;
}

/* expectation args = tokens after "expect", verbatim (the bootloader
 * stores `rest`, not the args/attrs split). */
static size_t expect_argc(const cdc_stmt *stmt) {
    return stmt->token_count > 0 ? stmt->token_count - 1 : 0;
}
static const char *expect_arg(const cdc_stmt *stmt, size_t index) {
    return stmt_token(stmt, index + 1);
}

static int parse_int(const char *text, long *out) {
    char *end;
    long value;
    if (!text || !*text) {
        return 0;
    }
    value = strtol(text, &end, 10);
    if (*end != '\0') {
        return 0;
    }
    *out = value;
    return 1;
}

static int compare_counts(long got, const char *op, long want) {
    if (strcmp(op, "==") == 0) {
        return got == want;
    }
    if (strcmp(op, ">=") == 0) {
        return got >= want;
    }
    if (strcmp(op, "<=") == 0) {
        return got <= want;
    }
    return 0;
}

/* witness link forms, in the bootloader's declaration order */
typedef struct {
    const char *link;
    const char *forms[3];
} link_form;
static const link_form LINK_FORMS[] = {
    {"reducer", {"flow", "commit", "nest"}},
    {"guard", {"guard", NULL, NULL}},
    {"trace", {"trace", NULL, NULL}},
    {"measure", {"measure", NULL, NULL}},
    {"policy", {"policy", NULL, NULL}},
    {"bridge", {"bridge", NULL, NULL}},
    {"counter", {"counter", NULL, NULL}},
    {"compile", {"compile", NULL, NULL}},
    {"interpret", {"interpret", NULL, NULL}},
    {"council", {"deliberate", NULL, NULL}},
    {"evolution", {"evolve", NULL, NULL}},
    {"universal", {"universal", NULL, NULL}},
    {"spectrum", {"spectrum", NULL, NULL}},
    {"store", {"store", NULL, NULL}},
    {"persistence", {"persist", NULL, NULL}},
};
enum { LINK_FORM_COUNT = sizeof(LINK_FORMS) / sizeof(LINK_FORMS[0]) };

static const char *form_primitive(const char *form) {
    if (strcmp(form, "deliberate") == 0) {
        return "council";
    }
    return form; /* evolve maps to evolve; everything else is itself */
}

static int registry_has_form_id(cdc_registry *registry, const char *form,
                                const char *id) {
    int index = form_index(form);
    if (index < 0) {
        return 0;
    }
    return strlist_contains(&registry->form_ids[index], id);
}

/* resolve_witness_link: primitive out (NULL on failure), detail written to
 * mem. Returns 1 when resolved. */
static int resolve_witness_link(cdc_registry *registry, const cdc_stmt *stmt,
                                const char **primitive_out, FILE *mem) {
    const char *present[LINK_FORM_COUNT];
    size_t present_count = 0;
    size_t i, f;

    for (i = 0; i < LINK_FORM_COUNT; i++) {
        if (cdc_stmt_attr(stmt, LINK_FORMS[i].link)) {
            present[present_count++] = LINK_FORMS[i].link;
        }
    }
    if (present_count != 1) {
        fputs("needs exactly one executable link (got ", mem);
        if (present_count == 0) {
            fputs("none", mem);
        } else {
            fputc('[', mem);
            for (i = 0; i < present_count; i++) {
                if (i > 0) {
                    fputs(", ", mem);
                }
                fprintf(mem, "'%s'", present[i]);
            }
            fputc(']', mem);
        }
        fputc(')', mem);
        *primitive_out = NULL;
        return 0;
    }
    for (i = 0; i < LINK_FORM_COUNT; i++) {
        if (strcmp(LINK_FORMS[i].link, present[0]) == 0) {
            const char *job = cdc_stmt_attr(stmt, present[0]);
            for (f = 0; f < 3 && LINK_FORMS[i].forms[f]; f++) {
                if (registry_has_form_id(registry, LINK_FORMS[i].forms[f],
                                         job)) {
                    *primitive_out = form_primitive(LINK_FORMS[i].forms[f]);
                    fprintf(mem, "%s=%s", present[0], job);
                    return 1;
                }
            }
            fprintf(mem, "%s=%s has no declared job", present[0], job);
            *primitive_out = NULL;
            return 0;
        }
    }
    *primitive_out = NULL;
    return 0;
}

static int compare_keyed(const void *a, const void *b) {
    return strcmp(((const keyed *)a)->key, ((const keyed *)b)->key);
}

/* check_framework_complete mirror; writes the label to mem, returns pass */
static int framework_complete(cdc_registry *registry, const char *key,
                              FILE *mem) {
    keyed *framework = keyedlist_find(&registry->frameworks, key);
    const char *label;
    const char *requires_text;
    const char *permits_text;
    strlist required = {0}, permitted = {0}, bound_roles = {0},
            bound_wids = {0};
    keyed *sorted = NULL;
    size_t i;
    int pass = 0;
    FILE *problems_mem = NULL;
    char *problems_buf = NULL;
    size_t problems_len = 0;
    int problem_count = 0;

    if (!framework) {
        fprintf(mem, "framework %s complete (missing framework declaration)",
                key);
        return 0;
    }
    label = cdc_stmt_attr(framework->stmt, "label");
    if (!label || !*label) {
        fprintf(mem, "framework %s complete (missing label)", key);
        return 0;
    }
    requires_text = cdc_stmt_attr(framework->stmt, "requires");
    permits_text = cdc_stmt_attr(framework->stmt, "permits");
    {
        const char *texts[2];
        strlist *targets[2];
        int t;
        texts[0] = requires_text ? requires_text : "";
        texts[1] = permits_text ? permits_text : "";
        targets[0] = &required;
        targets[1] = &permitted;
        for (t = 0; t < 2; t++) {
            const char *p = texts[t];
            while (*p) {
                const char *comma = strchr(p, ',');
                size_t n = comma ? (size_t)(comma - p) : strlen(p);
                if (n > 0) {
                    char piece[128];
                    if (n >= sizeof(piece)) {
                        n = sizeof(piece) - 1;
                    }
                    memcpy(piece, p, n);
                    piece[n] = '\0';
                    if (!strlist_push(targets[t], piece)) {
                        goto done;
                    }
                }
                p = comma ? comma + 1 : p + strlen(p);
            }
        }
    }
    if (required.count == 0 || permitted.count == 0) {
        fprintf(mem,
                "framework %s complete (missing requires/permits contract)",
                key);
        goto done;
    }

    problems_mem = open_memstream(&problems_buf, &problems_len);
    if (!problems_mem) {
        goto done;
    }
    sorted = cdc_frontend_alloc(NULL,
                                registry->witnesses.count * sizeof(keyed));
    if (!sorted && registry->witnesses.count > 0) {
        goto done;
    }
    memcpy(sorted, registry->witnesses.items,
           registry->witnesses.count * sizeof(keyed));
    qsort(sorted, registry->witnesses.count, sizeof(keyed), compare_keyed);

#define ADD_PROBLEM_PREFIX()                                                \
    do {                                                                    \
        if (problem_count > 0) {                                            \
            fputs("; ", problems_mem);                                      \
        }                                                                   \
        problem_count++;                                                    \
    } while (0)

    for (i = 0; i < registry->witnesses.count; i++) {
        const char *wid = sorted[i].key;
        const cdc_stmt *stmt = sorted[i].stmt;
        const char *fw_attr = cdc_stmt_attr(stmt, "framework");
        const char *role;
        const char *primitive = NULL;
        if (!fw_attr || strcmp(fw_attr, label) != 0) {
            continue;
        }
        role = cdc_stmt_attr(stmt, "role");
        if (!role || !*role) {
            ADD_PROBLEM_PREFIX();
            fprintf(problems_mem, "%s missing role", wid);
            continue;
        }
        if (!strlist_contains(&required, role)) {
            ADD_PROBLEM_PREFIX();
            fprintf(problems_mem, "%s carries unknown role %s", wid, role);
            continue;
        }
        if (strlist_contains(&bound_roles, role)) {
            ADD_PROBLEM_PREFIX();
            fprintf(problems_mem, "role %s duplicated by %s", role, wid);
            continue;
        }
        {
            char *link_buf = NULL;
            size_t link_len = 0;
            FILE *link_mem = open_memstream(&link_buf, &link_len);
            int resolved;
            if (!link_mem) {
                goto done;
            }
            resolved = resolve_witness_link(registry, stmt, &primitive,
                                            link_mem);
            fclose(link_mem);
            if (!resolved) {
                ADD_PROBLEM_PREFIX();
                fprintf(problems_mem, "%s %s", wid, link_buf);
                free(link_buf);
                continue;
            }
            free(link_buf);
        }
        if (!strlist_contains(&permitted, primitive)) {
            ADD_PROBLEM_PREFIX();
            fprintf(problems_mem,
                    "%s role %s uses unpermitted primitive %s", wid, role,
                    primitive);
            continue;
        }
        if (!strlist_push(&bound_roles, role) ||
            !strlist_push(&bound_wids, wid)) {
            goto done;
        }
    }
    {
        strlist missing = {0};
        for (i = 0; i < required.count; i++) {
            if (!strlist_contains(&bound_roles, required.items[i])) {
                if (!strlist_push(&missing, required.items[i])) {
                    strlist_free(&missing);
                    goto done;
                }
            }
        }
        if (missing.count > 0) {
            ADD_PROBLEM_PREFIX();
            fputs("missing roles ", problems_mem);
            repr_list(problems_mem, missing.items, missing.count);
        }
        strlist_free(&missing);
    }
#undef ADD_PROBLEM_PREFIX
    fclose(problems_mem);
    problems_mem = NULL;

    fprintf(mem, "framework %s complete (roles %zu/%zu", key,
            bound_roles.count, required.count);
    if (problem_count > 0) {
        fprintf(mem, "; %s", problems_buf);
    }
    fputc(')', mem);
    pass = problem_count == 0;

done:
    if (problems_mem) {
        fclose(problems_mem);
    }
    free(problems_buf);
    cdc_frontend_alloc(sorted, 0);
    strlist_free(&required);
    strlist_free(&permitted);
    strlist_free(&bound_roles);
    strlist_free(&bound_wids);
    return pass;
}

/* witnesses carrying attr==key count (law/capability linkage) */
static size_t witnesses_for(cdc_registry *registry, const char *key,
                            const char *attr) {
    size_t i, n = 0;
    for (i = 0; i < registry->witnesses.count; i++) {
        const char *value =
            cdc_stmt_attr(registry->witnesses.items[i].stmt, attr);
        if (value && strcmp(value, key) == 0) {
            n++;
        }
    }
    return n;
}

/* job-link family: expect <head> <wid> where the witness's <head> attr must
 * name a declared job of the matching form set. */
static int eval_job_link(cdc_registry *registry, const cdc_stmt *stmt,
                         const char *head, const char *const *forms,
                         size_t form_count, const char *missing_word,
                         FILE *mem) {
    const char *wid = expect_arg(stmt, 1);
    keyed *witness = wid ? keyedlist_find(&registry->witnesses, wid) : NULL;
    const char *step;
    size_t f;
    int ok = 0;
    if (!witness) {
        fprintf(mem, "%s %s (missing witness)", head, wid ? wid : "");
        return 0;
    }
    step = cdc_stmt_attr(witness->stmt, head);
    if (step) {
        for (f = 0; f < form_count; f++) {
            if (registry_has_form_id(registry, forms[f], step)) {
                ok = 1;
                break;
            }
        }
        fprintf(mem, "%s %s (%s %s)", head, wid, missing_word, step);
    } else {
        fprintf(mem, "%s %s (missing %s link)", head, wid, head);
    }
    return ok;
}

/* eval_expect mirror; writes the label into mem, returns pass */
static int eval_expect(cdc_registry *registry, const cdc_stmt *stmt,
                       const char *python_root, FILE *mem) {
    size_t argc = expect_argc(stmt);
    const char *head = expect_arg(stmt, 0);
    size_t i;

    if (argc == 0) {
        fputs("empty expectation", mem);
        return 0;
    }

    if (strcmp(head, "native") == 0 && argc >= 4 &&
        strcmp(expect_arg(stmt, 1), "substrate") == 0 &&
        strcmp(expect_arg(stmt, 2), "==") == 0) {
        const char *want = expect_arg(stmt, 3);
        const char *got =
            registry->kernel ? cdc_stmt_attr(registry->kernel, "target")
                             : NULL;
        fprintf(mem, "native substrate == %s (got %s)", want,
                got && *got ? got : "unset");
        return got && strcmp(got, want) == 0;
    }

    if (strcmp(head, "host-debt") == 0 && argc >= 3) {
        long want;
        long got = registry->bootsteps.count > 0 ? 1 : 0;
        const char *op = expect_arg(stmt, 1);
        if (!parse_int(expect_arg(stmt, 2), &want)) {
            goto unknown;
        }
        fprintf(mem, "host-debt %s %ld (got %ld)", op, want, got);
        return compare_counts(got, op, want);
    }

    if (strcmp(head, "frameworks") == 0 && argc == 2 &&
        strcmp(expect_arg(stmt, 1), "closed") == 0) {
        strlist labels = {0}, orphans = {0};
        int pass;
        for (i = 0; i < registry->frameworks.count; i++) {
            const char *label =
                cdc_stmt_attr(registry->frameworks.items[i].stmt, "label");
            if (!strlist_add_unique(&labels, label ? label : "")) {
                goto closed_fail;
            }
        }
        for (i = 0; i < registry->witnesses.count; i++) {
            const char *fw =
                cdc_stmt_attr(registry->witnesses.items[i].stmt,
                              "framework");
            if (fw && !strlist_contains(&labels, fw)) {
                if (!strlist_add_unique(&orphans, fw)) {
                    goto closed_fail;
                }
            }
        }
        qsort(orphans.items, orphans.count, sizeof(char *), compare_names);
        if (orphans.count > 0) {
            fputs("frameworks closed (orphan bindings ", mem);
            repr_list(mem, orphans.items, orphans.count);
            fputc(')', mem);
        } else {
            fprintf(mem, "frameworks closed (%zu labels)", labels.count);
        }
        pass = orphans.count == 0;
        strlist_free(&labels);
        strlist_free(&orphans);
        return pass;
    closed_fail:
        strlist_free(&labels);
        strlist_free(&orphans);
        return 0;
    }

    if ((strcmp(head, "terms") == 0 || strcmp(head, "rules") == 0 ||
         strcmp(head, "invariants") == 0 || strcmp(head, "witnesses") == 0 ||
         strcmp(head, "capabilities") == 0 ||
         strcmp(head, "frameworks") == 0) &&
        argc >= 3) {
        long want, got;
        const char *op = expect_arg(stmt, 1);
        if (!parse_int(expect_arg(stmt, 2), &want)) {
            goto unknown;
        }
        got = (long)(strcmp(head, "terms") == 0
                         ? registry->terms.count
                         : strcmp(head, "rules") == 0
                               ? registry->rules.count
                               : strcmp(head, "invariants") == 0
                                     ? registry->invariants.count
                                     : strcmp(head, "witnesses") == 0
                                           ? registry->witnesses.count
                                           : strcmp(head, "capabilities") ==
                                                     0
                                                 ? registry->capabilities
                                                       .count
                                                 : registry->frameworks
                                                       .count);
        fprintf(mem, "%s %s %ld (got %ld)", head, op, want, got);
        return compare_counts(got, op, want);
    }

    if (strcmp(head, "provides") == 0) {
        strlist missing = {0};
        int pass;
        for (i = 1; i < argc; i++) {
            const char *item = expect_arg(stmt, i);
            if (!strlist_contains(&registry->provides, item)) {
                if (!strlist_push(&missing, item)) {
                    strlist_free(&missing);
                    return 0;
                }
            }
        }
        fputs("provides", mem);
        for (i = 1; i < argc; i++) {
            fprintf(mem, " %s", expect_arg(stmt, i));
        }
        if (missing.count > 0) {
            fputs(" (missing ", mem);
            repr_list(mem, missing.items, missing.count);
            fputc(')', mem);
        }
        pass = missing.count == 0;
        strlist_free(&missing);
        return pass;
    }

    if (strcmp(head, "law") == 0 && argc >= 2) {
        const char *key = expect_arg(stmt, 1);
        size_t linked = witnesses_for(registry, key, "invariant");
        fprintf(mem, "law %s (witnesses %zu)", key, linked);
        return strlist_contains(&registry->invariants, key) && linked > 0;
    }

    if (strcmp(head, "capability") == 0 && argc >= 2) {
        const char *key = expect_arg(stmt, 1);
        size_t linked = witnesses_for(registry, key, "capability");
        fprintf(mem, "capability %s (witnesses %zu)", key, linked);
        return strlist_contains(&registry->capabilities, key) && linked > 0;
    }

    if (strcmp(head, "framework") == 0 && argc == 3 &&
        strcmp(expect_arg(stmt, 2), "complete") == 0) {
        return framework_complete(registry, expect_arg(stmt, 1), mem);
    }

    if (strcmp(head, "witness") == 0 && argc >= 2) {
        const char *wid = expect_arg(stmt, 1);
        fprintf(mem, "witness %s", wid);
        return keyedlist_find(&registry->witnesses, wid) != NULL;
    }

    if (strcmp(head, "reducer") == 0 && argc >= 2) {
        const char *wid = expect_arg(stmt, 1);
        keyed *witness = keyedlist_find(&registry->witnesses, wid);
        const char *step;
        if (!witness) {
            fprintf(mem, "reducer %s (missing witness)", wid);
            return 0;
        }
        step = cdc_stmt_attr(witness->stmt, "reducer");
        if (step) {
            fprintf(mem, "reducer %s (step %s)", wid, step);
            return registry_has_form_id(registry, "flow", step) ||
                   registry_has_form_id(registry, "commit", step) ||
                   registry_has_form_id(registry, "nest", step);
        }
        fprintf(mem, "reducer %s (missing reducer link)", wid);
        return 0;
    }

    {
        static const char *const SURFACE_HEADS[] = {
            "guard", "trace", "measure", "policy",
            "bridge", "counter", "universal", "spectrum",
        };
        for (i = 0; i < sizeof(SURFACE_HEADS) / sizeof(SURFACE_HEADS[0]);
             i++) {
            if (strcmp(head, SURFACE_HEADS[i]) == 0 && argc >= 2) {
                const char *forms[1];
                forms[0] = head;
                return eval_job_link(registry, stmt, head, forms, 1, "job",
                                     mem);
            }
        }
    }

    if ((strcmp(head, "compile") == 0 || strcmp(head, "proof") == 0 ||
         strcmp(head, "interpret") == 0) &&
        argc >= 2) {
        const char *forms[1];
        forms[0] = head;
        return eval_job_link(registry, stmt, head, forms, 1, "job", mem);
    }

    if (strcmp(head, "council") == 0 && argc >= 2) {
        const char *forms[1];
        forms[0] = "deliberate";
        return eval_job_link(registry, stmt, "council", forms, 1, "job",
                             mem);
    }

    if (strcmp(head, "evolution") == 0 && argc >= 2) {
        const char *forms[1];
        forms[0] = "evolve";
        return eval_job_link(registry, stmt, "evolution", forms, 1, "job",
                             mem);
    }

    if (strcmp(head, "store") == 0 && argc >= 2) {
        const char *forms[1];
        forms[0] = "store";
        return eval_job_link(registry, stmt, "store", forms, 1, "job", mem);
    }

    if (strcmp(head, "persistence") == 0 && argc >= 2) {
        const char *forms[1];
        forms[0] = "persist";
        return eval_job_link(registry, stmt, "persistence", forms, 1, "job",
                             mem);
    }

    if (strcmp(head, "python-files") == 0 && argc >= 3) {
        strlist files = {0};
        strlist counted = {0};
        strlist exempt = {0};
        size_t n;
        long want;
        int pass;
        const char *op = expect_arg(stmt, 1);
        if (!parse_int(expect_arg(stmt, 2), &want)) {
            goto unknown;
        }
        if (!python_files_in(python_root, &files)) {
            strlist_free(&files);
            return 0;
        }
        /* Frozen CI-only oracle exemption (Option A, D33): cdc_boot.py is
         * exempt BY NAME — the floor claims the language's execution
         * surface, and the oracle is a test fixture. The exemption is
         * rendered, never silent, and `bootloader minimal` still sees the
         * raw enumeration. Mirrors cdc_boot.py exactly (parity gate). */
        for (n = 0; n < files.count; n++) {
            strlist *side = strcmp(files.items[n], "cdc_boot.py") == 0
                                ? &exempt
                                : &counted;
            if (!strlist_push(side, files.items[n])) {
                strlist_free(&files);
                strlist_free(&counted);
                strlist_free(&exempt);
                return 0;
            }
        }
        fprintf(mem, "python-files %s %ld (got %zu: ", op, want,
                counted.count);
        repr_list(mem, counted.items, counted.count);
        fputs("; exempt oracle: ", mem);
        repr_list(mem, exempt.items, exempt.count);
        fputc(')', mem);
        pass = compare_counts((long)counted.count, op, want);
        strlist_free(&files);
        strlist_free(&counted);
        strlist_free(&exempt);
        return pass;
    }

    if (strcmp(head, "bootloader") == 0 && argc == 4 &&
        strcmp(expect_arg(stmt, 1), "minimal") == 0 &&
        strcmp(expect_arg(stmt, 2), "==") == 0 &&
        strcmp(expect_arg(stmt, 3), "true") == 0) {
        strlist files = {0};
        int pass;
        if (!python_files_in(python_root, &files)) {
            strlist_free(&files);
            return 0;
        }
        fputs("bootloader minimal == true (python files ", mem);
        repr_list(mem, files.items, files.count);
        fputc(')', mem);
        pass = files.count == 1 &&
               strcmp(files.items[0], "cdc_boot.py") == 0;
        strlist_free(&files);
        return pass;
    }

unknown:
    fputs("unknown expectation:", mem);
    for (i = 0; i < argc; i++) {
        fprintf(mem, " %s", expect_arg(stmt, i));
    }
    return 0;
}

int cdc_registry_vectors(cdc_registry *registry, const char *python_root,
                         FILE *stream) {
    size_t i;
    size_t passed = 0;
    cdc_vector_chain chain;

    cdc_vector_chain_init(&chain);
    for (i = 0; i < registry->expect_count; i++) {
        char *label_buf = NULL;
        size_t label_len = 0;
        FILE *mem = open_memstream(&label_buf, &label_len);
        char record[512];
        int ok;
        if (!mem) {
            return -1;
        }
        ok = eval_expect(registry, registry->expects[i].stmt, python_root,
                         mem);
        fclose(mem);
        passed += ok ? 1 : 0;
        /* The effects digest covers the check's full evaluated label, so a
         * check whose verdict is unchanged but whose REASON changed still
         * shows up as a different vector. */
        if (cdc_vector_render(&chain, registry->expects[i].source,
                              ok ? "commit" : "fail", NULL, label_buf,
                              label_len, NULL, record,
                              sizeof(record)) < 0) {
            free(label_buf);
            return -1;
        }
        free(label_buf);
        fprintf(stream, "%s\n", record);
    }
    return passed == registry->expect_count ? 1 : 0;
}

int cdc_registry_report(cdc_registry *registry, const char *python_root,
                        FILE *stream) {
    size_t i;
    size_t passed = 0;
    char rule[80];

    memset(rule, '=', 74);
    rule[74] = '\0';
    fprintf(stream, "%s\n", rule);
    fprintf(stream, "  .cdc native contract report\n");
    fprintf(stream, "%s\n", rule);
    for (i = 0; i < registry->expect_count; i++) {
        char *label_buf = NULL;
        size_t label_len = 0;
        FILE *mem = open_memstream(&label_buf, &label_len);
        int ok;
        if (!mem) {
            return -1;
        }
        ok = eval_expect(registry, registry->expects[i].stmt, python_root,
                         mem);
        fclose(mem);
        passed += ok ? 1 : 0;
        fprintf(stream, "  %s %s   [%s]\n", ok ? "OK" : "FAIL", label_buf,
                registry->expects[i].source);
        free(label_buf);
    }
    memset(rule, '-', 74);
    rule[74] = '\0';
    fprintf(stream, "%s\n", rule);
    fprintf(stream, "  %zu/%zu expectations met\n", passed,
            registry->expect_count);
    fprintf(stream, "  %zu terms, %zu rules, %zu invariants\n",
            registry->terms.count, registry->rules.count,
            registry->invariants.count);
    fprintf(stream,
            "  %zu capabilities, %zu frameworks, %zu native witnesses\n",
            registry->capabilities.count, registry->frameworks.count,
            registry->witnesses.count);
    memset(rule, '=', 74);
    rule[74] = '\0';
    fprintf(stream, "%s\n", rule);
    return passed == registry->expect_count ? 1 : 0;
}
