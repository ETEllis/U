#ifndef CDC_BLAKE3_H
#define CDC_BLAKE3_H

#include <stddef.h>
#include <stdint.h>

/* Portable BLAKE3 (hash and keyed-hash modes, 32-byte output) — the canonical content
 * digest required by Amendment A3, closing the DECISIONS D2 interim-digest
 * gate. Vendored as a single dependency-free translation unit so the
 * runtime keeps its no-external-dependency property.
 *
 * Scope: unkeyed hash and keyed-hash modes with 32-byte output. Key
 * derivation and extended (XOF) output are NOT implemented. Streaming update
 * is supported. Keyed mode is used only where a protocol explicitly requires
 * authenticity; unkeyed content identities remain unchanged.
 *
 * Verification: runtime/cdc_frontend_check.c `digest-vectors` checks this
 * implementation against tests/fixtures/digest/blake3_vectors.txt, which
 * covers single-block, block-boundary, chunk-boundary, and multi-level
 * tree inputs. */

enum { CDC_BLAKE3_OUT_LEN = 32 };

typedef struct {
    uint32_t cv[8];
    uint64_t chunk_counter;
    uint8_t block[64];
    uint8_t block_len;
    uint8_t blocks_compressed;
    uint32_t flags;
} cdc_blake3_chunk;

typedef struct {
    cdc_blake3_chunk chunk;
    uint32_t cv_stack[54][8];
    uint8_t cv_stack_len;
    uint32_t key_words[8];
    uint32_t flags;
} cdc_blake3_hasher;

void cdc_blake3_init(cdc_blake3_hasher *hasher);
void cdc_blake3_init_keyed(cdc_blake3_hasher *hasher,
                           const uint8_t key[CDC_BLAKE3_OUT_LEN]);
void cdc_blake3_update(cdc_blake3_hasher *hasher, const void *input,
                       size_t len);
void cdc_blake3_final(const cdc_blake3_hasher *hasher,
                      uint8_t out[CDC_BLAKE3_OUT_LEN]);

#endif
