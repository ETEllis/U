#define _POSIX_C_SOURCE 200809L

#include "cdc_parser.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char *dup_string(const char *s) {
    size_t n = strlen(s);
    char *out = cdc_frontend_alloc(NULL, n + 1);
    if (out) {
        memcpy(out, s, n + 1);
    }
    return out;
}

static int program_push(cdc_unit *program, cdc_stmt stmt) {
    if (program->count == program->capacity) {
        size_t next = program->capacity ? program->capacity * 2 : 32;
        void *grown =
            cdc_frontend_alloc(program->stmts, next * sizeof(cdc_stmt));
        if (!grown) {
            return 0;
        }
        program->stmts = grown;
        program->capacity = next;
    }
    program->stmts[program->count++] = stmt;
    return 1;
}

/* Grammar-0 required-first-argument rule: these directives raise in the
 * legacy loader when no flag argument is present. */
static int requires_first_arg(cdc_stmt_kind kind) {
    switch (kind) {
    case CDC_STMT_KERNEL:
    case CDC_STMT_INVARIANT:
    case CDC_STMT_CAPABILITY:
    case CDC_STMT_FRAMEWORK:
    case CDC_STMT_WITNESS:
    case CDC_STMT_FORM:
        return 1;
    default:
        return 0;
    }
}

static void classify_and_check(cdc_unit *program, cdc_stmt *stmt,
                               cdc_diag_list *diags) {
    const char *directive = cdc_stmt_directive(stmt);
    cdc_span span;
    span.file = program->file;
    span.line = stmt->line;
    span.col = stmt->token_count ? stmt->tokens[0].col : 1;
    span.len = stmt->token_count ? (int)stmt->tokens[0].length : 0;

    stmt->kind = cdc_directive_kind(directive);
    if (stmt->kind == CDC_STMT_UNKNOWN) {
        cdc_diag_add(diags, CDC_DIAG_ERROR, "CDC020", span,
                     "unknown directive '%s'", directive);
        return;
    }
    if (requires_first_arg(stmt->kind) && cdc_stmt_arg_count(stmt) == 0) {
        cdc_diag_add(diags, CDC_DIAG_ERROR, "CDC021", span,
                     "%s requires a %s", directive,
                     stmt->kind == CDC_STMT_KERNEL
                         ? "name"
                         : (stmt->kind == CDC_STMT_WITNESS ||
                            stmt->kind == CDC_STMT_FORM)
                               ? "id"
                               : "key");
        return;
    }
    if (stmt->kind == CDC_STMT_FRAMEWORK) {
        /* Duplicate framework keys are rejected by the legacy loader across
         * the whole load; within one parsed unit the parser can already see
         * the collision. Cross-file duplicates are the registry layer's
         * check. */
        const char *key = cdc_stmt_arg(stmt, 0);
        size_t i;
        for (i = 0; i + 1 < program->count; i++) {
            const cdc_stmt *prior = &program->stmts[i];
            if (prior->kind == CDC_STMT_FRAMEWORK) {
                const char *prior_key = cdc_stmt_arg(prior, 0);
                if (prior_key && key && strcmp(prior_key, key) == 0) {
                    cdc_diag_add(diags, CDC_DIAG_ERROR, "CDC022", span,
                                 "duplicate framework '%s'", key);
                    return;
                }
            }
        }
    }
}

int cdc_unit_parse_buffer(const char *buffer, size_t length,
                             const char *file, cdc_unit *out,
                             cdc_diag_list *diags) {
    size_t pos = 0;
    int line_no = 0;
    int ok = 1;

    cdc_unit_init(out);
    out->file = dup_string(file ? file : "<buffer>");
    if (!out->file) {
        return 0;
    }

    while (pos <= length) {
        size_t line_start = pos;
        size_t line_len;
        char *line_copy;
        cdc_line line;

        while (pos < length && buffer[pos] != '\n') {
            pos++;
        }
        line_len = pos - line_start;
        line_no++;

        /* Mirror str.splitlines() + per-line processing: the trailing
         * newline is consumed; '\r' is handled by the lexer trim. */
        line_copy = cdc_frontend_alloc(NULL, line_len + 1);
        if (!line_copy) {
            ok = 0;
            break;
        }
        memcpy(line_copy, buffer + line_start, line_len);
        line_copy[line_len] = '\0';
        /* Reject embedded NUL explicitly (grammar 0 operates on decoded
         * text; a NUL would silently truncate here otherwise). */
        if (memchr(buffer + line_start, '\0', line_len) != NULL) {
            cdc_span span = {out->file, line_no, 1, 0};
            cdc_diag_add(diags, CDC_DIAG_ERROR, "CDC014", span,
                         "NUL byte in source line");
            cdc_frontend_alloc(line_copy, 0);
            if (pos >= length) {
                break;
            }
            pos++;
            continue;
        }

        if (!cdc_lex_line(line_copy, line_no, out->file, &line, diags)) {
            ok = 0;
            cdc_frontend_alloc(line_copy, 0);
            break;
        }
        cdc_frontend_alloc(line_copy, 0);

        if (line.kind == CDC_LINE_TOKENS || line.kind == CDC_LINE_END) {
            cdc_stmt stmt;
            memset(&stmt, 0, sizeof(stmt));
            stmt.line = line_no;
            stmt.kind = line.kind == CDC_LINE_END ? CDC_STMT_END
                                                  : CDC_STMT_UNKNOWN;
            stmt.tokens = line.tokens;
            stmt.token_count = line.token_count;
            if (!program_push(out, stmt)) {
                cdc_line_free(&line);
                ok = 0;
                break;
            }
            if (line.kind == CDC_LINE_TOKENS) {
                classify_and_check(out, &out->stmts[out->count - 1], diags);
            }
        }

        if (pos >= length) {
            break;
        }
        pos++; /* skip '\n' */
    }
    return ok;
}

int cdc_unit_parse_stream(void *stdio_file, const char *path, cdc_unit *out,
                          cdc_diag_list *diags) {
    FILE *fp = stdio_file;
    char *buffer = NULL;
    size_t size = 0, cap = 0, got;
    int ok;
    cdc_span span = {path, 0, 0, 0};

    cdc_unit_init(out);
    for (;;) {
        if (size > (size_t)CDC_PARSE_MAX_SOURCE) {
            cdc_frontend_alloc(buffer, 0);
            cdc_diag_add(diags, CDC_DIAG_ERROR, "CDC004", span,
                         "source exceeds %d bytes", (int)CDC_PARSE_MAX_SOURCE);
            return 0;
        }
        if (size + 65536 > cap) {
            size_t next = cap ? cap * 2 : 65536;
            char *grown = cdc_frontend_alloc(buffer, next);
            if (!grown) {
                cdc_frontend_alloc(buffer, 0);
                return 0;
            }
            buffer = grown;
            cap = next;
        }
        got = fread(buffer + size, 1, cap - size, fp);
        size += got;
        if (got == 0) {
            if (ferror(fp)) {
                cdc_frontend_alloc(buffer, 0);
                cdc_diag_add(diags, CDC_DIAG_ERROR, "CDC003", span,
                             "read error on source file");
                return 0;
            }
            break;
        }
    }
    ok = cdc_unit_parse_buffer(buffer, size, path, out, diags);
    cdc_frontend_alloc(buffer, 0);
    return ok;
}

int cdc_unit_parse_file(const char *path, cdc_unit *out,
                           cdc_diag_list *diags) {
    int fd;
    struct stat st;
    FILE *fp;
    int ok;
    cdc_span span = {path, 0, 0, 0};

    cdc_unit_init(out);
    /* O_NONBLOCK so a FIFO with no writer is rejected by type below instead
     * of blocking the open (adversarial-review defect 2). */
    fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        cdc_diag_add(diags, CDC_DIAG_ERROR, "CDC001", span,
                     "cannot open source file");
        return 0;
    }
    if (fstat(fd, &st) != 0) {
        close(fd);
        cdc_diag_add(diags, CDC_DIAG_ERROR, "CDC003", span,
                     "cannot stat source file");
        return 0;
    }
    if (!S_ISREG(st.st_mode)) {
        close(fd);
        cdc_diag_add(diags, CDC_DIAG_ERROR, "CDC002", span,
                     "not a regular source file");
        return 0;
    }
    if (st.st_size > (off_t)CDC_PARSE_MAX_SOURCE) {
        close(fd);
        cdc_diag_add(diags, CDC_DIAG_ERROR, "CDC004", span,
                     "source exceeds %d bytes", (int)CDC_PARSE_MAX_SOURCE);
        return 0;
    }
    fp = fdopen(fd, "rb");
    if (!fp) {
        close(fd);
        cdc_diag_add(diags, CDC_DIAG_ERROR, "CDC001", span,
                     "cannot open source file");
        return 0;
    }
    ok = cdc_unit_parse_stream(fp, path, out, diags);
    fclose(fp);
    return ok;
}
