#ifndef CDC_SOURCE_H
#define CDC_SOURCE_H

#include <stddef.h>

/* cdc_source — expectation helpers shared by the C consumers.
 *
 * This file used to also carry the LEGACY LINE SCANNER: cdc_starts_with,
 * cdc_trim, cdc_strip_comment, cdc_first_token_after, and the cdc_read_*
 * attribute readers. Every runtime now parses through the grammar-1
 * frontend (cdc_parser.c / cdc_ast.c), so the scanner had no callers left
 * and has been deleted rather than kept "just in case" — an unused parser
 * for a language that already has one is a second source of truth waiting
 * to disagree with the first.
 *
 * Deletion gate: frontend-differential-dump. The independent oracle for the
 * frontend is `cdc_boot.py --dump` compared against the grammar-1 dump, and
 * that comparison is untouched by this removal. See DECISIONS D26 for why
 * --dump OUTLIVES the scanner rather than retiring with it.
 *
 * What remains is not a parser: these are the checked-expectation
 * primitives the runtimes use to compare a computed value against the value
 * a source declared. */

void cdc_source_fail(const char *message);

int cdc_close_enough(double actual, double expected, double tolerance);
void cdc_expect_string(const char *actual, const char *expected, const char *message);
void cdc_expect_int(int actual, int expected, const char *message);
void cdc_expect_double(double actual, double expected, double tolerance, const char *message);

#endif
