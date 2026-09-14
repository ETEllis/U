#ifndef CDC_PARSER_H
#define CDC_PARSER_H

#include "cdc_ast.h"
#include "cdc_diagnostic.h"

/* Grammar-1 parser (Amendment A1). Accepts exactly the grammar-0 language:
 * the statement stream a valid file produces here corresponds one-to-one to
 * the declarations the legacy loader collects, and every input the legacy
 * loader rejects produces at least one error diagnostic here (differential
 * gate CT1). The parser never exits and never prints; all failures are typed
 * diagnostics. Structural "end" lines and blank/comment lines are
 * represented/skipped exactly as grammar 0 does.
 *
 * Grammar-0 rejection conditions replicated as errors:
 *   CDC011 trailing escape            CDC012 unclosed quote
 *   CDC020 unknown directive          CDC021 missing required first argument
 *   CDC022 duplicate framework key (within the parsed unit)
 * Bounds: CDC010 line too long, CDC013 too many tokens, CDC900 allocation.
 *
 * I/O rejection codes (2026-07-23 adversarial-review contract): a source
 * unit is a readable REGULAR file (or an in-memory buffer). Anything else
 * fails closed with a typed diagnostic and is never accepted as an empty
 * program:
 *   CDC001 cannot open              CDC002 not a regular file
 *   CDC003 read/stat error          CDC004 source exceeds size bound
 * A zero-byte regular file IS a valid unit with zero statements. The size
 * bound is CDC_PARSE_MAX_SOURCE bytes. FIFOs are opened non-blocking and
 * rejected by type without ever blocking or reading.
 */

enum { CDC_PARSE_MAX_SOURCE = 64 << 20 }; /* 64 MiB per source unit */

/* Parses an in-memory buffer. Returns 1 on completion (check diags->errors
 * for acceptance), 0 on allocation failure (program left valid but partial).
 * `file` is recorded for spans and dump records (basename as given). */
int cdc_unit_parse_buffer(const char *buffer, size_t length,
                             const char *file, cdc_unit *out,
                             cdc_diag_list *diags);

/* Reads a stdio stream to EOF and parses it. Distinguishes EOF from read
 * error via ferror(): a read error records CDC003 and returns 0. Enforces
 * CDC_PARSE_MAX_SOURCE during growth (CDC004). Exposed so the test harness
 * can prove the mid-read error path; production callers use
 * cdc_unit_parse_file. */
int cdc_unit_parse_stream(void *stdio_file, const char *path, cdc_unit *out,
                          cdc_diag_list *diags);

/* Opens, validates (regular file, size bound), reads, and parses a file.
 * Returns 1 on completion, 0 with a typed CDC001-CDC004 diagnostic on I/O
 * rejection, or 0 with no I/O diagnostic on allocation failure. */
int cdc_unit_parse_file(const char *path, cdc_unit *out,
                           cdc_diag_list *diags);

#endif
