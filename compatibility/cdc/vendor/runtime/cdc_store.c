#define _POSIX_C_SOURCE 200809L

#include "cdc_store.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>

/* Record layout, format v2 (little-endian fixed fields; 3122af5 re-review
 * final repair — framing metadata is AUTHENTICATED before it is trusted):
 *   [4]  magic  "CDC2"
 *   [1]  type   'D' data / 'S' seal
 *   [8]  seq    (data: event ordinal; seal: sealed transaction ordinal)
 *   [4]  payload length (seal: 0)
 *   [32] framing tag: digest over the preceding 17 framing bytes
 *        (magic | type | seq | length). The scanner verifies this tag
 *        BEFORE allocating or reading payload_len, so a mutated length
 *        field can never masquerade as a torn tail.
 *   [32] payload digest (seal: digest of the transaction's event digests)
 *   [n]  payload
 * A transaction = its DATA records followed by one SEAL. Replay/recovery
 * accept only complete, tag-valid, digest-valid, sequence-continuous
 * records and only up to the last SEAL. Records above
 * CDC_STORE_MAX_RECORD fail closed before any allocation. */

static const uint8_t MAGIC[4] = {'C', 'D', 'C', '2'};
enum {
    FRAMING_SIZE = 4 + 1 + 8 + 4,
    OFF_TAG = FRAMING_SIZE,
    OFF_DIGEST = FRAMING_SIZE + CDC_DIGEST_SIZE,
    HEADER_SIZE = FRAMING_SIZE + 2 * CDC_DIGEST_SIZE,
    CDC_STORE_MAX_RECORD = 64 << 20
};

typedef struct {
    uint8_t *payload;
    size_t size;
} staged_event;

/* HEAD record (2026-07-28 review, finding 1). Every log begins with one
 * type-'H' record whose sequence field is the store GENERATION and whose
 * payload is:
 *   [16] store uuid   [8] base sealed   [8] base events
 *   [32] base replay state             [32] prior-generation anchor
 * The base is therefore part of the log itself, not a second file that
 * open() has to trust. Compaction rebuilds the whole log to a temporary
 * file and activates it with fsync -> rename -> directory fsync, so a
 * crash anywhere in the transition leaves either the old generation or the
 * new one and never a mixture. `base.pending` is a PREPARED base written
 * by cdc_store_snapshot that open() never reads; it becomes real only when
 * compaction activates it. */
enum {
    STORE_UUID_SIZE = 16,
    HEAD_BODY = STORE_UUID_SIZE + 8 + 8 + 2 * CDC_DIGEST_SIZE
};

struct cdc_store {
    char dir[512];
    char log_path[600];
    char pending_path[620];
    char lock_path[620];
    uint8_t uuid[STORE_UUID_SIZE];
    uint64_t generation;
    uint8_t prior_anchor[CDC_DIGEST_SIZE];
    uint64_t sealed;      /* sealed transactions visible (base + log) */
    uint64_t events;      /* events in sealed transactions (base + log) */
    long valid_bytes;     /* log byte length covering the sealed prefix */
    /* Compaction base carried by the HEAD record. */
    uint64_t base_sealed;
    uint64_t base_events;
    uint8_t base_state[CDC_DIGEST_SIZE];
    /* Compare-and-set: an armed fence pins the (generation, sealed,
     * replay state) TRIPLE a writer believes it is extending. Sealed count
     * alone is not enough — a compaction can leave it unchanged while the
     * representation underneath it is entirely different. */
    int fence_armed;
    uint64_t fence_generation;
    uint64_t fence_seal;
    uint8_t fence_state[CDC_DIGEST_SIZE];
    /* Serialization: the process-local coordination object shared by
     * every handle in this process that names the same lock file (see the
     * store_coord block below). The critical section is mutex + fcntl,
     * held across re-scan + append + fsync(file) + fsync(dir), across the
     * whole compaction transition, across open recovery, and across
     * reset. */
    struct store_coord *coord;
    staged_event *staged;
    size_t staged_count;
    size_t staged_cap;
    int fail_after;       /* injection: abort after N boundary ops */
    int ops_done;
    int kill_after;       /* injection: SIGKILL after N boundary ops */
};

static int sync_path(const char *path);
static int injected_crash(cdc_store *store);

const char *cdc_store_status_name(cdc_store_status status) {
    switch (status) {
    case CDC_STORE_OK:
        return "ok";
    case CDC_STORE_EARG:
        return "argument";
    case CDC_STORE_EIO:
        return "io";
    case CDC_STORE_EMEM:
        return "memory";
    case CDC_STORE_ECORRUPT:
        return "corrupt-tail";
    case CDC_STORE_ECRASH:
        return "injected-crash";
    case CDC_STORE_ESTATE:
        return "state";
    case CDC_STORE_EUNSUPPORTED:
        return "unsupported";
    case CDC_STORE_EUNSEALED:
        return "unsealed-tail";
    default:
        return "unknown";
    }
}

static void put_u64(uint8_t *out, uint64_t value) {
    int i;
    for (i = 0; i < 8; i++) {
        out[i] = (uint8_t)(value >> (i * 8));
    }
}

static uint64_t get_u64(const uint8_t *in) {
    uint64_t value = 0;
    int i;
    for (i = 0; i < 8; i++) {
        value |= (uint64_t)in[i] << (i * 8);
    }
    return value;
}

static void put_u32(uint8_t *out, uint32_t value) {
    int i;
    for (i = 0; i < 4; i++) {
        out[i] = (uint8_t)(value >> (i * 8));
    }
}

static uint32_t get_u32(const uint8_t *in) {
    uint32_t value = 0;
    int i;
    for (i = 0; i < 4; i++) {
        value |= (uint32_t)in[i] << (i * 8);
    }
    return value;
}

/* One link of the resumable replay chain. */
static void fold_chain(uint8_t chain[CDC_DIGEST_SIZE],
                       const uint8_t record_digest[CDC_DIGEST_SIZE]) {
    cdc_digest_ctx ctx;
    uint8_t next[CDC_DIGEST_SIZE];
    cdc_digest_init(&ctx);
    cdc_digest_update(&ctx, chain, CDC_DIGEST_SIZE);
    cdc_digest_update(&ctx, record_digest, CDC_DIGEST_SIZE);
    cdc_digest_final(&ctx, next);
    memcpy(chain, next, CDC_DIGEST_SIZE);
}

/* Typed scan result shared by open/verify/replay/attest (review B1/B2):
 * SCAN_CLEAN — sealed prefix is the whole file;
 * SCAN_TAIL — a physically incomplete final record or fully valid but
 *             unsealed transaction tail follows the sealed prefix
 *             (recoverable by truncation, latch-or-hold);
 * SCAN_CORRUPT — an integrity violation inside a structurally complete
 *             record (magic, type, length, payload digest, sequence,
 *             or seal digest): fail closed, never mutate. */
typedef enum { SCAN_CLEAN = 0, SCAN_TAIL = 1, SCAN_CORRUPT = 2 } scan_state;

typedef struct {
    uint64_t sealed;
    uint64_t events; /* events within the sealed prefix */
    long valid_bytes;
    scan_state state;
    /* HEAD record contents (the log's own identity and compaction base). */
    int has_head;
    uint8_t uuid[STORE_UUID_SIZE];
    uint64_t generation;
    uint64_t base_sealed;
    uint64_t base_events;
    uint8_t base_state[CDC_DIGEST_SIZE];
    uint8_t prior_anchor[CDC_DIGEST_SIZE];
    /* Chained replay identity: state_0 is the base (all zero, or the state
     * a snapshot recorded), and each sealed record folds in as
     * state_i = digest(state_{i-1} || record_digest_i). The chain is
     * RESUMABLE, so compacting the log away and continuing from a snapshot
     * yields the same replay digest — semantic identity survives a
     * physical rewrite, while the attest digest (raw bytes) legitimately
     * changes. */
    uint8_t replay_state[CDC_DIGEST_SIZE];
} scan_result;

/* Read-fault injection for the scan path (f1f68c0 re-review): simulates a
 * mid-read I/O error after N successful scan reads so the EOF-vs-fault
 * distinction is permanently testable. 0 disarms. */
static int scan_read_fail_at;
static int scan_read_ops;
static int scan_read_injected;

void cdc_store_set_read_fail_after(int operations) {
    scan_read_fail_at = operations;
    scan_read_ops = 0;
    scan_read_injected = 0;
}

static size_t scan_fread(void *buffer, size_t size, FILE *fp) {
    if (scan_read_fail_at > 0) {
        scan_read_ops++;
        if (scan_read_ops >= scan_read_fail_at) {
            scan_read_injected = 1;
            return 0;
        }
    }
    return fread(buffer, 1, size, fp);
}

/* A short or zero read is EOF only when the stream carries no error
 * indicator; a real read fault must surface as CDC_STORE_EIO and may
 * NEVER be classified as a torn tail (which would authorize destructive
 * truncation). */
static int scan_read_faulted(FILE *fp) {
    return ferror(fp) || scan_read_injected;
}

/* Scans a log from its own HEAD record. The compaction base is carried by
 * the log, so no caller can supply a base the log does not itself claim —
 * which is what made a foreign or dangling snapshot dangerous before. */
static cdc_store_status scan_log(const char *path, scan_result *result) {
    FILE *fp = fopen(path, "rb");
    long offset = 0;
    uint64_t expect_event;
    uint64_t expect_seal;
    cdc_digest_ctx txn_ctx;
    int txn_open = 0;
    uint8_t chain[CDC_DIGEST_SIZE];
    uint8_t sealed_chain[CDC_DIGEST_SIZE];

    memset(result, 0, sizeof(*result));
    result->state = SCAN_CLEAN;
    if (!fp) {
        return errno == ENOENT ? CDC_STORE_OK : CDC_STORE_EIO;
    }
    /* The log must be a regular file; directories, FIFOs, and devices are
     * I/O errors before a single byte is interpreted. */
    {
        struct stat st;
        if (fstat(fileno(fp), &st) != 0 || !S_ISREG(st.st_mode)) {
            fclose(fp);
            return CDC_STORE_EIO;
        }
    }
    /* The HEAD record: the log's identity and its compaction base. An
     * empty file is "no store yet" (open creates one); anything present
     * but not a valid HEAD is corruption, because the HEAD is only ever
     * published by an atomic rename and can never be legitimately torn. */
    {
        uint8_t header[HEADER_SIZE];
        uint8_t body[HEAD_BODY];
        uint8_t digest[CDC_DIGEST_SIZE];
        uint8_t tag[CDC_DIGEST_SIZE];
        size_t got = scan_fread(header, sizeof(header), fp);
        if (got < sizeof(header) && scan_read_faulted(fp)) {
            fclose(fp);
            return CDC_STORE_EIO;
        }
        if (got == 0) {
            fclose(fp);
            return CDC_STORE_OK; /* has_head stays 0: nothing here yet */
        }
        if (got < sizeof(header)) {
            fclose(fp);
            result->state = SCAN_CORRUPT;
            return CDC_STORE_OK;
        }
        cdc_digest(header, FRAMING_SIZE, tag);
        if (memcmp(header, MAGIC, sizeof(MAGIC)) != 0 || header[4] != 'H' ||
            memcmp(tag, header + OFF_TAG, CDC_DIGEST_SIZE) != 0 ||
            get_u32(header + 13) != (uint32_t)HEAD_BODY) {
            fclose(fp);
            result->state = SCAN_CORRUPT;
            return CDC_STORE_OK;
        }
        if (scan_fread(body, sizeof(body), fp) != sizeof(body)) {
            if (scan_read_faulted(fp)) {
                fclose(fp);
                return CDC_STORE_EIO;
            }
            fclose(fp);
            result->state = SCAN_CORRUPT;
            return CDC_STORE_OK;
        }
        cdc_digest(body, sizeof(body), digest);
        if (memcmp(digest, header + OFF_DIGEST, CDC_DIGEST_SIZE) != 0) {
            fclose(fp);
            result->state = SCAN_CORRUPT;
            return CDC_STORE_OK;
        }
        result->has_head = 1;
        result->generation = get_u64(header + 5);
        memcpy(result->uuid, body, STORE_UUID_SIZE);
        result->base_sealed = get_u64(body + STORE_UUID_SIZE);
        result->base_events = get_u64(body + STORE_UUID_SIZE + 8);
        memcpy(result->base_state, body + STORE_UUID_SIZE + 16,
               CDC_DIGEST_SIZE);
        memcpy(result->prior_anchor,
               body + STORE_UUID_SIZE + 16 + CDC_DIGEST_SIZE,
               CDC_DIGEST_SIZE);
        offset = (long)(HEADER_SIZE + HEAD_BODY);
    }
    result->sealed = result->base_sealed;
    result->events = result->base_events;
    result->valid_bytes = offset;
    expect_event = result->base_events + 1;
    expect_seal = result->base_sealed + 1;
    memcpy(chain, result->base_state, CDC_DIGEST_SIZE);
    memcpy(sealed_chain, result->base_state, CDC_DIGEST_SIZE);
    memcpy(result->replay_state, result->base_state, CDC_DIGEST_SIZE);
    for (;;) {
        uint8_t header[HEADER_SIZE];
        size_t got = scan_fread(header, sizeof(header), fp);
        uint32_t payload_len;
        uint64_t seq;
        uint8_t type;
        if (got < sizeof(header) && scan_read_faulted(fp)) {
            fclose(fp);
            return CDC_STORE_EIO;
        }
        if (got == 0) {
            break; /* clean record boundary at EOF */
        }
        if (got < sizeof(header)) {
            result->state = SCAN_TAIL; /* physically incomplete header */
            break;
        }
        if (memcmp(header, MAGIC, sizeof(MAGIC)) != 0) {
            result->state = SCAN_CORRUPT;
            break;
        }
        /* Authenticate the framing (magic, type, seq, length) BEFORE
         * trusting payload_len (3122af5 re-review): a complete header
         * whose tag fails is corruption, never a torn tail. */
        {
            uint8_t tag[CDC_DIGEST_SIZE];
            cdc_digest(header, FRAMING_SIZE, tag);
            if (memcmp(tag, header + OFF_TAG, CDC_DIGEST_SIZE) != 0) {
                result->state = SCAN_CORRUPT;
                break;
            }
        }
        type = header[4];
        seq = get_u64(header + 5);
        payload_len = get_u32(header + 13);
        if (payload_len > (uint32_t)CDC_STORE_MAX_RECORD) {
            result->state = SCAN_CORRUPT; /* bound enforced pre-allocation */
            break;
        }
        if (type == 'D') {
            uint8_t *payload = malloc(payload_len ? payload_len : 1);
            uint8_t digest[CDC_DIGEST_SIZE];
            size_t read_len;
            if (!payload) {
                fclose(fp);
                return CDC_STORE_EMEM;
            }
            read_len = scan_fread(payload, payload_len, fp);
            if (read_len != payload_len) {
                free(payload);
                if (scan_read_faulted(fp)) {
                    fclose(fp);
                    return CDC_STORE_EIO;
                }
                result->state = SCAN_TAIL; /* incomplete payload at EOF */
                break;
            }
            cdc_digest(payload, payload_len, digest);
            free(payload);
            if (memcmp(digest, header + OFF_DIGEST, CDC_DIGEST_SIZE) != 0 ||
                seq != expect_event) {
                result->state = SCAN_CORRUPT;
                break;
            }
            expect_event++;
            if (!txn_open) {
                cdc_digest_init(&txn_ctx);
                txn_open = 1;
            }
            cdc_digest_update(&txn_ctx, header + OFF_DIGEST,
                              CDC_DIGEST_SIZE);
            fold_chain(chain, header + OFF_DIGEST);
            offset += HEADER_SIZE + (long)payload_len;
        } else if (type == 'S') {
            uint8_t seal_expected[CDC_DIGEST_SIZE];
            if (payload_len != 0 || seq != expect_seal || !txn_open) {
                result->state = SCAN_CORRUPT;
                break;
            }
            cdc_digest_final(&txn_ctx, seal_expected);
            txn_open = 0;
            if (memcmp(seal_expected, header + OFF_DIGEST,
                       CDC_DIGEST_SIZE) != 0) {
                result->state = SCAN_CORRUPT;
                break;
            }
            expect_seal++;
            offset += HEADER_SIZE;
            result->sealed++;
            result->valid_bytes = offset;
            result->events = expect_event - 1;
            /* the chain advances only at a seal: unsealed work is not
             * part of the replay identity */
            fold_chain(chain, header + OFF_DIGEST);
            memcpy(sealed_chain, chain, CDC_DIGEST_SIZE);
            memcpy(result->replay_state, sealed_chain, CDC_DIGEST_SIZE);
        } else {
            result->state = SCAN_CORRUPT;
            break;
        }
    }
    fclose(fp);
    /* fully valid DATA records after the last seal are an unsealed tail */
    if (result->state == SCAN_CLEAN && offset != result->valid_bytes) {
        result->state = SCAN_TAIL;
    }
    return CDC_STORE_OK;
}

/* fsync a path (file or directory); best effort errors surface as EIO. */
static int sync_path(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    if (fsync(fd) != 0) {
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

/* ---- serialization (2026-07-28 review finding 2; same-application
 *      repair after ca26608) -------------------------------------------
 *
 * commit, compact, open recovery, and reset are check-then-act sequences.
 * Without mutual exclusion two writers can both pass the check before
 * either writes, which is exactly the race the old sequential test could
 * not see.
 *
 * Two levels, because POSIX fcntl record locks have two sharp edges:
 *
 *   1. They serialize PROCESSES only. Two handles inside one process are
 *      invisible to each other — both "acquire" the exclusive lock and
 *      the kernel merges them.
 *   2. They are owned by (process, file), not by descriptor: closing ANY
 *      descriptor the process holds on the lock file releases EVERY lock
 *      the process holds on it. Per-handle descriptors therefore let a
 *      plain cdc_store_close of one handle silently disarm another
 *      handle's exclusion mid-operation.
 *
 * The repair is the standard one: all handles in this process that name
 * the same lock file (by device+inode, resolved under a registry mutex)
 * share ONE reference-counted coordination object carrying ONE fcntl
 * descriptor and one process-local mutex. The mutex serializes handles
 * within the process; the fcntl lock — taken only while the mutex is held
 * — serializes processes; and the shared descriptor is closed only when
 * the LAST handle releases the object, so no close can drop a lock
 * another handle is relying on.
 *
 * Fork: entries are additionally keyed by pid. A forked child never
 * matches an inherited entry (whose mutex may have been copied in the
 * locked state), so it builds a fresh coordination object and contends
 * through fcntl like any other process. Forking while another thread is
 * inside a store call is outside the contract, as it is for POSIX
 * generally. The critical section never spans a public store call, so a
 * thread may hold at most one section at a time and recursion cannot
 * arise.
 *
 * CDC_STORE_TEST_PER_HANDLE_LOCK reproduces the pre-repair behavior —
 * one unregistered coordination object per handle — so the store-samep
 * suite can prove its checks catch the defect. verify.sh compiles that
 * define ONLY for the counterexample probe binary; it is never linked
 * into a shipped tool. */
typedef struct store_coord {
    dev_t dev;
    ino_t ino;
    pid_t pid;
    int lock_fd;
    unsigned refcount;
    pthread_mutex_t mutex;
    struct store_coord *next;
} store_coord;

static pthread_mutex_t coord_registry_lock = PTHREAD_MUTEX_INITIALIZER;
static store_coord *coord_registry = NULL;

#ifndef CDC_STORE_TEST_PER_HANDLE_LOCK
static store_coord *coord_find(dev_t dev, ino_t ino) {
    store_coord *coord;
    for (coord = coord_registry; coord; coord = coord->next) {
        if (coord->dev == dev && coord->ino == ino &&
            coord->pid == getpid()) {
            return coord;
        }
    }
    return NULL;
}
#endif

static cdc_store_status coord_new(int fd, const struct stat *st,
                                  store_coord **out) {
    store_coord *coord = calloc(1, sizeof(*coord));
    if (!coord) {
        return CDC_STORE_EMEM;
    }
    coord->dev = st->st_dev;
    coord->ino = st->st_ino;
    coord->pid = getpid();
    coord->lock_fd = fd;
    coord->refcount = 1;
    if (pthread_mutex_init(&coord->mutex, NULL) != 0) {
        free(coord);
        return CDC_STORE_EIO;
    }
#ifndef CDC_STORE_TEST_PER_HANDLE_LOCK
    coord->next = coord_registry;
    coord_registry = coord;
#endif
    *out = coord;
    return CDC_STORE_OK;
}

/* Resolves the process-shared coordination object for `lock_path`,
 * creating both the lock file and the object as needed. Returns EIO with
 * errno preserved so open-vs-reset can distinguish a missing directory. */
static cdc_store_status coord_acquire(const char *lock_path,
                                      store_coord **out) {
    struct stat st;
#ifndef CDC_STORE_TEST_PER_HANDLE_LOCK
    store_coord *coord;
#endif
    cdc_store_status status;
    int fd;

    *out = NULL;
    pthread_mutex_lock(&coord_registry_lock);
#ifndef CDC_STORE_TEST_PER_HANDLE_LOCK
    /* Common path: the file exists and this process already coordinates
     * on it. Resolved WITHOUT opening a descriptor, because a redundant
     * descriptor is exactly the close-drops-locks hazard. */
    if (stat(lock_path, &st) == 0) {
        coord = coord_find(st.st_dev, st.st_ino);
        if (coord) {
            coord->refcount++;
            pthread_mutex_unlock(&coord_registry_lock);
            *out = coord;
            return CDC_STORE_OK;
        }
    }
#endif
    fd = open(lock_path, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        pthread_mutex_unlock(&coord_registry_lock);
        return CDC_STORE_EIO;
    }
    if (fstat(fd, &st) != 0) {
        pthread_mutex_unlock(&coord_registry_lock);
        close(fd);
        return CDC_STORE_EIO;
    }
#ifndef CDC_STORE_TEST_PER_HANDLE_LOCK
    coord = coord_find(st.st_dev, st.st_ino);
    if (coord) {
        /* Rare: the file was recreated between the stat and the open (an
         * external actor; resets never remove it). The fresh descriptor
         * is redundant, and closing it is only safe while no lock is held
         * on the file — take the section mutex first: fcntl locks are
         * held ONLY inside it, so inside it this process holds none. */
        coord->refcount++;
        pthread_mutex_unlock(&coord_registry_lock);
        pthread_mutex_lock(&coord->mutex);
        close(fd);
        pthread_mutex_unlock(&coord->mutex);
        *out = coord;
        return CDC_STORE_OK;
    }
#endif
    status = coord_new(fd, &st, out);
    pthread_mutex_unlock(&coord_registry_lock);
    if (status != CDC_STORE_OK) {
        close(fd);
    }
    return status;
}

/* Test-only pause point for the 1->0->1 lifecycle check: the last release
 * signals `signal_fd` and blocks on `wait_fd` at the moment its ordering
 * matters, so the check can hold the release open while a new first
 * opener and a foreign contender attempt entry. One-shot; never armed in
 * production. */
static int release_pause_signal_fd = -1;
static int release_pause_wait_fd = -1;

void cdc_store_set_release_pause(int signal_fd, int wait_fd) {
    release_pause_signal_fd = signal_fd;
    release_pause_wait_fd = wait_fd;
}

static void release_pause(void) {
    int signal_fd = release_pause_signal_fd;
    int wait_fd = release_pause_wait_fd;
    char byte = 'p';
    ssize_t n;
    if (signal_fd < 0) {
        return;
    }
    release_pause_signal_fd = -1;
    release_pause_wait_fd = -1;
    do {
        n = write(signal_fd, &byte, 1);
    } while (n < 0 && errno == EINTR);
    do {
        n = read(wait_fd, &byte, 1);
    } while (n < 0 && errno == EINTR);
}

static void coord_release(store_coord *coord) {
    store_coord **link;
    if (!coord) {
        return;
    }
    pthread_mutex_lock(&coord_registry_lock);
    if (--coord->refcount > 0) {
        pthread_mutex_unlock(&coord_registry_lock);
        return;
    }
    for (link = &coord_registry; *link; link = &(*link)->next) {
        if (*link == coord) {
            *link = coord->next;
            break;
        }
    }
#ifdef CDC_STORE_TEST_RELEASE_WINDOW
    /* COUNTEREXAMPLE ORDERING (second 2026-07-28 review, finding 2): the
     * registry lock is released BEFORE the descriptor closes. In that
     * window a new first opener registers a fresh object and takes the
     * process fcntl lock through a NEW descriptor to the same file — and
     * this close then drops it, because POSIX owns record locks by
     * (process, file), not by descriptor. verify.sh compiles this define
     * ONLY for the probe binary and requires the lifecycle check to
     * catch it. */
    pthread_mutex_unlock(&coord_registry_lock);
    release_pause();
    pthread_mutex_destroy(&coord->mutex);
    close(coord->lock_fd);
    free(coord);
#else
    /* Last handle in this process: the descriptor is closed while the
     * registry lock is STILL HELD, so no new opener can register a
     * replacement object (and lock the file through a new descriptor)
     * before this close lands — closing here can drop only locks that no
     * longer exist. Closing inside the critical section is outside the
     * contract, so no lock is held through this descriptor either. */
    release_pause();
    pthread_mutex_destroy(&coord->mutex);
    close(coord->lock_fd);
    free(coord);
    pthread_mutex_unlock(&coord_registry_lock);
#endif
}

static cdc_store_status coord_enter(store_coord *coord) {
    struct flock lock;
    if (!coord || coord->lock_fd < 0) {
        return CDC_STORE_EIO;
    }
    pthread_mutex_lock(&coord->mutex);
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    while (fcntl(coord->lock_fd, F_SETLKW, &lock) != 0) {
        if (errno != EINTR) {
            pthread_mutex_unlock(&coord->mutex);
            return CDC_STORE_EIO;
        }
    }
    return CDC_STORE_OK;
}

static void coord_exit(store_coord *coord) {
    struct flock lock;
    if (!coord || coord->lock_fd < 0) {
        return;
    }
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    while (fcntl(coord->lock_fd, F_SETLK, &lock) != 0 && errno == EINTR) {
        /* retry */
    }
    pthread_mutex_unlock(&coord->mutex);
}

static cdc_store_status store_lock(cdc_store *store) {
    return coord_enter(store->coord);
}

static void store_unlock(cdc_store *store) {
    coord_exit(store->coord);
}

cdc_store_status cdc_store_lock_test(cdc_store *store) {
    if (!store) {
        return CDC_STORE_EARG;
    }
    return store_lock(store);
}

void cdc_store_unlock_test(cdc_store *store) {
    if (store) {
        store_unlock(store);
    }
}

/* ---- generation records ---------------------------------------------- */

static cdc_store_status fill_uuid(uint8_t uuid[STORE_UUID_SIZE]) {
    FILE *fp = fopen("/dev/urandom", "rb");
    if (!fp) {
        return CDC_STORE_EIO;
    }
    if (fread(uuid, 1, STORE_UUID_SIZE, fp) != STORE_UUID_SIZE) {
        fclose(fp);
        return CDC_STORE_EIO;
    }
    fclose(fp);
    return CDC_STORE_OK;
}

static void build_head_body(uint8_t body[HEAD_BODY],
                            const uint8_t uuid[STORE_UUID_SIZE],
                            uint64_t base_sealed, uint64_t base_events,
                            const uint8_t base_state[CDC_DIGEST_SIZE],
                            const uint8_t prior_anchor[CDC_DIGEST_SIZE]) {
    memcpy(body, uuid, STORE_UUID_SIZE);
    put_u64(body + STORE_UUID_SIZE, base_sealed);
    put_u64(body + STORE_UUID_SIZE + 8, base_events);
    memcpy(body + STORE_UUID_SIZE + 16, base_state, CDC_DIGEST_SIZE);
    memcpy(body + STORE_UUID_SIZE + 16 + CDC_DIGEST_SIZE, prior_anchor,
           CDC_DIGEST_SIZE);
}

static void build_head_record(uint8_t record[HEADER_SIZE + HEAD_BODY],
                              uint64_t generation,
                              const uint8_t body[HEAD_BODY]) {
    memcpy(record, MAGIC, sizeof(MAGIC));
    record[4] = 'H';
    put_u64(record + 5, generation);
    put_u32(record + 13, (uint32_t)HEAD_BODY);
    cdc_digest(record, FRAMING_SIZE, record + OFF_TAG);
    cdc_digest(body, HEAD_BODY, record + OFF_DIGEST);
    memcpy(record + HEADER_SIZE, body, HEAD_BODY);
}

/* Publishes `bytes` as the log in ONE atomic step: write a temporary file,
 * make it durable, rename it over the log, then make the directory entry
 * durable. A crash at any point leaves either the previous log or the new
 * one — never a mixture, and never a store that cannot be opened. */
static cdc_store_status publish_log(cdc_store *store, const uint8_t *bytes,
                                    size_t size) {
    char temp_path[640];
    FILE *fp;
    int written = snprintf(temp_path, sizeof(temp_path), "%s.next",
                           store->log_path);
    if (written < 0 || (size_t)written >= sizeof(temp_path)) {
        return CDC_STORE_EARG;
    }
    fp = fopen(temp_path, "wb");
    if (!fp) {
        return CDC_STORE_EIO;
    }
    if (injected_crash(store)) {
        fclose(fp); /* before the new generation has any content */
        return CDC_STORE_ECRASH;
    }
    if ((size > 0 && fwrite(bytes, 1, size, fp) != size) ||
        fflush(fp) != 0) {
        fclose(fp);
        unlink(temp_path);
        return CDC_STORE_EIO;
    }
    if (injected_crash(store)) {
        fclose(fp); /* written but not yet durable */
        return CDC_STORE_ECRASH;
    }
    if (fsync(fileno(fp)) != 0) {
        fclose(fp);
        unlink(temp_path);
        return CDC_STORE_EIO;
    }
    fclose(fp);
    if (injected_crash(store)) {
        return CDC_STORE_ECRASH; /* durable, but not yet activated */
    }
    if (rename(temp_path, store->log_path) != 0) {
        unlink(temp_path);
        return CDC_STORE_EIO;
    }
    if (injected_crash(store)) {
        return CDC_STORE_ECRASH; /* activated, directory entry not durable */
    }
    if (sync_path(store->dir) != 0) {
        return CDC_STORE_EIO;
    }
    return CDC_STORE_OK;
}

cdc_store_status cdc_store_open(const char *dir, cdc_store **out,
                                int *recovered_out) {
    cdc_store *store;
    cdc_store_status status;
    scan_result scan;
    int written;

    if (!dir || !out) {
        return CDC_STORE_EARG;
    }
    *out = NULL;
    if (recovered_out) {
        *recovered_out = 0;
    }
    if (mkdir(dir, 0777) != 0 && errno != EEXIST) {
        return CDC_STORE_EIO;
    }
    store = calloc(1, sizeof(*store));
    if (!store) {
        return CDC_STORE_EMEM;
    }
    written = snprintf(store->dir, sizeof(store->dir), "%s", dir);
    if (written < 0 || (size_t)written >= sizeof(store->dir)) {
        free(store);
        return CDC_STORE_EARG;
    }
    written = snprintf(store->log_path, sizeof(store->log_path),
                       "%s/log.cdcstore", dir);
    if (written < 0 || (size_t)written >= sizeof(store->log_path)) {
        free(store);
        return CDC_STORE_EARG;
    }
    written = snprintf(store->pending_path, sizeof(store->pending_path),
                       "%s/base.pending", dir);
    if (written < 0 || (size_t)written >= sizeof(store->pending_path)) {
        free(store);
        return CDC_STORE_EARG;
    }
    written = snprintf(store->lock_path, sizeof(store->lock_path),
                       "%s/lock.cdcstore", dir);
    if (written < 0 || (size_t)written >= sizeof(store->lock_path)) {
        free(store);
        return CDC_STORE_EARG;
    }
    status = coord_acquire(store->lock_path, &store->coord);
    if (status != CDC_STORE_OK) {
        free(store);
        return status;
    }
    /* The whole open — scan, first-generation creation, tail recovery,
     * position adoption — is ONE critical section. Two simultaneous
     * openers (same process or not) otherwise both scan the same torn
     * tail and both truncate from stale offsets (same-application repair
     * after ca26608: open recovery is a check-then-act sequence too). */
    if (store_lock(store) != CDC_STORE_OK) {
        coord_release(store->coord);
        free(store);
        return CDC_STORE_EIO;
    }
    status = scan_log(store->log_path, &scan);
    if (status != CDC_STORE_OK) {
        store_unlock(store);
        coord_release(store->coord);
        free(store);
        return status;
    }
    if (scan.state == SCAN_CORRUPT) {
        /* Committed-prefix integrity violation: fail closed, preserve the
         * evidence bytes exactly as found (review B1). */
        store_unlock(store);
        coord_release(store->coord);
        free(store);
        return CDC_STORE_ECORRUPT;
    }
    if (!scan.has_head) {
        /* A directory with no log yet: publish generation 0 atomically, so
         * a store either exists completely or not at all. */
        uint8_t record[HEADER_SIZE + HEAD_BODY];
        uint8_t body[HEAD_BODY];
        uint8_t zero[CDC_DIGEST_SIZE];
        cdc_store_status created;
        memset(zero, 0, sizeof(zero));
        created = fill_uuid(store->uuid);
        if (created != CDC_STORE_OK) {
            store_unlock(store);
            coord_release(store->coord);
            free(store);
            return created;
        }
        build_head_body(body, store->uuid, 0, 0, zero, zero);
        build_head_record(record, 0, body);
        created = publish_log(store, record, sizeof(record));
        if (created != CDC_STORE_OK) {
            store_unlock(store);
            coord_release(store->coord);
            free(store);
            return created;
        }
        status = scan_log(store->log_path, &scan);
        if (status != CDC_STORE_OK || !scan.has_head) {
            store_unlock(store);
            coord_release(store->coord);
            free(store);
            return status == CDC_STORE_OK ? CDC_STORE_EIO : status;
        }
    }
    if (scan.state == SCAN_TAIL) {
        /* Latch-or-hold recovery: only a physically incomplete final
         * record or a valid-but-unsealed transaction tail is discarded.
         * The truncation itself is made durable before recovery is
         * reported (review secondary hardening). */
        if (truncate(store->log_path, scan.valid_bytes) != 0 ||
            sync_path(store->log_path) != 0 ||
            sync_path(store->dir) != 0) {
            store_unlock(store);
            coord_release(store->coord);
            free(store);
            return CDC_STORE_EIO;
        }
        if (recovered_out) {
            *recovered_out = 1;
        }
    }
    memcpy(store->uuid, scan.uuid, STORE_UUID_SIZE);
    store->generation = scan.generation;
    store->base_sealed = scan.base_sealed;
    store->base_events = scan.base_events;
    memcpy(store->base_state, scan.base_state, CDC_DIGEST_SIZE);
    memcpy(store->prior_anchor, scan.prior_anchor, CDC_DIGEST_SIZE);
    store->sealed = scan.sealed;
    store->events = scan.events;
    store->valid_bytes = scan.valid_bytes;
    store_unlock(store);
    *out = store;
    return CDC_STORE_OK;
}

void cdc_store_close(cdc_store *store) {
    size_t i;
    if (!store) {
        return;
    }
    for (i = 0; i < store->staged_count; i++) {
        free(store->staged[i].payload);
    }
    /* Never close a descriptor here: the coordination object owns the one
     * fcntl descriptor for this store in this process, and it survives
     * until the LAST handle releases it — otherwise this close would drop
     * every lock the process holds on the lock file (POSIX owns record
     * locks by process+file, not by descriptor). */
    coord_release(store->coord);
    free(store->staged);
    free(store);
}

uint64_t cdc_store_generation(const cdc_store *store) {
    return store ? store->generation : 0;
}

uint64_t cdc_store_sealed_count(const cdc_store *store) {
    return store ? store->sealed : 0;
}

uint64_t cdc_store_event_count(const cdc_store *store) {
    return store ? store->events : 0;
}

cdc_store_status cdc_store_reset(const char *dir) {
    char path[640];
    size_t i;
    /* The store's own artifacts as of the generation redesign (D16): the
     * log, the prepared-but-unactivated base, and the crash-window temp
     * files publish_log and snapshot may leave behind. This list drifted
     * once already — it still named the pre-D16 "snapshot.cdcstore" and
     * missed "base.pending", so a crash between snapshot and compact left
     * a stale foreign base that survived a mode=fresh reset and turned the
     * next early compact into ECORRUPT (D27). The lock file is deliberately
     * NOT removed: unlinking a file another process holds a lock on would
     * leave that process serializing on an orphan while new openers lock a
     * fresh file — two writers, each "holding the lock". */
    static const char *const ARTIFACTS[] = {"log.cdcstore",
                                            "log.cdcstore.next",
                                            "base.pending",
                                            "base.pending.tmp"};
    char lock_path[640];
    store_coord *coord = NULL;
    cdc_store_status status = CDC_STORE_OK;
    int written;

    if (!dir) {
        return CDC_STORE_EARG;
    }
    written = snprintf(lock_path, sizeof(lock_path), "%s/lock.cdcstore",
                       dir);
    if (written < 0 || (size_t)written >= sizeof(lock_path)) {
        return CDC_STORE_EARG;
    }
    /* Reset is a deletion racing every other operation, so it runs inside
     * the same critical section they do (same-application repair after
     * ca26608). A directory that does not exist cannot hold artifacts or
     * writers: nothing to delete, nothing to serialize against. */
    if (coord_acquire(lock_path, &coord) != CDC_STORE_OK) {
        struct stat st;
        if (stat(dir, &st) != 0 && errno == ENOENT) {
            return CDC_STORE_OK;
        }
        return CDC_STORE_EIO;
    }
    if (coord_enter(coord) != CDC_STORE_OK) {
        coord_release(coord);
        return CDC_STORE_EIO;
    }
    for (i = 0; i < sizeof(ARTIFACTS) / sizeof(ARTIFACTS[0]); i++) {
        written = snprintf(path, sizeof(path), "%s/%s", dir, ARTIFACTS[i]);
        if (written < 0 || (size_t)written >= sizeof(path)) {
            status = CDC_STORE_EARG;
            break;
        }
        if (unlink(path) != 0 && errno != ENOENT) {
            status = CDC_STORE_EIO;
            break;
        }
    }
    coord_exit(coord);
    coord_release(coord);
    return status;
}

cdc_store_status cdc_store_stage(cdc_store *store, const void *payload,
                                 size_t size) {
    staged_event event;
    if (!store || (!payload && size > 0)) {
        return CDC_STORE_EARG;
    }
    if (size > (size_t)CDC_STORE_MAX_RECORD) {
        return CDC_STORE_EARG; /* documented per-record bound */
    }
    if (store->staged_count == store->staged_cap) {
        size_t next = store->staged_cap ? store->staged_cap * 2 : 8;
        void *grown = realloc(store->staged, next * sizeof(staged_event));
        if (!grown) {
            return CDC_STORE_EMEM;
        }
        store->staged = grown;
        store->staged_cap = next;
    }
    event.payload = malloc(size ? size : 1);
    if (!event.payload) {
        return CDC_STORE_EMEM;
    }
    memcpy(event.payload, payload, size);
    event.size = size;
    store->staged[store->staged_count++] = event;
    return CDC_STORE_OK;
}

cdc_store_status cdc_store_rollback(cdc_store *store) {
    size_t i;
    if (!store) {
        return CDC_STORE_EARG;
    }
    for (i = 0; i < store->staged_count; i++) {
        free(store->staged[i].payload);
    }
    store->staged_count = 0;
    return CDC_STORE_OK;
}

/* boundary-op accounting for the crash matrix: each record write is one
 * op; fflush, fsync(log), and fsync(dir) are one op each. */
int cdc_store_commit_operations(const cdc_store *store) {
    if (!store) {
        return 0;
    }
    return (int)store->staged_count + 1 /* seal */ + 3 /* flush+2 sync */;
}

static int injected_crash(cdc_store *store) {
    if (store->kill_after > 0) {
        store->ops_done++;
        if (store->ops_done >= store->kill_after) {
            /* Real process death: unflushed stdio buffers are lost exactly
             * as they would be in a power cut. Nothing after this runs. */
            raise(SIGKILL);
        }
        return 0;
    }
    if (store->fail_after <= 0) {
        return 0;
    }
    store->ops_done++;
    return store->ops_done >= store->fail_after;
}

void cdc_store_set_kill_after(cdc_store *store, int operations) {
    if (store) {
        store->kill_after = operations;
        store->ops_done = 0;
    }
}

void cdc_store_set_fail_after(cdc_store *store, int operations) {
    if (store) {
        store->fail_after = operations;
        store->ops_done = 0;
    }
}

/* Writes one record; when the injected crash fires, writes only a torn
 * prefix of it (header + half the payload) to simulate a mid-write power
 * cut, then reports the crash. */
static cdc_store_status write_record(cdc_store *store, FILE *fp,
                                     uint8_t type, uint64_t seq,
                                     const uint8_t *payload,
                                     uint32_t payload_len,
                                     const uint8_t digest[CDC_DIGEST_SIZE]) {
    uint8_t header[HEADER_SIZE];
    int crash = injected_crash(store);
    memcpy(header, MAGIC, sizeof(MAGIC));
    header[4] = type;
    put_u64(header + 5, seq);
    put_u32(header + 13, payload_len);
    cdc_digest(header, FRAMING_SIZE, header + OFF_TAG);
    memcpy(header + OFF_DIGEST, digest, CDC_DIGEST_SIZE);
    if (crash) {
        size_t torn = payload_len / 2;
        fwrite(header, 1, sizeof(header), fp);
        if (torn > 0) {
            fwrite(payload, 1, torn, fp);
        }
        fflush(fp);
        return CDC_STORE_ECRASH;
    }
    if (fwrite(header, 1, sizeof(header), fp) != sizeof(header)) {
        return CDC_STORE_EIO;
    }
    if (payload_len > 0 &&
        fwrite(payload, 1, payload_len, fp) != payload_len) {
        return CDC_STORE_EIO;
    }
    return CDC_STORE_OK;
}

cdc_store_status cdc_store_commit(cdc_store *store) {
    FILE *fp;
    size_t i;
    cdc_digest_ctx seal_ctx;
    uint8_t seal_digest[CDC_DIGEST_SIZE];
    cdc_store_status status = CDC_STORE_OK;
    int dir_fd;

    if (!store) {
        return CDC_STORE_EARG;
    }
    if (store->staged_count == 0) {
        return CDC_STORE_ESTATE;
    }
    /* Everything from here to the directory fsync happens under the store
     * lock, so the compare-and-set below is genuinely atomic with respect
     * to other processes rather than a check the winner can outrun. */
    if (store_lock(store) != CDC_STORE_OK) {
        return CDC_STORE_EIO;
    }
    {
        scan_result now;
        if (scan_log(store->log_path, &now) != CDC_STORE_OK) {
            store_unlock(store);
            return CDC_STORE_EIO;
        }
        if (now.state == SCAN_CORRUPT || !now.has_head) {
            store_unlock(store);
            return CDC_STORE_ECORRUPT;
        }
        /* The writer's own view must still be current even without a
         * fence: a compaction under it changes the sequence space, so
         * appending from stale counters would forge a broken chain. */
        if (now.generation != store->generation) {
            store_unlock(store);
            store->fence_armed = 0;
            return CDC_STORE_ESTATE;
        }
        if (store->fence_armed &&
            (now.generation != store->fence_generation ||
             now.sealed != store->fence_seal ||
             memcmp(now.replay_state, store->fence_state, CDC_DIGEST_SIZE) !=
                 0)) {
            store->fence_armed = 0;
            store_unlock(store);
            return CDC_STORE_ESTATE; /* stale writer */
        }
        /* Adopt the on-disk position: another process may legitimately
         * have appended since this handle last looked. */
        store->sealed = now.sealed;
        store->events = now.events;
        store->valid_bytes = now.valid_bytes;
    }
    fp = fopen(store->log_path, "ab");
    if (!fp) {
        store_unlock(store);
        return CDC_STORE_EIO;
    }
    cdc_digest_init(&seal_ctx);
    for (i = 0; i < store->staged_count && status == CDC_STORE_OK; i++) {
        uint8_t digest[CDC_DIGEST_SIZE];
        cdc_digest(store->staged[i].payload, store->staged[i].size, digest);
        cdc_digest_update(&seal_ctx, digest, sizeof(digest));
        status = write_record(store, fp, 'D', store->events + i + 1,
                              store->staged[i].payload,
                              (uint32_t)store->staged[i].size, digest);
    }
    if (status == CDC_STORE_OK) {
        cdc_digest_final(&seal_ctx, seal_digest);
        status = write_record(store, fp, 'S', store->sealed + 1, NULL, 0,
                              seal_digest);
    }
    if (status == CDC_STORE_OK) {
        if (injected_crash(store)) {
            status = CDC_STORE_ECRASH; /* after writes, before flush */
        } else if (fflush(fp) != 0) {
            status = CDC_STORE_EIO;
        }
    }
    if (status == CDC_STORE_OK) {
        if (injected_crash(store)) {
            status = CDC_STORE_ECRASH; /* after flush, before fsync */
        } else if (fsync(fileno(fp)) != 0) {
            status = CDC_STORE_EIO;
        }
    }
    fclose(fp);
    if (status == CDC_STORE_OK) {
        if (injected_crash(store)) {
            status = CDC_STORE_ECRASH; /* after log sync, before dir sync */
        } else {
            dir_fd = open(store->dir, O_RDONLY);
            if (dir_fd < 0 || fsync(dir_fd) != 0) {
                if (dir_fd >= 0) {
                    close(dir_fd);
                }
                status = CDC_STORE_EIO;
            } else {
                close(dir_fd);
            }
        }
    }
    if (status == CDC_STORE_OK) {
        store->events += store->staged_count;
        store->sealed += 1;
        /* Re-arm the fence at the state this commit just produced, so a
         * writer that keeps committing stays fenced against everyone else
         * without having to re-fence by hand. */
        if (store->fence_armed) {
            scan_result now;
            if (scan_log(store->log_path, &now) == CDC_STORE_OK &&
                now.state != SCAN_CORRUPT && now.has_head) {
                store->fence_generation = now.generation;
                store->fence_seal = now.sealed;
                memcpy(store->fence_state, now.replay_state,
                       CDC_DIGEST_SIZE);
            } else {
                store->fence_armed = 0;
            }
        }
        store->valid_bytes = -1; /* recomputed on next open/verify */
        cdc_store_rollback(store);
        store_unlock(store);
        return CDC_STORE_OK;
    }
    /* crash or error: staged events remain staged; the on-disk tail (if
     * any) is unsealed and will be truncated by recovery. */
    store_unlock(store);
    return status;
}

cdc_store_status cdc_store_replay(cdc_store *store, char *out,
                                  size_t out_size) {
    scan_result scan;

    if (!store || !out) {
        return CDC_STORE_EARG;
    }
    /* Replay identity is the chained state the scan produced, resumed from
     * the compaction base. It therefore depends on the sealed history, not
     * on how that history is currently laid out on disk — which is exactly
     * why compaction preserves it while the attest (raw-bytes) digest
     * legitimately changes. */
    if (scan_log(store->log_path, &scan) != CDC_STORE_OK) {
        return CDC_STORE_EIO;
    }
    if (scan.state == SCAN_CORRUPT) {
        return CDC_STORE_ECORRUPT; /* corrupt evidence is never replayed */
    }
    cdc_digest_hex(scan.replay_state, out, out_size);
    return CDC_STORE_OK;
}

/* Second half of exact recovery. scan_log() has already authenticated the
 * entire file and fixed valid_bytes at its last SEAL while the shared store
 * lock is held. This pass re-checks framing, payload digests, sequences, and
 * transaction seals as it supplies the retained payload bytes. */
static cdc_store_status visit_sealed_prefix(
    const char *path, long valid_bytes, uint64_t expected_events,
    uint64_t expected_transactions, cdc_store_event_visitor visitor,
    void *context) {
    FILE *fp;
    uint8_t head[HEADER_SIZE + HEAD_BODY];
    long offset = (long)sizeof(head);
    uint64_t event_sequence = 1;
    uint64_t transaction_sequence = 1;
    uint64_t visited = 0;
    uint64_t sealed = 0;
    cdc_digest_ctx txn_ctx;
    int txn_open = 0;
    cdc_store_status status = CDC_STORE_OK;

    fp = fopen(path, "rb");
    if (!fp) {
        return CDC_STORE_EIO;
    }
    if (fread(head, 1, sizeof(head), fp) != sizeof(head)) {
        status = ferror(fp) ? CDC_STORE_EIO : CDC_STORE_ECORRUPT;
        goto done;
    }
    while (offset < valid_bytes) {
        uint8_t header[HEADER_SIZE];
        uint8_t tag[CDC_DIGEST_SIZE];
        uint8_t type;
        uint64_t sequence;
        uint32_t payload_len;

        if (valid_bytes - offset < HEADER_SIZE ||
            fread(header, 1, sizeof(header), fp) != sizeof(header)) {
            status = ferror(fp) ? CDC_STORE_EIO : CDC_STORE_ECORRUPT;
            goto done;
        }
        cdc_digest(header, FRAMING_SIZE, tag);
        if (memcmp(header, MAGIC, sizeof(MAGIC)) != 0 ||
            memcmp(tag, header + OFF_TAG, CDC_DIGEST_SIZE) != 0) {
            status = CDC_STORE_ECORRUPT;
            goto done;
        }
        type = header[4];
        sequence = get_u64(header + 5);
        payload_len = get_u32(header + 13);
        if (payload_len > (uint32_t)CDC_STORE_MAX_RECORD) {
            status = CDC_STORE_ECORRUPT;
            goto done;
        }
        if (type == 'D') {
            uint8_t *payload;
            uint8_t digest[CDC_DIGEST_SIZE];

            if (sequence != event_sequence ||
                (long)payload_len > valid_bytes - offset - HEADER_SIZE) {
                status = CDC_STORE_ECORRUPT;
                goto done;
            }
            payload = malloc(payload_len ? payload_len : 1);
            if (!payload) {
                status = CDC_STORE_EMEM;
                goto done;
            }
            if (payload_len > 0 &&
                fread(payload, 1, payload_len, fp) != payload_len) {
                status = ferror(fp) ? CDC_STORE_EIO : CDC_STORE_ECORRUPT;
                free(payload);
                goto done;
            }
            cdc_digest(payload, payload_len, digest);
            if (memcmp(digest, header + OFF_DIGEST, CDC_DIGEST_SIZE) != 0) {
                free(payload);
                status = CDC_STORE_ECORRUPT;
                goto done;
            }
            if (!txn_open) {
                cdc_digest_init(&txn_ctx);
                txn_open = 1;
            }
            cdc_digest_update(&txn_ctx, header + OFF_DIGEST,
                              CDC_DIGEST_SIZE);
            status = visitor(context, event_sequence, transaction_sequence,
                             payload, payload_len);
            free(payload);
            if (status != CDC_STORE_OK) {
                goto done;
            }
            event_sequence++;
            visited++;
            offset += HEADER_SIZE + (long)payload_len;
        } else if (type == 'S') {
            uint8_t seal_expected[CDC_DIGEST_SIZE];

            if (payload_len != 0 || sequence != transaction_sequence ||
                !txn_open) {
                status = CDC_STORE_ECORRUPT;
                goto done;
            }
            cdc_digest_final(&txn_ctx, seal_expected);
            txn_open = 0;
            if (memcmp(seal_expected, header + OFF_DIGEST,
                       CDC_DIGEST_SIZE) != 0) {
                status = CDC_STORE_ECORRUPT;
                goto done;
            }
            transaction_sequence++;
            sealed++;
            offset += HEADER_SIZE;
        } else {
            status = CDC_STORE_ECORRUPT;
            goto done;
        }
    }
    if (offset != valid_bytes || txn_open || visited != expected_events ||
        sealed != expected_transactions) {
        status = CDC_STORE_ECORRUPT;
    }

done:
    fclose(fp);
    return status;
}

cdc_store_status cdc_store_visit_events(cdc_store *store,
                                        cdc_store_event_visitor visitor,
                                        void *context) {
    scan_result scan;
    cdc_store_status status;

    if (!store || !visitor) {
        return CDC_STORE_EARG;
    }
    status = store_lock(store);
    if (status != CDC_STORE_OK) {
        return status;
    }
    status = scan_log(store->log_path, &scan);
    if (status != CDC_STORE_OK) {
        store_unlock(store);
        return status;
    }
    if (scan.state == SCAN_CORRUPT || !scan.has_head) {
        store_unlock(store);
        return CDC_STORE_ECORRUPT;
    }
    /* A compaction HEAD retains counts and replay identity, not payload
     * bytes. Exact recovery must refuse instead of silently omitting them. */
    if (scan.base_events != 0 || scan.base_sealed != 0) {
        store_unlock(store);
        return CDC_STORE_EUNSUPPORTED;
    }
    status = visit_sealed_prefix(store->log_path, scan.valid_bytes,
                                 scan.events, scan.sealed, visitor, context);
    store_unlock(store);
    return status;
}

cdc_store_status cdc_store_attest(cdc_store *store, char *out,
                                  size_t out_size) {
    FILE *fp;
    cdc_digest_ctx ctx;
    uint8_t digest[CDC_DIGEST_SIZE];
    uint8_t buffer[4096];
    scan_result scan;
    long remaining;

    if (!store || !out) {
        return CDC_STORE_EARG;
    }
    if (scan_log(store->log_path, &scan) != CDC_STORE_OK) {
        return CDC_STORE_EIO;
    }
    if (scan.state == SCAN_CORRUPT) {
        return CDC_STORE_ECORRUPT; /* corrupt evidence is never attested */
    }
    /* The digest runs from byte 0, so it covers the HEAD record — store
     * uuid, generation, and compaction base included. That is what makes
     * the attestation meaningful after compaction: two different histories
     * compacted to the same sealed count still carry different base states
     * in their HEADs, so they attest differently (2026-07-28 review,
     * finding 1). Attesting only the post-compaction tail would digest an
     * effectively empty file and collapse them together. */
    cdc_digest_init(&ctx);
    fp = fopen(store->log_path, "rb");
    remaining = scan.valid_bytes;
    while (fp && remaining > 0) {
        size_t take = remaining > (long)sizeof(buffer) ? sizeof(buffer)
                                                       : (size_t)remaining;
        if (fread(buffer, 1, take, fp) != take) {
            fclose(fp);
            return CDC_STORE_EIO;
        }
        cdc_digest_update(&ctx, buffer, take);
        remaining -= (long)take;
    }
    if (fp) {
        fclose(fp);
    }
    cdc_digest_final(&ctx, digest);
    cdc_digest_hex(digest, out, out_size);
    return CDC_STORE_OK;
}

cdc_store_status cdc_store_verify(cdc_store *store) {
    scan_result scan;
    if (!store) {
        return CDC_STORE_EARG;
    }
    if (scan_log(store->log_path, &scan) != CDC_STORE_OK) {
        return CDC_STORE_EIO;
    }
    if (scan.state == SCAN_CORRUPT) {
        return CDC_STORE_ECORRUPT;
    }
    if (scan.state == SCAN_TAIL) {
        return CDC_STORE_EUNSEALED;
    }
    return CDC_STORE_OK;
}

/* ---- snapshot / compact / fence -------------------------------------
 *
 * Two-phase, one atomic transition (2026-07-28 review, finding 1):
 *
 *   snapshot  prepares a base for generation+1 in `base.pending`. open()
 *             NEVER reads this file, so a crash after snapshot leaves the
 *             store exactly as it was — the dangling-base window is gone
 *             because there is no window.
 *   compact   verifies the prepared base still belongs to THIS store and
 *             still covers the CURRENT sealed prefix, both under the store
 *             lock, then rebuilds the log as a single HEAD record and
 *             activates it with fsync -> rename -> directory fsync.
 *
 * The base is bound to the store uuid and to the exact generation it
 * succeeds. That defeats substituting a valid base from another store and
 * replaying a stale base over an advanced log. It is BINDING, not keyed
 * authentication: an attacker who can rewrite the whole log can still
 * present any self-consistent history, and detecting that requires an
 * anchor retained outside the store. Ed25519 signing over the HEAD is the
 * queued repair for that; it is not claimed here. */

enum {
    PEND_BODY = 8 + HEAD_BODY, /* target generation | head body */
    PEND_SIZE = PEND_BODY + CDC_DIGEST_SIZE
};

/* Reads the prepared base, if one exists. *found=0 when absent.
 * ECORRUPT when present but malformed or tag-mismatched. EIO when the path
 * is not a regular file (directory, FIFO, device) or the read faults —
 * checked before any blocking open, so a FIFO cannot stall the store. */
static cdc_store_status pending_load(const char *path, int *found,
                                     uint64_t *target_generation,
                                     uint8_t body[HEAD_BODY]) {
    int fd;
    struct stat st;
    uint8_t buffer[PEND_SIZE + 1];
    uint8_t tag[CDC_DIGEST_SIZE];
    ssize_t got;

    *found = 0;
    fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        return errno == ENOENT ? CDC_STORE_OK : CDC_STORE_EIO;
    }
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        close(fd);
        return CDC_STORE_EIO;
    }
    got = read(fd, buffer, sizeof(buffer));
    close(fd);
    if (got < 0) {
        return CDC_STORE_EIO;
    }
    if ((size_t)got != PEND_SIZE) {
        return CDC_STORE_ECORRUPT; /* short, or trailing bytes */
    }
    cdc_digest(buffer, PEND_BODY, tag);
    if (memcmp(tag, buffer + PEND_BODY, CDC_DIGEST_SIZE) != 0) {
        return CDC_STORE_ECORRUPT;
    }
    *target_generation = get_u64(buffer);
    memcpy(body, buffer + 8, HEAD_BODY);
    *found = 1;
    return CDC_STORE_OK;
}

cdc_store_status cdc_store_snapshot(cdc_store *store) {
    uint8_t buffer[PEND_SIZE];
    char temp_path[660];
    FILE *fp;
    scan_result scan;
    int written;
    cdc_store_status status;

    if (!store) {
        return CDC_STORE_EARG;
    }
    if (store_lock(store) != CDC_STORE_OK) {
        return CDC_STORE_EIO;
    }
    status = scan_log(store->log_path, &scan);
    if (status != CDC_STORE_OK) {
        store_unlock(store);
        return status;
    }
    if (scan.state == SCAN_CORRUPT || !scan.has_head) {
        store_unlock(store);
        return CDC_STORE_ECORRUPT;
    }
    /* The prepared base succeeds THIS generation of THIS store, and its
     * anchor commits to the HEAD it replaces. */
    {
        uint8_t body[HEAD_BODY];
        uint8_t anchor[CDC_DIGEST_SIZE];
        uint8_t current[HEAD_BODY];
        build_head_body(current, scan.uuid, scan.base_sealed,
                        scan.base_events, scan.base_state,
                        scan.prior_anchor);
        cdc_digest(current, sizeof(current), anchor);
        build_head_body(body, scan.uuid, scan.sealed, scan.events,
                        scan.replay_state, anchor);
        put_u64(buffer, scan.generation + 1);
        memcpy(buffer + 8, body, HEAD_BODY);
        cdc_digest(buffer, PEND_BODY, buffer + PEND_BODY);
    }
    written = snprintf(temp_path, sizeof(temp_path), "%s.tmp",
                       store->pending_path);
    if (written < 0 || (size_t)written >= sizeof(temp_path)) {
        store_unlock(store);
        return CDC_STORE_EARG;
    }
    fp = fopen(temp_path, "wb");
    if (!fp) {
        store_unlock(store);
        return CDC_STORE_EIO;
    }
    if (injected_crash(store)) {
        fclose(fp);
        unlink(temp_path);
        store_unlock(store);
        return CDC_STORE_ECRASH; /* before the base is written */
    }
    if (fwrite(buffer, 1, sizeof(buffer), fp) != sizeof(buffer) ||
        fflush(fp) != 0 || fsync(fileno(fp)) != 0) {
        fclose(fp);
        unlink(temp_path);
        store_unlock(store);
        return CDC_STORE_EIO;
    }
    fclose(fp);
    if (injected_crash(store)) {
        store_unlock(store);
        return CDC_STORE_ECRASH; /* base durable, not yet published */
    }
    if (rename(temp_path, store->pending_path) != 0 ||
        sync_path(store->dir) != 0) {
        unlink(temp_path);
        store_unlock(store);
        return CDC_STORE_EIO;
    }
    store_unlock(store);
    return CDC_STORE_OK;
}

cdc_store_status cdc_store_compact(cdc_store *store) {
    int found = 0;
    uint64_t target_generation = 0;
    uint8_t body[HEAD_BODY];
    uint8_t record[HEADER_SIZE + HEAD_BODY];
    cdc_store_status status;
    scan_result scan;

    if (!store) {
        return CDC_STORE_EARG;
    }
    if (store_lock(store) != CDC_STORE_OK) {
        return CDC_STORE_EIO;
    }
    status = pending_load(store->pending_path, &found, &target_generation,
                          body);
    if (status != CDC_STORE_OK) {
        store_unlock(store);
        return status;
    }
    if (!found) {
        store_unlock(store);
        return CDC_STORE_ESTATE; /* nothing prepared to compact against */
    }
    status = scan_log(store->log_path, &scan);
    if (status != CDC_STORE_OK) {
        store_unlock(store);
        return status;
    }
    if (scan.state == SCAN_CORRUPT || !scan.has_head) {
        store_unlock(store);
        return CDC_STORE_ECORRUPT;
    }
    /* Identity: a base from another store is never activated here. */
    if (memcmp(body, scan.uuid, STORE_UUID_SIZE) != 0) {
        store_unlock(store);
        return CDC_STORE_ECORRUPT;
    }
    /* Generation: the base must succeed exactly the generation on disk, so
     * a stale base cannot be replayed over an advanced log. */
    if (target_generation != scan.generation + 1) {
        store_unlock(store);
        return CDC_STORE_ESTATE;
    }
    /* Coverage: the base must account for the ENTIRE sealed prefix as it
     * stands right now. A transaction committed after the base was
     * prepared makes this fail, so compaction holds rather than discarding
     * a committed transaction (2026-07-28 review, finding 2). */
    if (get_u64(body + STORE_UUID_SIZE) != scan.sealed ||
        get_u64(body + STORE_UUID_SIZE + 8) != scan.events ||
        memcmp(body + STORE_UUID_SIZE + 16, scan.replay_state,
               CDC_DIGEST_SIZE) != 0) {
        store_unlock(store);
        return CDC_STORE_ESTATE;
    }
    build_head_record(record, target_generation, body);
    status = publish_log(store, record, sizeof(record));
    if (status != CDC_STORE_OK) {
        store_unlock(store);
        return status;
    }
    /* The prepared base has been consumed; removing it is not part of the
     * atomic step because its presence or absence changes nothing — a
     * leftover pending base fails the generation check on the next run. */
    unlink(store->pending_path);
    sync_path(store->dir);
    store->generation = target_generation;
    store->base_sealed = scan.sealed;
    store->base_events = scan.events;
    memcpy(store->base_state, scan.replay_state, CDC_DIGEST_SIZE);
    memcpy(store->prior_anchor, body + STORE_UUID_SIZE + 16 + CDC_DIGEST_SIZE,
           CDC_DIGEST_SIZE);
    store->sealed = scan.sealed;
    store->events = scan.events;
    store->valid_bytes = (long)(HEADER_SIZE + HEAD_BODY);
    store->fence_armed = 0; /* the representation changed underneath it */
    store_unlock(store);
    return CDC_STORE_OK;
}


cdc_store_status cdc_store_fence(cdc_store *store, uint64_t expected_seal) {
    scan_result scan;

    if (!store) {
        return CDC_STORE_EARG;
    }
    if (store_lock(store) != CDC_STORE_OK) {
        return CDC_STORE_EIO;
    }
    if (scan_log(store->log_path, &scan) != CDC_STORE_OK) {
        store_unlock(store);
        return CDC_STORE_EIO;
    }
    if (scan.state == SCAN_CORRUPT || !scan.has_head) {
        store_unlock(store);
        return CDC_STORE_ECORRUPT;
    }
    if (scan.sealed != expected_seal) {
        store->fence_armed = 0;
        store_unlock(store);
        return CDC_STORE_ESTATE; /* the writer's view is already stale */
    }
    /* The armed token is the whole (generation, sealed, replay state)
     * triple. Sealed count alone would let a compaction slip past: it can
     * leave the count identical while replacing the representation the
     * writer intended to extend. */
    store->fence_armed = 1;
    store->fence_generation = scan.generation;
    store->fence_seal = scan.sealed;
    memcpy(store->fence_state, scan.replay_state, CDC_DIGEST_SIZE);
    store_unlock(store);
    return CDC_STORE_OK;
}
