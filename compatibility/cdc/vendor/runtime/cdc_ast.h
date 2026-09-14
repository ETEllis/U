#ifndef CDC_AST_H
#define CDC_AST_H

#include <stdio.h>

#include "cdc_lexer.h"

/* Grammar-1 AST (Amendment A1). A program is an ordered statement stream;
 * a statement is the token sequence of one non-empty source line plus its
 * grammar-0 classification. Attribute views replicate grammar-0 consumer
 * semantics exactly:
 *   - dict view (cdc_stmt_attr): first-occurrence order, last value wins
 *     (Python dict semantics in cdc_boot.py);
 *   - legacy view (cdc_stmt_attr_first): first occurrence wins (C scanner
 *     semantics in cdc_source.c);
 *   - args are the tokens after the directive containing no '=';
 *   - expect statements are consumed as raw token sequences (grammar 0
 *     stores them verbatim, bypassing the args/attrs split).
 */

typedef enum {
    CDC_STMT_KERNEL,
    CDC_STMT_TERM,
    CDC_STMT_RULE,
    CDC_STMT_PROVIDES,
    CDC_STMT_BOOTLOADER,
    CDC_STMT_INVARIANT, /* invariant + law */
    CDC_STMT_CAPABILITY,
    CDC_STMT_FRAMEWORK,
    CDC_STMT_WITNESS,
    CDC_STMT_FORM, /* field module cell channel guard counter flow commit
                      nest trace measure policy bridge compile interpret
                      proof council deliberate evolve universal orbit
                      variational spectrum store persist frame reduce complex
                      topology authority transport */
    CDC_STMT_EXPECT,
    CDC_STMT_END,
    CDC_STMT_UNKNOWN
} cdc_stmt_kind;

typedef struct {
    cdc_stmt_kind kind;
    int line; /* 1-based source line */
    cdc_token *tokens;
    size_t token_count;
} cdc_stmt;

typedef struct {
    char *file; /* owned basename-preserving copy of the given name */
    cdc_stmt *stmts;
    size_t count;
    size_t capacity;
} cdc_unit;

/* Directive classification shared with the legacy loader. Returns the kind
 * for a directive token ("law" maps to CDC_STMT_INVARIANT; the source-form
 * directives map to CDC_STMT_FORM). */
cdc_stmt_kind cdc_directive_kind(const char *directive);

/* True when the directive is one of the source-form directives. */
int cdc_directive_is_form(const char *directive);

void cdc_unit_init(cdc_unit *program);
void cdc_unit_free(cdc_unit *program);

/* Statement views. */
const char *cdc_stmt_directive(const cdc_stmt *stmt); /* tokens[0] or "" */
size_t cdc_stmt_arg_count(const cdc_stmt *stmt);
const char *cdc_stmt_arg(const cdc_stmt *stmt, size_t index); /* NULL past end */
/* Dict semantics (last wins). NULL when absent. */
const char *cdc_stmt_attr(const cdc_stmt *stmt, const char *key);
/* Legacy first-occurrence semantics. NULL when absent. */
const char *cdc_stmt_attr_first(const cdc_stmt *stmt, const char *key);

/* Canonical serialization (grammar 1): one statement per line, tokens in
 * original order separated by single spaces, quoting normalized (double
 * quotes, escaping '"' and '\\'), comments and blank lines dropped, "end"
 * lines preserved. Deterministic byte output; the canonical bytes are the
 * digest surface for source identity. */
void cdc_stmt_canonical(const cdc_stmt *stmt, FILE *stream);
void cdc_unit_canonical(const cdc_unit *program, FILE *stream);

/* Structural equality of the statement streams (token texts and kinds;
 * line numbers and spans ignored). Used by the round-trip gate. */
int cdc_unit_equal(const cdc_unit *a, const cdc_unit *b);

#endif
