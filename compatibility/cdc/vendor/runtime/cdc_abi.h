#ifndef CDC_ABI_H
#define CDC_ABI_H

#include <stddef.h>
#include <stdint.h>

/* cdc_abi — the stable embeddable C boundary (Amendment A2).
 *
 * ABI version: 1.5. cdc_abi_version() returns (major << 16) | minor.
 * Additions bump the minor version; any breaking change bumps the major
 * version and renames nothing silently. The CLI, daemons, SDK bindings, and
 * tests consume this boundary; nothing outside runtime/ may include the
 * internal frontend headers.
 *
 * Ownership and lifetime
 *   Every object returned through an out-parameter is owned by the caller
 *   and released with the matching *_destroy function (bytes with
 *   cdc_bytes_free). Destroy functions accept NULL. Handles are independent:
 *   destroying a cdc_program does not invalidate results derived from it.
 *
 * Thread safety
 *   A cdc_program is immutable after parse; concurrent reads are safe.
 *   cdc_runtime and cdc_result are single-thread objects; guard external
 *   sharing. No global state is mutated by this surface.
 *
 * Determinism
 *   Parsing, diagnostics, canonical serialization, and result serialization
 *   are bit-deterministic for identical inputs on any platform.
 *
 * Allocation and errors
 *   All allocation failures surface as CDC_ERR_MEMORY; no function aborts
 *   the process or prints. Every function returns cdc_status; out-values are
 *   NULL/0 on failure.
 *
 * Availability through ABI 1.5
 *   Parse, diagnostics, canonical bytes, result serialization, registry
 *   loading, and contract verification (cdc_runtime_load +
 *   cdc_runtime_verify — the bootloader-parity report) are fully
 *   implemented. cdc_runtime_execute is declared for ABI shape stability
 *   but returns CDC_ERR_STATE with an explanatory result until the Phase C
 *   execution surface lands; it never partially executes. This is a typed
 *   fail-closed contract, not undefined behavior.
 */

#define CDC_ABI_VERSION_MAJOR 1
#define CDC_ABI_VERSION_MINOR 5

/* ABI 1.3 added typed effect receipts (cdc_receipt.h), the structured
 * record of one executed effect. Consumers that need an outcome read a
 * receipt instead of matching the human report line; the two channels are
 * rendered from one struct and their agreement is gated.
 *
 * ABI 1.4 adds the RFTC C3 authenticated control plane. cdc_supervisor.h is
 * the stable opaque boundary: keyed canonical transport, scoped authority,
 * causal admission, and an all-or-nothing commit callback are serialized
 * behind one supervisor. It is a shared-key integrity/authenticity boundary,
 * not a public-key signature or mTLS claim.
 *
 * ABI 1.5 adds sealed oriented-frame snapshots, witnessed topology
 * transitions, provenance-preserving logical cells, the bounded recursive
 * scheduler, its canonical wire payload, and the composed supervised
 * scheduler admission boundary. It also adds the durable scheduler journal:
 * complete signed envelopes are sealed before admission commits, and fresh
 * schedulers can re-authenticate and deterministically reconstruct exact,
 * uncompacted history after restart. D38 tightens the same ABI: durable journal
 * outcomes retain terminal causal history; replay reapplies peer policy;
 * scheduler-exported configuration-epoch receipts bind recursive structure;
 * and imported cell state is canonically revalidated before atomic migration.
 * These are deterministic classical reference-frame mechanisms; this ABI does
 * not claim a qubit, entanglement, or quantum advantage. */
#include "cdc_receipt.h"
#include "cdc_scheduler.h"
#include "cdc_scheduler_journal.h"
#include "cdc_scheduler_wire.h"
#include "cdc_shared_record.h"
#include "cdc_supervisor.h"
#include "cdc_supervised_scheduler.h"

typedef enum {
    CDC_OK = 0,
    CDC_ERR_ARGUMENT = 1, /* bad parameter combination */
    CDC_ERR_IO = 2,       /* file unreadable */
    CDC_ERR_PARSE = 3,    /* source rejected; diagnostics available */
    CDC_ERR_MEMORY = 4,   /* allocation failure */
    CDC_ERR_STATE = 5,    /* operation not available in this object state */
} cdc_status;

typedef struct cdc_program cdc_program;
typedef struct cdc_runtime cdc_runtime;
typedef struct cdc_result cdc_result;

uint32_t cdc_abi_version(void);
const char *cdc_status_name(cdc_status status);

/* Parses a source unit. Exactly one of `path` and `buffer` must be non-NULL
 * (`length` applies to `buffer`). A handle is returned even when the source
 * is rejected (status CDC_ERR_PARSE) so diagnostics can be read; only
 * CDC_ERR_ARGUMENT / CDC_ERR_IO / CDC_ERR_MEMORY return no handle.
 *
 * I/O contract (2026-07-23 review): a path must name a readable REGULAR
 * file. Directories, FIFOs, and devices return CDC_ERR_IO without blocking
 * and without a handle — never an empty accepted program. Mid-read errors
 * return CDC_ERR_IO; allocation failure is always CDC_ERR_MEMORY, never
 * remapped. A zero-byte regular file is a VALID unit with zero statements
 * (CDC_OK). Sources above the parser bound (64 MiB) return CDC_ERR_IO. */
cdc_status cdc_program_parse(const char *path, const char *buffer,
                             size_t length, cdc_program **out);

/* Number of statements in the parsed unit (0 when rejected early). */
size_t cdc_program_statement_count(const cdc_program *program);

/* Statement introspection (ABI 1.2). Index is 0-based over the statement
 * stream; out-of-range or NULL inputs return NULL. Returned pointers are
 * borrowed and valid until the program is destroyed. Attribute lookup uses
 * grammar-0 dict semantics (last occurrence wins). Structural "end"
 * statements report directive "end" and carry no args/attrs. */
const char *cdc_program_statement_directive(const cdc_program *program,
                                            size_t index);
const char *cdc_program_statement_arg(const cdc_program *program,
                                      size_t index, size_t arg_index);
const char *cdc_program_statement_attr(const cdc_program *program,
                                       size_t index, const char *key);

/* Number of error diagnostics recorded for the unit. */
size_t cdc_program_error_count(const cdc_program *program);

/* Produces the unit's diagnostics as a result object (possibly empty). */
cdc_status cdc_program_diagnostics(const cdc_program *program,
                                   cdc_result **out);

/* Canonical grammar-1 serialization of an accepted unit. Rejected units
 * return CDC_ERR_STATE. Bytes are caller-owned (cdc_bytes_free). */
cdc_status cdc_program_canonical_bytes(const cdc_program *program,
                                       char **out_bytes, size_t *out_length);

cdc_status cdc_runtime_create(cdc_runtime **out);

/* Loads an accepted program into the runtime's contract registry. The
 * runtime TAKES OWNERSHIP of the program (do not destroy it after a
 * successful load; it is released with the runtime). Rejected programs
 * (parse errors) are refused with CDC_ERR_STATE; a load-order contract
 * violation (duplicate framework key across the load) returns
 * CDC_ERR_PARSE and the runtime remains usable. */
cdc_status cdc_runtime_load(cdc_runtime *runtime, cdc_program *program);

/* Evaluates the contract over everything loaded (bootloader-parity report:
 * ordered per-check OK/FAIL records plus summary). When `program` is
 * non-NULL it is loaded first (ownership transfers as in
 * cdc_runtime_load). The report text is available via cdc_result_text;
 * cdc_result_error_count carries the number of failed expectations.
 * Returns CDC_OK even when expectations fail — failure lives in the
 * result; only argument/memory problems return error statuses. */
cdc_status cdc_runtime_verify(cdc_runtime *runtime,
                              const cdc_program *program, cdc_result **out);

/* The ORDERED per-check parity vector for the loaded corpus (interface
 * section 7): one record per check, in evaluation order. Aggregate counts
 * can agree while the checks behind them differ, so conformance compares
 * these record by record. The trace digest chains, so ordering is part of
 * the compared value rather than something a reader has to notice. */
cdc_status cdc_runtime_vectors(cdc_runtime *runtime,
                               const cdc_program *program,
                               cdc_result **out);

/* Declared at ABI 1.0; functional from the Phase C execution surface.
 * Until then returns CDC_ERR_STATE and, when `out` is non-NULL, a result
 * explaining the gap. It never partially executes. */
cdc_status cdc_runtime_execute(cdc_runtime *runtime,
                               const cdc_program *program,
                               const char *entry_job, cdc_result **out);

/* Raw text carried by a result (the verify report), or "" when the result
 * carries none. Borrowed pointer, valid until the result is destroyed. */
const char *cdc_result_text(const cdc_result *result);

/* Number of error entries carried by a result. */
size_t cdc_result_error_count(const cdc_result *result);

/* Serializes a result as deterministic JSON:
 *   {"abi":"1.0","errors":N,"diagnostics":["...", ...]}
 * Bytes are caller-owned (cdc_bytes_free). */
cdc_status cdc_result_serialize(const cdc_result *result, char **out_bytes,
                                size_t *out_length);

void cdc_program_destroy(cdc_program *program);
void cdc_runtime_destroy(cdc_runtime *runtime);
void cdc_result_destroy(cdc_result *result);
void cdc_bytes_free(char *bytes);

#endif
