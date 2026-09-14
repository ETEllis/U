#ifndef CDC_DIAGNOSTIC_H
#define CDC_DIAGNOSTIC_H

#include <stddef.h>
#include <stdio.h>

/* Typed, collecting diagnostics for the grammar-1 frontend (Amendment A1).
 * The frontend never exits the process; every failure is a diagnostic with a
 * stable code and a source span, rendered deterministically. */

typedef enum {
    CDC_DIAG_NOTE = 0,
    CDC_DIAG_WARNING = 1,
    CDC_DIAG_ERROR = 2
} cdc_diag_severity;

typedef struct {
    const char *file; /* borrowed; owned by the program that parses */
    int line;         /* 1-based; 0 = whole-file */
    int col;          /* 1-based byte column in the raw line; 0 = whole-line */
    int len;          /* highlighted byte length; 0 = point */
} cdc_span;

typedef struct {
    cdc_diag_severity severity;
    char code[8];      /* stable identifier, e.g. "CDC001" */
    cdc_span span;
    char message[256];
} cdc_diag;

typedef struct {
    cdc_diag *items;
    size_t count;
    size_t capacity;
    size_t errors;
    int out_of_memory; /* set when a diagnostic could not be recorded */
} cdc_diag_list;

void cdc_diag_list_init(cdc_diag_list *list);
void cdc_diag_list_free(cdc_diag_list *list);

/* Appends a diagnostic; on allocation failure sets out_of_memory and keeps
 * the list valid (the failure itself is the recorded condition). */
void cdc_diag_add(cdc_diag_list *list, cdc_diag_severity severity,
                  const char *code, cdc_span span, const char *fmt, ...);

/* Deterministic rendering: "file:line:col: severity[code]: message\n".
 * line/col of 0 render as the file-level form. Returns bytes written. */
size_t cdc_diag_render(const cdc_diag *diag, char *out, size_t out_size);

/* Renders every diagnostic to the given stdio stream. */
void cdc_diag_list_print(const cdc_diag_list *list, FILE *stream);

/* Frontend-wide allocation hook (realloc semantics; size 0 frees). The
 * default is realloc/free. The harness swaps in a failing allocator to prove
 * clean behavior under allocation failure. */
void *cdc_frontend_alloc(void *ptr, size_t size);
void cdc_frontend_set_allocator(void *(*fn)(void *ptr, size_t size));

#endif
