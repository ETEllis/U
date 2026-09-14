#include "cdc_lexer.h"

#include <string.h>

/* Grammar 0 separates tokens on the shell whitespace set only; the trim step
 * (Python str.strip) removes a slightly larger ASCII whitespace set at the
 * line edges. Both are replicated exactly. */
static int is_separator(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int is_trim_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
           c == '\f';
}

typedef struct {
    char *data;
    size_t len;
    size_t cap;
    int failed;
} sbuf;

static void sbuf_init(sbuf *b) {
    memset(b, 0, sizeof(*b));
}

static void sbuf_push(sbuf *b, char c) {
    if (b->failed) {
        return;
    }
    if (b->len + 2 > b->cap) {
        size_t next = b->cap ? b->cap * 2 : 32;
        char *grown = cdc_frontend_alloc(b->data, next);
        if (!grown) {
            b->failed = 1;
            return;
        }
        b->data = grown;
        b->cap = next;
    }
    b->data[b->len++] = c;
    b->data[b->len] = '\0';
}

static void sbuf_reset(sbuf *b) {
    b->len = 0;
    if (b->data) {
        b->data[0] = '\0';
    }
}

static void sbuf_free(sbuf *b) {
    cdc_frontend_alloc(b->data, 0);
    sbuf_init(b);
}

typedef struct {
    cdc_token *items;
    size_t count;
    size_t cap;
    int failed;
} tokvec;

static void tokvec_init(tokvec *v) {
    memset(v, 0, sizeof(*v));
}

static void tokvec_free_deep(tokvec *v) {
    size_t i;
    for (i = 0; i < v->count; i++) {
        cdc_frontend_alloc(v->items[i].text, 0);
    }
    cdc_frontend_alloc(v->items, 0);
    tokvec_init(v);
}

static int tokvec_push(tokvec *v, cdc_token token) {
    if (v->count == v->cap) {
        size_t next = v->cap ? v->cap * 2 : 8;
        void *grown = cdc_frontend_alloc(v->items, next * sizeof(cdc_token));
        if (!grown) {
            v->failed = 1;
            return 0;
        }
        v->items = grown;
        v->cap = next;
    }
    v->items[v->count++] = token;
    return 1;
}

void cdc_line_free(cdc_line *line) {
    size_t i;
    if (!line) {
        return;
    }
    for (i = 0; i < line->token_count; i++) {
        cdc_frontend_alloc(line->tokens[i].text, 0);
    }
    cdc_frontend_alloc(line->tokens, 0);
    line->tokens = NULL;
    line->token_count = 0;
}

/* Detaches the accumulated text into an owned token. Returns 0 on
 * allocation failure. */
static int emit_token(tokvec *out, sbuf *text, int col, int raw_end_col,
                      int had_quotes) {
    cdc_token token;
    char *owned;
    if (text->failed || out->failed) {
        return 0;
    }
    owned = cdc_frontend_alloc(NULL, text->len + 1);
    if (!owned) {
        return 0;
    }
    memcpy(owned, text->data ? text->data : "", text->len);
    owned[text->len] = '\0';
    token.text = owned;
    token.length = text->len;
    token.col = col;
    token.raw_len = raw_end_col - col;
    token.had_quotes = had_quotes;
    token.had_equals_quoted = 0;
    if (!tokvec_push(out, token)) {
        cdc_frontend_alloc(owned, 0);
        return 0;
    }
    sbuf_reset(text);
    return 1;
}

int cdc_lex_line(const char *raw, int line_no, const char *file,
                 cdc_line *out, cdc_diag_list *diags) {
    size_t raw_len = strlen(raw);
    size_t start = 0;
    size_t end;
    size_t pos;
    cdc_span span;
    tokvec tokens;
    sbuf text;
    enum { Q_NONE, Q_SINGLE, Q_DOUBLE } quote = Q_NONE;
    int in_token = 0;
    int had_quotes = 0;
    int token_col = 0;
    int oom = 0;

    memset(out, 0, sizeof(*out));
    out->line = line_no;
    span.file = file;
    span.line = line_no;
    span.col = 0;
    span.len = 0;

    if (raw_len > CDC_LEX_MAX_LINE) {
        span.col = 1;
        cdc_diag_add(diags, CDC_DIAG_ERROR, "CDC010", span,
                     "line exceeds %d bytes", (int)CDC_LEX_MAX_LINE);
        out->kind = CDC_LINE_ERROR;
        return 1;
    }

    /* Comment strip: first '#' truncates unconditionally (D5 quirk kept for
     * grammar-0 acceptance parity). */
    end = raw_len;
    {
        const char *hash = memchr(raw, '#', raw_len);
        if (hash) {
            end = (size_t)(hash - raw);
        }
    }
    /* Trim (Python str.strip ASCII behavior on this byte range). */
    while (start < end && is_trim_space(raw[start])) {
        start++;
    }
    while (end > start && is_trim_space(raw[end - 1])) {
        end--;
    }
    if (start == end) {
        out->kind = CDC_LINE_EMPTY;
        return 1;
    }
    if (end - start == 3 && memcmp(raw + start, "end", 3) == 0) {
        out->kind = CDC_LINE_END;
        return 1;
    }

    tokvec_init(&tokens);
    sbuf_init(&text);
    pos = start;
    while (pos < end) {
        char c = raw[pos];
        if (quote == Q_NONE) {
            if (!in_token && is_separator(c)) {
                pos++;
                continue;
            }
            if (!in_token) {
                in_token = 1;
                had_quotes = 0;
                token_col = (int)pos + 1;
            }
            if (is_separator(c)) {
                if (!emit_token(&tokens, &text, token_col, (int)pos + 1,
                                had_quotes)) {
                    oom = 1;
                    break;
                }
                in_token = 0;
                pos++;
                continue;
            }
            if (c == '\\') {
                if (pos + 1 >= end) {
                    span.col = (int)pos + 1;
                    span.len = 1;
                    cdc_diag_add(diags, CDC_DIAG_ERROR, "CDC011", span,
                                 "no escaped character after '\\'");
                    goto lex_error;
                }
                sbuf_push(&text, raw[pos + 1]);
                pos += 2;
                continue;
            }
            if (c == '\'') {
                quote = Q_SINGLE;
                had_quotes = 1;
                pos++;
                continue;
            }
            if (c == '"') {
                quote = Q_DOUBLE;
                had_quotes = 1;
                pos++;
                continue;
            }
            sbuf_push(&text, c);
            pos++;
            continue;
        }
        if (quote == Q_SINGLE) {
            if (c == '\'') {
                quote = Q_NONE;
            } else {
                sbuf_push(&text, c);
            }
            pos++;
            continue;
        }
        /* Q_DOUBLE */
        if (c == '"') {
            quote = Q_NONE;
            pos++;
            continue;
        }
        if (c == '\\') {
            if (pos + 1 >= end) {
                span.col = (int)pos + 1;
                span.len = 1;
                cdc_diag_add(diags, CDC_DIAG_ERROR, "CDC011", span,
                             "no escaped character after '\\'");
                goto lex_error;
            }
            if (raw[pos + 1] == '"' || raw[pos + 1] == '\\') {
                sbuf_push(&text, raw[pos + 1]);
            } else {
                sbuf_push(&text, '\\');
                sbuf_push(&text, raw[pos + 1]);
            }
            pos += 2;
            continue;
        }
        sbuf_push(&text, c);
        pos++;
    }

    if (!oom && quote != Q_NONE) {
        span.col = (int)pos;
        span.len = 1;
        cdc_diag_add(diags, CDC_DIAG_ERROR, "CDC012", span,
                     "no closing quotation");
        goto lex_error;
    }
    if (!oom && in_token) {
        if (!emit_token(&tokens, &text, token_col, (int)pos + 1, had_quotes)) {
            oom = 1;
        }
    }
    if (!oom && tokens.count > CDC_LEX_MAX_TOKENS) {
        span.col = 1;
        cdc_diag_add(diags, CDC_DIAG_ERROR, "CDC013", span,
                     "line exceeds %d tokens", (int)CDC_LEX_MAX_TOKENS);
        goto lex_error;
    }
    if (oom || text.failed || tokens.failed) {
        sbuf_free(&text);
        tokvec_free_deep(&tokens);
        out->kind = CDC_LINE_ERROR;
        return 0;
    }
    sbuf_free(&text);
    out->kind = CDC_LINE_TOKENS;
    out->tokens = tokens.items;
    out->token_count = tokens.count;
    return 1;

lex_error:
    sbuf_free(&text);
    tokvec_free_deep(&tokens);
    out->kind = CDC_LINE_ERROR;
    return 1;
}
