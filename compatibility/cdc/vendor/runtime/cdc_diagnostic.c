#include "cdc_diagnostic.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* Allocation hook so the harness can inject allocator failure (Amendment
 * Phase B step 5). Default is malloc/realloc; cdc_frontend_set_allocator
 * swaps in a failing allocator for the oom mode. */
static void *(*cdc_alloc_fn)(void *ptr, size_t size) = NULL;

static void *default_alloc(void *ptr, size_t size) {
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, size);
}

void cdc_frontend_set_allocator(void *(*fn)(void *ptr, size_t size));

void cdc_frontend_set_allocator(void *(*fn)(void *ptr, size_t size)) {
    cdc_alloc_fn = fn;
}

void *cdc_frontend_alloc(void *ptr, size_t size);

void *cdc_frontend_alloc(void *ptr, size_t size) {
    return (cdc_alloc_fn ? cdc_alloc_fn : default_alloc)(ptr, size);
}

void cdc_diag_list_init(cdc_diag_list *list) {
    memset(list, 0, sizeof(*list));
}

void cdc_diag_list_free(cdc_diag_list *list) {
    cdc_frontend_alloc(list->items, 0);
    memset(list, 0, sizeof(*list));
}

void cdc_diag_add(cdc_diag_list *list, cdc_diag_severity severity,
                  const char *code, cdc_span span, const char *fmt, ...) {
    cdc_diag *slot;
    va_list ap;
    if (severity == CDC_DIAG_ERROR) {
        list->errors++;
    }
    if (list->count == list->capacity) {
        size_t next = list->capacity ? list->capacity * 2 : 8;
        void *grown = cdc_frontend_alloc(list->items, next * sizeof(cdc_diag));
        if (!grown) {
            list->out_of_memory = 1;
            return;
        }
        list->items = grown;
        list->capacity = next;
    }
    slot = &list->items[list->count++];
    memset(slot, 0, sizeof(*slot));
    slot->severity = severity;
    snprintf(slot->code, sizeof(slot->code), "%s", code ? code : "CDC000");
    slot->span = span;
    va_start(ap, fmt);
    vsnprintf(slot->message, sizeof(slot->message), fmt, ap);
    va_end(ap);
}

static const char *severity_name(cdc_diag_severity severity) {
    switch (severity) {
    case CDC_DIAG_NOTE:
        return "note";
    case CDC_DIAG_WARNING:
        return "warning";
    default:
        return "error";
    }
}

size_t cdc_diag_render(const cdc_diag *diag, char *out, size_t out_size) {
    int written;
    if (diag->span.line > 0 && diag->span.col > 0) {
        written = snprintf(out, out_size, "%s:%d:%d: %s[%s]: %s\n",
                           diag->span.file ? diag->span.file : "<source>",
                           diag->span.line, diag->span.col,
                           severity_name(diag->severity), diag->code,
                           diag->message);
    } else if (diag->span.line > 0) {
        written = snprintf(out, out_size, "%s:%d: %s[%s]: %s\n",
                           diag->span.file ? diag->span.file : "<source>",
                           diag->span.line, severity_name(diag->severity),
                           diag->code, diag->message);
    } else {
        written = snprintf(out, out_size, "%s: %s[%s]: %s\n",
                           diag->span.file ? diag->span.file : "<source>",
                           severity_name(diag->severity), diag->code,
                           diag->message);
    }
    return written < 0 ? 0 : (size_t)written;
}

void cdc_diag_list_print(const cdc_diag_list *list, FILE *stream) {
    size_t i;
    char buffer[512];
    for (i = 0; i < list->count; i++) {
        cdc_diag_render(&list->items[i], buffer, sizeof(buffer));
        fputs(buffer, stream);
    }
    if (list->out_of_memory) {
        fputs("<diagnostics>: error[CDC900]: diagnostic allocation failed\n",
              stream);
    }
}
