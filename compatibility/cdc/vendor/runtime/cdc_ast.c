#include "cdc_ast.h"

#include <string.h>

static const char *const FORM_DIRECTIVES[] = {
    "field",   "module",  "cell",    "channel", "guard",
    "counter", "flow",    "commit",  "nest",    "trace",
    "measure", "policy",  "bridge",  "compile", "interpret",
    "proof",   "council", "deliberate", "evolve", "universal",
    "orbit",   "variational", "spectrum",
    "store",   "persist", "frame",   "reduce",  "complex",
    "topology", "authority", "transport",
};

int cdc_directive_is_form(const char *directive) {
    size_t i;
    for (i = 0; i < sizeof(FORM_DIRECTIVES) / sizeof(FORM_DIRECTIVES[0]); i++) {
        if (strcmp(directive, FORM_DIRECTIVES[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

cdc_stmt_kind cdc_directive_kind(const char *directive) {
    if (strcmp(directive, "kernel") == 0) {
        return CDC_STMT_KERNEL;
    }
    if (strcmp(directive, "term") == 0) {
        return CDC_STMT_TERM;
    }
    if (strcmp(directive, "rule") == 0) {
        return CDC_STMT_RULE;
    }
    if (strcmp(directive, "provides") == 0) {
        return CDC_STMT_PROVIDES;
    }
    if (strcmp(directive, "bootloader") == 0) {
        return CDC_STMT_BOOTLOADER;
    }
    if (strcmp(directive, "invariant") == 0 || strcmp(directive, "law") == 0) {
        return CDC_STMT_INVARIANT;
    }
    if (strcmp(directive, "capability") == 0) {
        return CDC_STMT_CAPABILITY;
    }
    if (strcmp(directive, "framework") == 0) {
        return CDC_STMT_FRAMEWORK;
    }
    if (strcmp(directive, "witness") == 0) {
        return CDC_STMT_WITNESS;
    }
    if (strcmp(directive, "expect") == 0) {
        return CDC_STMT_EXPECT;
    }
    if (cdc_directive_is_form(directive)) {
        return CDC_STMT_FORM;
    }
    return CDC_STMT_UNKNOWN;
}

void cdc_unit_init(cdc_unit *program) {
    memset(program, 0, sizeof(*program));
}

void cdc_unit_free(cdc_unit *program) {
    size_t i, j;
    if (!program) {
        return;
    }
    for (i = 0; i < program->count; i++) {
        cdc_stmt *stmt = &program->stmts[i];
        for (j = 0; j < stmt->token_count; j++) {
            cdc_frontend_alloc(stmt->tokens[j].text, 0);
        }
        cdc_frontend_alloc(stmt->tokens, 0);
    }
    cdc_frontend_alloc(program->stmts, 0);
    cdc_frontend_alloc(program->file, 0);
    cdc_unit_init(program);
}

const char *cdc_stmt_directive(const cdc_stmt *stmt) {
    if (stmt->token_count == 0) {
        return "";
    }
    return stmt->tokens[0].text;
}

static int token_is_attr(const cdc_token *token) {
    return memchr(token->text, '=', token->length) != NULL;
}

size_t cdc_stmt_arg_count(const cdc_stmt *stmt) {
    size_t i, n = 0;
    for (i = 1; i < stmt->token_count; i++) {
        if (!token_is_attr(&stmt->tokens[i])) {
            n++;
        }
    }
    return n;
}

const char *cdc_stmt_arg(const cdc_stmt *stmt, size_t index) {
    size_t i, n = 0;
    for (i = 1; i < stmt->token_count; i++) {
        if (!token_is_attr(&stmt->tokens[i])) {
            if (n == index) {
                return stmt->tokens[i].text;
            }
            n++;
        }
    }
    return NULL;
}

/* key/value split at the first '=' of the unescaped token text. */
static int attr_matches(const cdc_token *token, const char *key,
                        const char **value_out) {
    const char *eq = memchr(token->text, '=', token->length);
    size_t key_len;
    if (!eq) {
        return 0;
    }
    key_len = (size_t)(eq - token->text);
    if (strlen(key) != key_len || strncmp(token->text, key, key_len) != 0) {
        return 0;
    }
    *value_out = eq + 1;
    return 1;
}

const char *cdc_stmt_attr(const cdc_stmt *stmt, const char *key) {
    size_t i;
    const char *value = NULL;
    const char *found;
    for (i = 1; i < stmt->token_count; i++) {
        if (attr_matches(&stmt->tokens[i], key, &found)) {
            value = found; /* last occurrence wins */
        }
    }
    return value;
}

const char *cdc_stmt_attr_first(const cdc_stmt *stmt, const char *key) {
    size_t i;
    const char *found;
    for (i = 1; i < stmt->token_count; i++) {
        if (attr_matches(&stmt->tokens[i], key, &found)) {
            return found;
        }
    }
    return NULL;
}

/* Canonical quoting: bare when the token is non-empty and free of shell
 * structure; double-quoted with '"' and '\\' escaped otherwise. '#' cannot
 * occur in token text (comments are stripped before lexing), so canonical
 * output always survives re-parsing. */
static int needs_quote(const cdc_token *token) {
    size_t i;
    if (token->length == 0) {
        return 1;
    }
    for (i = 0; i < token->length; i++) {
        char c = token->text[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\'' ||
            c == '"' || c == '\\') {
            return 1;
        }
    }
    return 0;
}

static void write_token(const cdc_token *token, FILE *stream) {
    size_t i;
    if (!needs_quote(token)) {
        fwrite(token->text, 1, token->length, stream);
        return;
    }
    fputc('"', stream);
    for (i = 0; i < token->length; i++) {
        char c = token->text[i];
        if (c == '"' || c == '\\') {
            fputc('\\', stream);
        }
        fputc(c, stream);
    }
    fputc('"', stream);
}

void cdc_stmt_canonical(const cdc_stmt *stmt, FILE *stream) {
    size_t i;
    if (stmt->kind == CDC_STMT_END) {
        fputs("end\n", stream);
        return;
    }
    for (i = 0; i < stmt->token_count; i++) {
        if (i > 0) {
            fputc(' ', stream);
        }
        write_token(&stmt->tokens[i], stream);
    }
    fputc('\n', stream);
}

void cdc_unit_canonical(const cdc_unit *program, FILE *stream) {
    size_t i;
    for (i = 0; i < program->count; i++) {
        cdc_stmt_canonical(&program->stmts[i], stream);
    }
}

int cdc_unit_equal(const cdc_unit *a, const cdc_unit *b) {
    size_t i, j;
    if (a->count != b->count) {
        return 0;
    }
    for (i = 0; i < a->count; i++) {
        const cdc_stmt *sa = &a->stmts[i];
        const cdc_stmt *sb = &b->stmts[i];
        if (sa->kind != sb->kind || sa->token_count != sb->token_count) {
            return 0;
        }
        for (j = 0; j < sa->token_count; j++) {
            if (sa->tokens[j].length != sb->tokens[j].length ||
                memcmp(sa->tokens[j].text, sb->tokens[j].text,
                       sa->tokens[j].length) != 0) {
                return 0;
            }
        }
    }
    return 1;
}
