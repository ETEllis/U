#ifndef CDC_REGISTRY_H
#define CDC_REGISTRY_H

#include <stdio.h>

#include "cdc_ast.h"
#include "cdc_diagnostic.h"

/* Grammar-1 registry and contract evaluator: the native mirror of the
 * bootloader's collect + eval_expect + report semantics (cdc_boot.py),
 * byte-identical in report output for valid corpora. This is the engine
 * behind the toolchain-verify-parity deletion gate: verify.sh diffs this
 * report against the bootloader's on every run; when the gate retires, this
 * evaluator is the only contract checker.
 *
 * Semantics mirrored exactly (including Python list-repr formatting in
 * labels): set-uniqueness for terms/rules/provides/bootloader steps;
 * capability attr-merge and witness overwrite; duplicate framework keys
 * rejected across the whole load; witnesses iterated in sorted-id order for
 * framework completeness; expectations evaluated in load order. Malformed
 * expectation forms that crash the bootloader (e.g. a bad integer) are
 * reported as failed checks here — a divergence only reachable on corpora
 * the bootloader cannot process at all. */

typedef struct cdc_registry cdc_registry;

cdc_registry *cdc_registry_create(void);
void cdc_registry_destroy(cdc_registry *registry);

/* Collects one parsed unit into the registry. The registry BORROWS the
 * unit's statements: the unit must outlive the registry. Returns 0 on
 * allocation failure or on a load-order contract violation (duplicate
 * framework key), recording a diagnostic. */
int cdc_registry_load(cdc_registry *registry, const cdc_unit *unit,
                      cdc_diag_list *diags);

/* Evaluates all collected expectations and writes the contract report to
 * `stream` (byte-identical to the bootloader's report for valid corpora).
 * `python_root` is the directory scanned non-recursively for *.py (the
 * python-files/bootloader expectations); pass "." for the repo root.
 * Returns 1 when every expectation passes, 0 otherwise, -1 on allocation
 * failure. Per-check records (ordered) are the report's OK/FAIL lines. */
int cdc_registry_report(cdc_registry *registry, const char *python_root,
                        FILE *stream);

/* Writes the ORDERED per-check parity vector (interface section 7) for the
 * collected expectations: one record per check, in evaluation order.
 *
 *   <file:line> <commit|fail> <coordinate> <effects> <trace> <closure>
 *
 * Aggregate counts can agree while the checks behind them differ, so parity
 * is compared record by record. The trace digest chains over every effects
 * digest so far, which makes ORDERING part of the compared value: a record
 * that moves changes its own trace digest and every one after it.
 *
 * Contract checks carry no bridge coordinate and no closure witness yet;
 * those fields are "-" rather than a placeholder digest that would read as
 * evidence. Returns 1 when every check passed, 0 otherwise, -1 on
 * allocation failure. */
int cdc_registry_vectors(cdc_registry *registry, const char *python_root,
                         FILE *stream);

#endif
