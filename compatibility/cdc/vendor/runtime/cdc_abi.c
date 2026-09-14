#define _POSIX_C_SOURCE 200809L

#include "cdc_abi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cdc_ast.h"
#include "cdc_diagnostic.h"
#include "cdc_parser.h"
#include "cdc_registry.h"

struct cdc_program {
    cdc_unit unit;
    cdc_diag_list diags;
    int completed; /* parse ran to completion (allocation-wise) */
};

struct cdc_runtime {
    uint32_t abi;
    cdc_registry *registry;
    cdc_program **loaded; /* owned programs backing the registry */
    size_t loaded_count;
    size_t loaded_cap;
};

struct cdc_result {
    char *json;
    size_t length;
    char *text; /* verify report text, or NULL */
    size_t errors;
};

uint32_t cdc_abi_version(void) {
    return ((uint32_t)CDC_ABI_VERSION_MAJOR << 16) |
           (uint32_t)CDC_ABI_VERSION_MINOR;
}

const char *cdc_status_name(cdc_status status) {
    switch (status) {
    case CDC_OK:
        return "ok";
    case CDC_ERR_ARGUMENT:
        return "argument";
    case CDC_ERR_IO:
        return "io";
    case CDC_ERR_PARSE:
        return "parse";
    case CDC_ERR_MEMORY:
        return "memory";
    case CDC_ERR_STATE:
        return "state";
    default:
        return "unknown";
    }
}

/* CDC001-CDC004 are the parser's typed I/O rejection codes (unreadable,
 * non-regular, read/stat error, size bound). They map to CDC_ERR_IO and are
 * never conflated with allocation failure. */
static int diags_have_io_code(const cdc_diag_list *diags) {
    size_t i;
    for (i = 0; i < diags->count; i++) {
        const char *code = diags->items[i].code;
        if (strncmp(code, "CDC00", 5) == 0 && code[5] >= '1' &&
            code[5] <= '4' && code[6] == '\0') {
            return 1;
        }
    }
    return 0;
}

cdc_status cdc_program_parse(const char *path, const char *buffer,
                             size_t length, cdc_program **out) {
    cdc_program *program;
    int completed;

    if (!out || (path && buffer) || (!path && !buffer)) {
        if (out) {
            *out = NULL;
        }
        return CDC_ERR_ARGUMENT;
    }
    *out = NULL;
    program = cdc_frontend_alloc(NULL, sizeof(*program));
    if (!program) {
        return CDC_ERR_MEMORY;
    }
    memset(program, 0, sizeof(*program));
    cdc_unit_init(&program->unit);
    cdc_diag_list_init(&program->diags);

    if (path) {
        completed = cdc_unit_parse_file(path, &program->unit,
                                        &program->diags);
        if (!completed && diags_have_io_code(&program->diags)) {
            cdc_program_destroy(program);
            return CDC_ERR_IO;
        }
    } else {
        completed = cdc_unit_parse_buffer(buffer, length, "<buffer>",
                                          &program->unit, &program->diags);
    }
    if (!completed || program->diags.out_of_memory) {
        cdc_program_destroy(program);
        return CDC_ERR_MEMORY;
    }
    program->completed = 1;
    *out = program;
    return program->diags.errors > 0 ? CDC_ERR_PARSE : CDC_OK;
}

size_t cdc_program_statement_count(const cdc_program *program) {
    return program ? program->unit.count : 0;
}

size_t cdc_program_error_count(const cdc_program *program) {
    return program ? program->diags.errors : 0;
}

static const cdc_stmt *statement_at(const cdc_program *program,
                                    size_t index) {
    if (!program || index >= program->unit.count) {
        return NULL;
    }
    return &program->unit.stmts[index];
}

const char *cdc_program_statement_directive(const cdc_program *program,
                                            size_t index) {
    const cdc_stmt *stmt = statement_at(program, index);
    if (!stmt) {
        return NULL;
    }
    return stmt->kind == CDC_STMT_END ? "end" : cdc_stmt_directive(stmt);
}

const char *cdc_program_statement_arg(const cdc_program *program,
                                      size_t index, size_t arg_index) {
    const cdc_stmt *stmt = statement_at(program, index);
    if (!stmt || stmt->kind == CDC_STMT_END) {
        return NULL;
    }
    return cdc_stmt_arg(stmt, arg_index);
}

const char *cdc_program_statement_attr(const cdc_program *program,
                                       size_t index, const char *key) {
    const cdc_stmt *stmt = statement_at(program, index);
    if (!stmt || stmt->kind == CDC_STMT_END || !key) {
        return NULL;
    }
    return cdc_stmt_attr(stmt, key);
}

/* JSON string escaping: '"', '\\', and control bytes. */
static int json_escape_into(FILE *stream, const char *text) {
    const unsigned char *p = (const unsigned char *)text;
    for (; *p; p++) {
        if (*p == '"' || *p == '\\') {
            if (fprintf(stream, "\\%c", *p) < 0) {
                return 0;
            }
        } else if (*p < 0x20) {
            if (fprintf(stream, "\\u%04x", *p) < 0) {
                return 0;
            }
        } else {
            if (fputc(*p, stream) == EOF) {
                return 0;
            }
        }
    }
    return 1;
}

static cdc_status result_from_lines(size_t errors, const char *const *lines,
                                    size_t line_count, cdc_result **out) {
    cdc_result *result;
    char *bytes = NULL;
    size_t length = 0;
    FILE *mem;
    size_t i;

    *out = NULL;
    mem = open_memstream(&bytes, &length);
    if (!mem) {
        return CDC_ERR_MEMORY;
    }
    fprintf(mem, "{\"abi\":\"%d.%d\",\"errors\":%zu,\"diagnostics\":[",
            CDC_ABI_VERSION_MAJOR, CDC_ABI_VERSION_MINOR, errors);
    for (i = 0; i < line_count; i++) {
        if (i > 0) {
            fputc(',', mem);
        }
        fputc('"', mem);
        if (!json_escape_into(mem, lines[i])) {
            fclose(mem);
            free(bytes);
            return CDC_ERR_MEMORY;
        }
        fputc('"', mem);
    }
    fputs("]}", mem);
    if (fclose(mem) != 0) {
        free(bytes);
        return CDC_ERR_MEMORY;
    }
    result = cdc_frontend_alloc(NULL, sizeof(*result));
    if (!result) {
        free(bytes);
        return CDC_ERR_MEMORY;
    }
    memset(result, 0, sizeof(*result));
    result->json = bytes;
    result->length = length;
    result->errors = errors;
    *out = result;
    return CDC_OK;
}

cdc_status cdc_program_diagnostics(const cdc_program *program,
                                   cdc_result **out) {
    const char **lines = NULL;
    char **storage = NULL;
    size_t i, filled = 0;
    cdc_status status = CDC_OK;

    if (!program || !out) {
        return CDC_ERR_ARGUMENT;
    }
    *out = NULL;
    if (program->diags.count > 0) {
        lines = cdc_frontend_alloc(NULL,
                                   program->diags.count * sizeof(*lines));
        storage = cdc_frontend_alloc(NULL,
                                     program->diags.count * sizeof(*storage));
        if (!lines || !storage) {
            cdc_frontend_alloc(lines, 0);
            cdc_frontend_alloc(storage, 0);
            return CDC_ERR_MEMORY;
        }
        for (i = 0; i < program->diags.count; i++) {
            /* Two-pass render (adversarial-review defect 1): size first,
             * allocate exactly, render into the full allocation, and strip
             * the trailing newline only inside it. Diagnostics are evidence
             * surfaces — never truncated. */
            size_t needed =
                cdc_diag_render(&program->diags.items[i], NULL, 0);
            char *slot;
            if (needed >= SIZE_MAX - 1) {
                status = CDC_ERR_MEMORY; /* size arithmetic fails closed */
                break;
            }
            slot = cdc_frontend_alloc(NULL, needed + 1);
            if (!slot) {
                status = CDC_ERR_MEMORY;
                break;
            }
            cdc_diag_render(&program->diags.items[i], slot, needed + 1);
            if (needed > 0 && slot[needed - 1] == '\n') {
                slot[needed - 1] = '\0';
            }
            storage[filled] = slot;
            lines[filled] = slot;
            filled++;
        }
    }
    if (status == CDC_OK) {
        status = result_from_lines(program->diags.errors, lines, filled,
                                   out);
    }
    for (i = 0; i < filled; i++) {
        cdc_frontend_alloc(storage[i], 0);
    }
    cdc_frontend_alloc(storage, 0);
    cdc_frontend_alloc(lines, 0);
    return status;
}

cdc_status cdc_program_canonical_bytes(const cdc_program *program,
                                       char **out_bytes,
                                       size_t *out_length) {
    char *bytes = NULL;
    size_t length = 0;
    FILE *mem;

    if (!program || !out_bytes || !out_length) {
        return CDC_ERR_ARGUMENT;
    }
    *out_bytes = NULL;
    *out_length = 0;
    if (!program->completed || program->diags.errors > 0) {
        return CDC_ERR_STATE;
    }
    mem = open_memstream(&bytes, &length);
    if (!mem) {
        return CDC_ERR_MEMORY;
    }
    cdc_unit_canonical(&program->unit, mem);
    if (fclose(mem) != 0) {
        free(bytes);
        return CDC_ERR_MEMORY;
    }
    *out_bytes = bytes;
    *out_length = length;
    return CDC_OK;
}

cdc_status cdc_runtime_create(cdc_runtime **out) {
    cdc_runtime *runtime;
    if (!out) {
        return CDC_ERR_ARGUMENT;
    }
    runtime = cdc_frontend_alloc(NULL, sizeof(*runtime));
    if (!runtime) {
        *out = NULL;
        return CDC_ERR_MEMORY;
    }
    memset(runtime, 0, sizeof(*runtime));
    runtime->abi = cdc_abi_version();
    runtime->registry = cdc_registry_create();
    if (!runtime->registry) {
        cdc_frontend_alloc(runtime, 0);
        *out = NULL;
        return CDC_ERR_MEMORY;
    }
    *out = runtime;
    return CDC_OK;
}

cdc_status cdc_runtime_load(cdc_runtime *runtime, cdc_program *program) {
    cdc_diag_list load_diags;
    int loaded;

    if (!runtime || !program) {
        return CDC_ERR_ARGUMENT;
    }
    if (!program->completed || program->diags.errors > 0) {
        return CDC_ERR_STATE;
    }
    if (runtime->loaded_count == runtime->loaded_cap) {
        size_t next = runtime->loaded_cap ? runtime->loaded_cap * 2 : 8;
        void *grown = cdc_frontend_alloc(runtime->loaded,
                                         next * sizeof(cdc_program *));
        if (!grown) {
            return CDC_ERR_MEMORY;
        }
        runtime->loaded = grown;
        runtime->loaded_cap = next;
    }
    cdc_diag_list_init(&load_diags);
    loaded = cdc_registry_load(runtime->registry, &program->unit,
                               &load_diags);
    if (!loaded) {
        cdc_status status =
            load_diags.count > 0 ? CDC_ERR_PARSE : CDC_ERR_MEMORY;
        cdc_diag_list_free(&load_diags);
        return status;
    }
    cdc_diag_list_free(&load_diags);
    runtime->loaded[runtime->loaded_count++] = program;
    return CDC_OK;
}

cdc_status cdc_runtime_vectors(cdc_runtime *runtime,
                               const cdc_program *program,
                               cdc_result **out) {
    char *vectors = NULL;
    size_t vectors_len = 0;
    FILE *mem;
    int verdict;
    cdc_result *result;

    if (!runtime || !out) {
        return CDC_ERR_ARGUMENT;
    }
    *out = NULL;
    if (program) {
        cdc_status status =
            cdc_runtime_load(runtime, (cdc_program *)program);
        if (status != CDC_OK) {
            return status;
        }
    }
    mem = open_memstream(&vectors, &vectors_len);
    if (!mem) {
        return CDC_ERR_MEMORY;
    }
    verdict = cdc_registry_vectors(runtime->registry, ".", mem);
    if (fclose(mem) != 0 || verdict < 0) {
        free(vectors);
        return CDC_ERR_MEMORY;
    }
    result = cdc_frontend_alloc(NULL, sizeof(*result));
    if (!result) {
        free(vectors);
        return CDC_ERR_MEMORY;
    }
    memset(result, 0, sizeof(*result));
    result->text = vectors;
    result->errors = verdict == 1 ? 0 : 1;
    *out = result;
    return CDC_OK;
}

cdc_status cdc_runtime_verify(cdc_runtime *runtime,
                              const cdc_program *program, cdc_result **out) {
    char *report = NULL;
    size_t report_len = 0;
    FILE *mem;
    int verdict;
    cdc_result *result;
    size_t failed;

    if (!runtime || !out) {
        return CDC_ERR_ARGUMENT;
    }
    *out = NULL;
    if (program) {
        cdc_status status =
            cdc_runtime_load(runtime, (cdc_program *)program);
        if (status != CDC_OK) {
            return status;
        }
    }
    mem = open_memstream(&report, &report_len);
    if (!mem) {
        return CDC_ERR_MEMORY;
    }
    verdict = cdc_registry_report(runtime->registry, ".", mem);
    if (fclose(mem) != 0 || verdict < 0) {
        free(report);
        return CDC_ERR_MEMORY;
    }
    /* failed-expectation count: reparse the summary is fragile; the
     * registry's verdict is boolean, so carry 0 or 1+ via a scan of FAIL
     * records for an exact count. */
    failed = 0;
    {
        const char *p = report;
        while ((p = strstr(p, "\n  FAIL ")) != NULL) {
            failed++;
            p += 8;
        }
    }
    if (verdict == 0 && failed == 0) {
        failed = 1; /* fail-closed: verdict says failure even if scan missed */
    }
    result = cdc_frontend_alloc(NULL, sizeof(*result));
    if (!result) {
        free(report);
        return CDC_ERR_MEMORY;
    }
    memset(result, 0, sizeof(*result));
    result->text = report;
    result->errors = failed;
    {
        char *json = NULL;
        size_t json_len = 0;
        FILE *jmem = open_memstream(&json, &json_len);
        if (!jmem) {
            free(report);
            cdc_frontend_alloc(result, 0);
            return CDC_ERR_MEMORY;
        }
        fprintf(jmem,
                "{\"abi\":\"%d.%d\",\"errors\":%zu,\"diagnostics\":[]}",
                CDC_ABI_VERSION_MAJOR, CDC_ABI_VERSION_MINOR, failed);
        if (fclose(jmem) != 0) {
            free(json);
            free(report);
            cdc_frontend_alloc(result, 0);
            return CDC_ERR_MEMORY;
        }
        result->json = json;
        result->length = json_len;
    }
    *out = result;
    return CDC_OK;
}

const char *cdc_result_text(const cdc_result *result) {
    return result && result->text ? result->text : "";
}

static cdc_status unavailable_result(const char *operation,
                                     cdc_result **out) {
    char line[128];
    const char *lines[1];
    if (!out) {
        return CDC_ERR_STATE;
    }
    snprintf(line, sizeof(line),
             "<abi>: error[CDC950]: %s lands with the Phase C execution "
             "surface (ABI 1.1)",
             operation);
    lines[0] = line;
    if (result_from_lines(1, lines, 1, out) != CDC_OK) {
        *out = NULL;
    }
    return CDC_ERR_STATE;
}

cdc_status cdc_runtime_execute(cdc_runtime *runtime,
                               const cdc_program *program,
                               const char *entry_job, cdc_result **out) {
    (void)entry_job;
    if (!runtime || !program) {
        return CDC_ERR_ARGUMENT;
    }
    return unavailable_result("cdc_runtime_execute", out);
}

size_t cdc_result_error_count(const cdc_result *result) {
    return result ? result->errors : 0;
}

cdc_status cdc_result_serialize(const cdc_result *result, char **out_bytes,
                                size_t *out_length) {
    char *copy;
    if (!result || !out_bytes || !out_length) {
        return CDC_ERR_ARGUMENT;
    }
    copy = cdc_frontend_alloc(NULL, result->length + 1);
    if (!copy) {
        *out_bytes = NULL;
        *out_length = 0;
        return CDC_ERR_MEMORY;
    }
    memcpy(copy, result->json, result->length + 1);
    *out_bytes = copy;
    *out_length = result->length;
    return CDC_OK;
}

void cdc_program_destroy(cdc_program *program) {
    if (!program) {
        return;
    }
    cdc_unit_free(&program->unit);
    cdc_diag_list_free(&program->diags);
    free(program);
}

void cdc_runtime_destroy(cdc_runtime *runtime) {
    size_t i;
    if (!runtime) {
        return;
    }
    cdc_registry_destroy(runtime->registry);
    for (i = 0; i < runtime->loaded_count; i++) {
        cdc_program_destroy(runtime->loaded[i]);
    }
    cdc_frontend_alloc(runtime->loaded, 0);
    cdc_frontend_alloc(runtime, 0);
}

void cdc_result_destroy(cdc_result *result) {
    if (!result) {
        return;
    }
    free(result->json);
    free(result->text);
    cdc_frontend_alloc(result, 0);
}

void cdc_bytes_free(char *bytes) {
    free(bytes);
}
