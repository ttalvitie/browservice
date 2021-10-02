// sha3.h
// 19-Nov-11  Markku-Juhani O. Saarinen <mjos@iki.fi>

// Modified for use with browservice (omit unused stuff, follow strict aliasing rules)

#ifndef SHA3_H
#define SHA3_H

#include <stddef.h>
#include <stdint.h>

#ifndef KECCAKF_ROUNDS
#define KECCAKF_ROUNDS 24
#endif

#ifndef ROTL64
#define ROTL64(x, y) (((x) << (y)) | ((x) >> (64 - (y))))
#endif

// state context
typedef struct {
    uint64_t st[25];
    int pt, rsiz, mdlen;                    // these don't overflow
} sha3_ctx_t;

// Compression function.
void sha3_keccakf(uint64_t st[25]);

// OpenSSL - like interfece
int sha3_init(sha3_ctx_t *c, int mdlen);    // mdlen = hash output in bytes
int sha3_update(sha3_ctx_t *c, const unsigned char *data, size_t len);
int sha3_final(unsigned char *md, sha3_ctx_t *c);    // digest goes to md

// compute a sha3 hash (md) of given byte length from "in"
void *sha3(const unsigned char *in, size_t inlen, unsigned char *md, int mdlen);

#endif

