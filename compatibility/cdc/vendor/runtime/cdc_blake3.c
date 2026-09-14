/* Portable BLAKE3, hash and keyed-hash modes, 32-byte output. Structure follows the
 * reference design: 1024-byte chunks of sixteen 64-byte blocks, compressed
 * with a 7-round ChaCha-derived permutation, combined as a binary Merkle
 * tree whose final compression carries the ROOT flag. See cdc_blake3.h for
 * scope and verification. */
#include "cdc_blake3.h"

#include <string.h>

enum {
    CHUNK_LEN = 1024,
    BLOCK_LEN = 64,
    CHUNK_START = 1 << 0,
    CHUNK_END = 1 << 1,
    PARENT = 1 << 2,
    ROOT = 1 << 3,
    KEYED_HASH = 1 << 4
};

static const uint32_t IV[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u,
                               0xa54ff53au, 0x510e527fu, 0x9b05688cu,
                               0x1f83d9abu, 0x5be0cd19u};

static const uint8_t MSG_PERMUTATION[16] = {2, 6,  3,  10, 7, 0,  4,  13,
                                            1, 11, 12, 5,  9, 14, 15, 8};

static uint32_t rotr32(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

static void g(uint32_t *state, size_t a, size_t b, size_t c, size_t d,
              uint32_t mx, uint32_t my) {
    state[a] = state[a] + state[b] + mx;
    state[d] = rotr32(state[d] ^ state[a], 16);
    state[c] = state[c] + state[d];
    state[b] = rotr32(state[b] ^ state[c], 12);
    state[a] = state[a] + state[b] + my;
    state[d] = rotr32(state[d] ^ state[a], 8);
    state[c] = state[c] + state[d];
    state[b] = rotr32(state[b] ^ state[c], 7);
}

static void round_fn(uint32_t state[16], const uint32_t m[16]) {
    /* columns */
    g(state, 0, 4, 8, 12, m[0], m[1]);
    g(state, 1, 5, 9, 13, m[2], m[3]);
    g(state, 2, 6, 10, 14, m[4], m[5]);
    g(state, 3, 7, 11, 15, m[6], m[7]);
    /* diagonals */
    g(state, 0, 5, 10, 15, m[8], m[9]);
    g(state, 1, 6, 11, 12, m[10], m[11]);
    g(state, 2, 7, 8, 13, m[12], m[13]);
    g(state, 3, 4, 9, 14, m[14], m[15]);
}

static void permute(uint32_t m[16]) {
    uint32_t permuted[16];
    size_t i;
    for (i = 0; i < 16; i++) {
        permuted[i] = m[MSG_PERMUTATION[i]];
    }
    memcpy(m, permuted, sizeof(permuted));
}

static void compress(const uint32_t cv[8], const uint32_t block_words[16],
                     uint64_t counter, uint32_t block_len, uint32_t flags,
                     uint32_t out[16]) {
    uint32_t block[16];
    uint32_t state[16];
    size_t i;

    state[0] = cv[0];
    state[1] = cv[1];
    state[2] = cv[2];
    state[3] = cv[3];
    state[4] = cv[4];
    state[5] = cv[5];
    state[6] = cv[6];
    state[7] = cv[7];
    state[8] = IV[0];
    state[9] = IV[1];
    state[10] = IV[2];
    state[11] = IV[3];
    state[12] = (uint32_t)counter;
    state[13] = (uint32_t)(counter >> 32);
    state[14] = block_len;
    state[15] = flags;

    memcpy(block, block_words, sizeof(block));
    for (i = 0; i < 7; i++) {
        round_fn(state, block);
        if (i < 6) {
            permute(block);
        }
    }
    for (i = 0; i < 8; i++) {
        state[i] ^= state[i + 8];
        state[i + 8] ^= cv[i];
    }
    memcpy(out, state, sizeof(state));
}

static void words_from_block(const uint8_t block[BLOCK_LEN],
                             uint32_t words[16]) {
    size_t i;
    for (i = 0; i < 16; i++) {
        words[i] = (uint32_t)block[i * 4] |
                   ((uint32_t)block[i * 4 + 1] << 8) |
                   ((uint32_t)block[i * 4 + 2] << 16) |
                   ((uint32_t)block[i * 4 + 3] << 24);
    }
}

static void words_from_key(const uint8_t key[CDC_BLAKE3_OUT_LEN],
                           uint32_t words[8]) {
    size_t i;
    for (i = 0; i < 8; i++) {
        words[i] = (uint32_t)key[i * 4] |
                   ((uint32_t)key[i * 4 + 1] << 8) |
                   ((uint32_t)key[i * 4 + 2] << 16) |
                   ((uint32_t)key[i * 4 + 3] << 24);
    }
}

/* A deferred compression: everything needed to produce either a chaining
 * value or the root output. */
typedef struct {
    uint32_t input_cv[8];
    uint32_t block_words[16];
    uint64_t counter;
    uint32_t block_len;
    uint32_t flags;
} output_t;

static void output_chaining_value(const output_t *o, uint32_t cv[8]) {
    uint32_t state[16];
    compress(o->input_cv, o->block_words, o->counter, o->block_len,
             o->flags, state);
    memcpy(cv, state, 8 * sizeof(uint32_t));
}

static void output_root_bytes(const output_t *o,
                              uint8_t out[CDC_BLAKE3_OUT_LEN]) {
    uint32_t state[16];
    size_t i;
    /* The root output block counter starts at 0 (it indexes XOF output
     * blocks, not chunks); 32 bytes need exactly one compression. */
    compress(o->input_cv, o->block_words, 0, o->block_len, o->flags | ROOT,
             state);
    for (i = 0; i < 8; i++) {
        out[i * 4] = (uint8_t)state[i];
        out[i * 4 + 1] = (uint8_t)(state[i] >> 8);
        out[i * 4 + 2] = (uint8_t)(state[i] >> 16);
        out[i * 4 + 3] = (uint8_t)(state[i] >> 24);
    }
}

static uint32_t chunk_start_flag(const cdc_blake3_chunk *chunk) {
    return chunk->blocks_compressed == 0 ? (uint32_t)CHUNK_START : 0u;
}

static size_t chunk_len(const cdc_blake3_chunk *chunk) {
    return (size_t)chunk->blocks_compressed * BLOCK_LEN +
           (size_t)chunk->block_len;
}

static void chunk_init(cdc_blake3_chunk *chunk, uint64_t counter,
                       const uint32_t key_words[8], uint32_t flags) {
    memcpy(chunk->cv, key_words, sizeof(chunk->cv));
    chunk->chunk_counter = counter;
    memset(chunk->block, 0, sizeof(chunk->block));
    chunk->block_len = 0;
    chunk->blocks_compressed = 0;
    chunk->flags = flags;
}

static void chunk_update(cdc_blake3_chunk *chunk, const uint8_t *input,
                         size_t len) {
    while (len > 0) {
        size_t want;
        size_t take;
        if (chunk->block_len == BLOCK_LEN) {
            uint32_t words[16];
            uint32_t state[16];
            words_from_block(chunk->block, words);
            compress(chunk->cv, words, chunk->chunk_counter, BLOCK_LEN,
                     chunk->flags | chunk_start_flag(chunk), state);
            memcpy(chunk->cv, state, 8 * sizeof(uint32_t));
            chunk->blocks_compressed++;
            memset(chunk->block, 0, sizeof(chunk->block));
            chunk->block_len = 0;
        }
        want = BLOCK_LEN - (size_t)chunk->block_len;
        take = len < want ? len : want;
        memcpy(chunk->block + chunk->block_len, input, take);
        chunk->block_len = (uint8_t)(chunk->block_len + take);
        input += take;
        len -= take;
    }
}

static void chunk_output(const cdc_blake3_chunk *chunk, output_t *out) {
    memcpy(out->input_cv, chunk->cv, sizeof(out->input_cv));
    words_from_block(chunk->block, out->block_words);
    out->counter = chunk->chunk_counter;
    out->block_len = chunk->block_len;
    out->flags = chunk->flags | chunk_start_flag(chunk) | (uint32_t)CHUNK_END;
}

static void parent_output(const uint32_t left[8], const uint32_t right[8],
                          const uint32_t key_words[8], uint32_t flags,
                          output_t *out) {
    memcpy(out->input_cv, key_words, sizeof(out->input_cv));
    memcpy(out->block_words, left, 8 * sizeof(uint32_t));
    memcpy(out->block_words + 8, right, 8 * sizeof(uint32_t));
    out->counter = 0;
    out->block_len = BLOCK_LEN;
    out->flags = flags | PARENT;
}

static void parent_cv(const uint32_t left[8], const uint32_t right[8],
                      const uint32_t key_words[8], uint32_t flags,
                      uint32_t out[8]) {
    output_t parent;
    parent_output(left, right, key_words, flags, &parent);
    output_chaining_value(&parent, out);
}

void cdc_blake3_init(cdc_blake3_hasher *hasher) {
    memcpy(hasher->key_words, IV, sizeof(hasher->key_words));
    hasher->flags = 0;
    chunk_init(&hasher->chunk, 0, hasher->key_words, hasher->flags);
    hasher->cv_stack_len = 0;
}

void cdc_blake3_init_keyed(cdc_blake3_hasher *hasher,
                           const uint8_t key[CDC_BLAKE3_OUT_LEN]) {
    words_from_key(key, hasher->key_words);
    hasher->flags = KEYED_HASH;
    chunk_init(&hasher->chunk, 0, hasher->key_words, hasher->flags);
    hasher->cv_stack_len = 0;
}

static void push_cv(cdc_blake3_hasher *hasher, const uint32_t cv[8]) {
    memcpy(hasher->cv_stack[hasher->cv_stack_len], cv, 8 * sizeof(uint32_t));
    hasher->cv_stack_len++;
}

/* Merge the completed chunk's chaining value into the tree: every trailing
 * zero bit of the chunk count marks a subtree that is now complete. */
static void add_chunk_cv(cdc_blake3_hasher *hasher, uint32_t cv[8],
                         uint64_t total_chunks) {
    while ((total_chunks & 1) == 0) {
        uint32_t merged[8];
        hasher->cv_stack_len--;
        parent_cv(hasher->cv_stack[hasher->cv_stack_len], cv,
                  hasher->key_words, hasher->flags, merged);
        memcpy(cv, merged, sizeof(merged));
        total_chunks >>= 1;
    }
    push_cv(hasher, cv);
}

void cdc_blake3_update(cdc_blake3_hasher *hasher, const void *input,
                       size_t len) {
    const uint8_t *bytes = input;
    while (len > 0) {
        size_t want;
        size_t take;
        if (chunk_len(&hasher->chunk) == CHUNK_LEN) {
            output_t output;
            uint32_t cv[8];
            uint64_t total_chunks;
            chunk_output(&hasher->chunk, &output);
            output_chaining_value(&output, cv);
            total_chunks = hasher->chunk.chunk_counter + 1;
            add_chunk_cv(hasher, cv, total_chunks);
            chunk_init(&hasher->chunk, total_chunks, hasher->key_words,
                       hasher->flags);
        }
        want = CHUNK_LEN - chunk_len(&hasher->chunk);
        take = len < want ? len : want;
        chunk_update(&hasher->chunk, bytes, take);
        bytes += take;
        len -= take;
    }
}

void cdc_blake3_final(const cdc_blake3_hasher *hasher,
                      uint8_t out[CDC_BLAKE3_OUT_LEN]) {
    output_t output;
    size_t remaining = hasher->cv_stack_len;
    chunk_output(&hasher->chunk, &output);
    while (remaining > 0) {
        uint32_t right[8];
        remaining--;
        output_chaining_value(&output, right);
        parent_output(hasher->cv_stack[remaining], right, hasher->key_words,
                      hasher->flags, &output);
    }
    output_root_bytes(&output, out);
}
