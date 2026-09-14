#ifndef CDC_STORE_H
#define CDC_STORE_H

#include <stddef.h>
#include <stdint.h>

#include "cdc_digest.h"

/* cdc_store — the generic durable, replayable state substrate (Phase D,
 * gates CT4/MM1 seed). Protocol verbs (amendment Phase D.1):
 *
 *   append replay snapshot rebuild transaction compare-and-set/fence
 *   commit rollback recover compact attest verify
 *
 * All of these verbs are implemented. Their guarantees are runtime-checked
 * per run by permanent counterexamples in scripts/verify.sh, not proved.
 *
 * Durability and integrity contract of the reference backend
 * (2026-07-24 independent review, B1/B2):
 * - The log is append-only: DATA records followed by a SEAL record per
 *   transaction. Replay counts only sealed transactions.
 * - TORN/UNSEALED TAIL vs CORRUPT COMMITTED PREFIX are distinguished and
 *   never conflated. Recovery may truncate ONLY a physically incomplete
 *   final record or a fully valid but unsealed transaction tail
 *   (latch-or-hold: the unfinished transaction never happened). ANY
 *   integrity violation inside a structurally complete record — payload
 *   digest, sequence continuity, seal digest, type, or length — is
 *   CDC_STORE_ECORRUPT: open fails, no handle, and the log bytes are
 *   never mutated. Corrupted evidence is preserved, not repaired.
 * - Framing metadata is TAG-CHECKED before it is trusted (record format
 *   v2, 3122af5 re-review): every record header carries a tag digested
 *   over magic, type, sequence, and length, verified BEFORE the length
 *   field is trusted or any allocation happens; a mutated length can never
 *   masquerade as a torn tail. Records above a documented bound (64 MiB)
 *   fail closed pre-allocation. The tags are UNKEYED digests: they detect
 *   corruption and accidental mutation, not a motivated forger. Keyed
 *   authentication (Ed25519 over the HEAD) is queued, not claimed.
 * - Sequence numbers are monotonic and verified (DATA: global event
 *   ordinal; SEAL: transaction ordinal). Each SEAL digest is recomputed
 *   from its transaction's DATA digests and must match.
 * - open/verify/replay/visit/attest share one typed scan; none can bypass
 *   integrity state. replay/attest operate on the sealed prefix only and
 *   refuse corrupt logs.
 * - Every record carries its payload digest (canonical BLAKE3; the
 *   interim sha256 of D2 was retired when BLAKE3 was vendored).
 * - Commit path: write(all records) -> fflush -> fsync(log fd) ->
 *   fsync(directory fd). The injectable failure hook aborts at each
 *   boundary to prove recovery (cdc_store_set_fail_after).
 * - Writers are serialized across processes AND across handles within
 *   one process (D16, then D29). The critical section — held across
 *   re-scan + append + fsync(file) + fsync(dir), across the whole
 *   compaction transition, across open recovery, and across reset — is
 *   a process-local mutex plus an fcntl write lock on `lock.cdcstore`.
 *   All handles in one process that name the same store (by the lock
 *   file's device+inode) share ONE reference-counted coordination
 *   object carrying ONE fcntl descriptor: the mutex excludes
 *   same-process handles (fcntl alone cannot — the kernel merges a
 *   process's locks), and the shared descriptor lives until the LAST
 *   handle closes, because closing ANY descriptor a process holds on
 *   the lock file would drop EVERY lock the process holds on it. The
 *   fence token is a STALENESS check on top of that mutual exclusion,
 *   not a substitute for it.
 * - A writer that armed a fence and lost the compare-and-set has a SPENT
 *   handle: its cached counters are stale, so it must be reopened rather
 *   than reused.
 * - GENERATIONS. The log begins with a HEAD record carrying the store
 *   uuid, a monotonic generation, the compaction base, and an anchor over
 *   the HEAD it replaced. `snapshot` PREPARES a base in `base.pending`
 *   that open() never reads; `compact` verifies it still belongs to this
 *   store and still covers the current sealed prefix, both under the lock,
 *   then rebuilds the log and activates it with fsync -> rename ->
 *   directory fsync. A crash anywhere in that transition leaves the old
 *   generation or the new one, never a mixture.
 * - Attest covers the HEAD, so two different histories compacted to the
 *   same sealed count attest differently, and two stores with identical
 *   histories still attest differently.
 * - NOT claimed: rollback of an entire log file to a previous generation
 *   is detectable only by an observer who retained the generation
 *   externally. Nothing inside a single directory can distinguish "never
 *   compacted" from "rolled back", and unkeyed digests do not stop an
 *   attacker who can rewrite the whole file. An external anchor plus
 *   Ed25519 signing is the queued repair.
 *
 * Language surface (capability H6, framework_persistence.cdc). This library
 * is not reachable from `.cdc` source as a service call. The `store` and
 * `persist` forms bind to it through cdc_native_runtime.c, and `op=append`
 * runs the IDENTICAL balanced-ternary barrier that governs in-memory
 * latching: only an accepted decision reaches cdc_store_stage/commit. A
 * violated prefix balance stages nothing, so the sealed bytes cannot move —
 * latch-or-hold stated at the language level rather than enforced by
 * convention at the call site.
 */

typedef struct cdc_store cdc_store;

typedef enum {
    CDC_STORE_OK = 0,
    CDC_STORE_EARG = 1,
    CDC_STORE_EIO = 2,
    CDC_STORE_EMEM = 3,
    CDC_STORE_ECORRUPT = 4,     /* committed prefix integrity violation:
                                   fail closed, log NEVER mutated */
    CDC_STORE_ECRASH = 5,       /* injected failure fired (test harness) */
    CDC_STORE_ESTATE = 6,
    CDC_STORE_EUNSUPPORTED = 7, /* exact payload recovery after compaction */
    CDC_STORE_EUNSEALED = 8,    /* verify: valid but unsealed/torn tail
                                   present (recoverable by open) */
} cdc_store_status;

const char *cdc_store_status_name(cdc_store_status status);

/* Opens (creating if needed) the store rooted at `dir`, running recovery:
 * any unsealed or torn tail is truncated and reported via *recovered_out
 * (0 = clean, 1 = a tail was truncated). */
cdc_store_status cdc_store_open(const char *dir, cdc_store **out,
                                int *recovered_out);
void cdc_store_close(cdc_store *store);

/* Number of sealed transactions visible. */
uint64_t cdc_store_sealed_count(const cdc_store *store);

/* Number of sealed events (payload records) visible. */
uint64_t cdc_store_event_count(const cdc_store *store);

/* Monotonic compaction generation carried by the log's HEAD record. 0 is a
 * never-compacted store; each activated compaction advances it by one. */
uint64_t cdc_store_generation(const cdc_store *store);

/* Removes exactly this store's own artifacts — the log, any prepared base
 * (base.pending), and the crash-window temp files — from `dir`, leaving
 * every other path untouched, so a declared store can be opened from a
 * known-empty state. Runs inside the store's critical section, so it
 * cannot interleave with a commit, compaction, or open in any handle or
 * process; a handle whose store was reset under it gets a typed
 * ECORRUPT refusal on its next operation, never a partial view. The lock
 * file is deliberately kept: unlinking a file another process holds a
 * lock on would break serialization. Artifacts that are absent are not an
 * error; the directory itself is never removed. This is the only deletion
 * path in the store and it never widens. */
cdc_store_status cdc_store_reset(const char *dir);

/* Transaction: stage any number of event payloads, then commit (all
 * sealed atomically) or rollback (nothing written). Staging is in-memory;
 * nothing touches the log before commit. */
cdc_store_status cdc_store_stage(cdc_store *store, const void *payload,
                                 size_t size);
cdc_store_status cdc_store_commit(cdc_store *store);
cdc_store_status cdc_store_rollback(cdc_store *store);

/* Replay: folds every sealed event's digest into a deterministic state
 * digest ("blake3:<hex>" written to out), resuming from the compaction
 * base in the HEAD. Identical event history yields an identical state
 * digest on any platform and across compaction. NOTE: compaction keeps a
 * COMMITMENT to the discarded history, not the history itself — after
 * compacting, the events are gone and only this identity survives. */
cdc_store_status cdc_store_replay(cdc_store *store, char *out,
                                  size_t out_size);

/* Exact event recovery. The visitor is called once for every payload in
 * global event-sequence order, with the 1-based transaction sequence that
 * sealed it. The payload pointer is valid only for the duration of the
 * callback. Return CDC_STORE_OK to continue; any other callback status
 * stops recovery and is returned unchanged.
 *
 * This operation holds the store's shared critical section, validates the
 * complete log through the same typed scan used by open/verify/replay, and
 * establishes the sealed byte boundary before invoking the first callback.
 * Therefore a corrupt committed prefix invokes no callbacks, and a valid
 * or physically torn unsealed tail is never visited. The callback must not
 * call any cdc_store function for this store (including close or reset).
 *
 * Compaction deliberately discards historical payload bytes while retaining
 * their replay commitment. If any compacted base events exist, exact recovery
 * is impossible and this function returns CDC_STORE_EUNSUPPORTED without
 * invoking the visitor; it never presents a post-compaction suffix as the
 * complete history. */
typedef cdc_store_status (*cdc_store_event_visitor)(
    void *context, uint64_t event_sequence, uint64_t transaction_sequence,
    const void *payload, size_t payload_size);

cdc_store_status cdc_store_visit_events(cdc_store *store,
                                        cdc_store_event_visitor visitor,
                                        void *context);

/* Attest: digest of the raw sealed log bytes (evidence identity). */
cdc_store_status cdc_store_attest(cdc_store *store, char *out,
                                  size_t out_size);

/* Verify: full structural re-scan of the log (lengths, digests, seals). */
cdc_store_status cdc_store_verify(cdc_store *store);

/* Two-phase compaction. snapshot() prepares a base for generation+1 that
 * open() never trusts; compact() activates it atomically or refuses.
 * compact() returns ESTATE when nothing is prepared, when the prepared base
 * no longer covers the sealed prefix (a transaction landed after it was
 * prepared), or when it targets a generation other than the next one;
 * ECORRUPT when the base belongs to a different store. */
cdc_store_status cdc_store_snapshot(cdc_store *store);
cdc_store_status cdc_store_compact(cdc_store *store);

/* Arms a compare-and-set at the store's current (generation, sealed,
 * replay-state) triple, refusing with ESTATE if `expected_seal` is already
 * stale. The armed token is re-checked under the store lock inside
 * commit(). */
cdc_store_status cdc_store_fence(cdc_store *store, uint64_t expected_seal);

/* Failure injection (crash matrix): abort the commit path after N
 * successful write/flush/sync operations (0 disables). The abort leaves
 * whatever bytes were written — including torn partial records — for
 * recovery to prove the latch-or-hold contract. */
void cdc_store_set_fail_after(cdc_store *store, int operations);

/* Read-fault injection (f1f68c0 re-review): simulate a mid-read I/O
 * error after N successful scan reads (0 disarms; global to the scan
 * path). A faulted read surfaces as CDC_STORE_EIO — never as a torn
 * tail, never truncating, never yielding a handle. Non-regular log
 * paths (directory, FIFO, device) are CDC_STORE_EIO before any byte is
 * interpreted. */
void cdc_store_set_read_fail_after(int operations);

/* Out-of-process fault injection: SIGKILL this process after N commit
 * boundary operations (0 disarms). Unlike the in-process hook, the process
 * genuinely dies, so unflushed stdio buffers are lost the way they are in a
 * real power cut — a distinct loss mode from a torn write. Intended for a
 * forked child; recovery is then asserted by the surviving parent. */
void cdc_store_set_kill_after(cdc_store *store, int operations);

/* Number of write/flush/sync boundary operations a commit of the current
 * staged transaction would perform (for exhaustive injection sweeps). */
int cdc_store_commit_operations(const cdc_store *store);

/* Test-only (simultaneous-handle checks): enter/exit the store's critical
 * section without performing an operation, so a check can HOLD the
 * section deterministically while proving that other handles, opens,
 * resets, and processes are genuinely excluded. All handles in one
 * process that name the same store share one coordination object (mutex +
 * a single fcntl descriptor kept alive until the last close), so this
 * excludes same-process handles as well as other processes. A thread may
 * hold at most one section, must release it from the same thread, and
 * must not call any other store function on that store while holding it.
 * Not part of the store contract; no production caller may use these. */
cdc_store_status cdc_store_lock_test(cdc_store *store);
void cdc_store_unlock_test(cdc_store *store);

/* Test-only (1->0->1 lifecycle check): arms a one-shot pause inside the
 * next last-handle release, at the point where its ordering against new
 * openers matters. The release writes one byte to `signal_fd`, then
 * blocks reading one byte from `wait_fd` before closing the coordination
 * descriptor. Never armed in production. */
void cdc_store_set_release_pause(int signal_fd, int wait_fd);

#endif
