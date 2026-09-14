#ifndef CDC_LEXER_H
#define CDC_LEXER_H

#include <stddef.h>

#include "cdc_diagnostic.h"

/* Grammar-1 lexer (Amendment A1).
 *
 * The accepted language is byte-identical to grammar 0, the legacy loader
 * pipeline (cdc_boot.py): per line, first strip everything from the first '#'
 * onward — even inside quotes (DECISIONS D5) — then trim ASCII whitespace,
 * then split with POSIX-shell token rules:
 *   - tokens separate on unquoted whitespace;
 *   - '...' preserves everything literally, no escapes;
 *   - "..." preserves text; backslash escapes only '"' and '\\';
 *   - outside quotes, backslash escapes any single character;
 *   - adjacent quoted/unquoted segments concatenate into one token;
 *   - an unterminated quote or trailing escape is a lexical error
 *     (grammar 0 raises SyntaxError there).
 * Bytes are treated opaquely; UTF-8 validity is not required (structural
 * characters are all ASCII). A line whose stripped content is empty produces
 * no tokens; a line whose stripped content is exactly "end" is structural
 * (grammar 0 skips it before tokenizing).
 */

typedef enum {
    CDC_LINE_EMPTY = 0,  /* nothing after comment strip + trim */
    CDC_LINE_END = 1,    /* the literal "end" structural line */
    CDC_LINE_TOKENS = 2, /* one or more tokens */
    CDC_LINE_ERROR = 3   /* lexical error; diagnostics recorded */
} cdc_line_kind;

typedef struct {
    char *text;      /* unescaped token text, NUL-terminated, owned */
    size_t length;   /* strlen(text); embedded NULs cannot occur */
    int col;         /* 1-based byte column of token start in the raw line */
    int raw_len;     /* byte length of the raw source region of the token */
    int had_quotes;  /* any part of the token was quoted */
    int had_equals_quoted; /* '=' appeared only inside quotes (still an
                              attribute in grammar 0, which splits on the
                              unescaped text) */
} cdc_token;

typedef struct {
    cdc_line_kind kind;
    int line;          /* 1-based */
    cdc_token *tokens; /* owned array */
    size_t token_count;
} cdc_line;

/* Tokenizes one raw source line (without its newline). Appends diagnostics
 * on lexical errors. Returns 0 on allocation failure (out->kind is then
 * CDC_LINE_ERROR and the line owns nothing). */
int cdc_lex_line(const char *raw, int line_no, const char *file,
                 cdc_line *out, cdc_diag_list *diags);

void cdc_line_free(cdc_line *line);

/* Hard bounds (typed diagnostics, not crashes). */
enum {
    CDC_LEX_MAX_LINE = 1 << 20,   /* 1 MiB per line */
    CDC_LEX_MAX_TOKENS = 4096     /* tokens per line */
};

#endif
