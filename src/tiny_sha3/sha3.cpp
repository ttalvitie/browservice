// sha3.c
// 19-Nov-11  Markku-Juhani O. Saarinen <mjos@iki.fi>

// Revised 07-Aug-15 to match with official release of FIPS PUB 202 "SHA3"
// Revised 03-Sep-15 for portability + OpenSSL - style API

// Modified for use with browservice (omit unused stuff, follow strict aliasing rules)

#include "sha3.hpp"

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "Only little endian is supported"
#endif

// update the state with given number of rounds

void sha3_keccakf(uint64_t st[25])
{
    // constants
    const uint64_t keccakf_rndc[24] = {
        0x0000000000000001, 0x0000000000008082, 0x800000000000808a,
        0x8000000080008000, 0x000000000000808b, 0x0000000080000001,
        0x8000000080008081, 0x8000000000008009, 0x000000000000008a,
        0x0000000000000088, 0x0000000080008009, 0x000000008000000a,
        0x000000008000808b, 0x800000000000008b, 0x8000000000008089,
        0x8000000000008003, 0x8000000000008002, 0x8000000000000080,
        0x000000000000800a, 0x800000008000000a, 0x8000000080008081,
        0x8000000000008080, 0x0000000080000001, 0x8000000080008008
    };
    const int keccakf_rotc[24] = {
        1,  3,  6,  10, 15, 21, 28, 36, 45, 55, 2,  14,
        27, 41, 56, 8,  25, 43, 62, 18, 39, 61, 20, 44
    };
    const int keccakf_piln[24] = {
        10, 7,  11, 17, 18, 3, 5,  16, 8,  21, 24, 4,
        15, 23, 19, 13, 12, 2, 20, 14, 22, 9,  6,  1
    };

    // variables
    int i, j, r;
    uint64_t t, bc[5];

    // actual iteration
    for (r = 0; r < KECCAKF_ROUNDS; r++) {

        // Theta
        for (i = 0; i < 5; i++)
            bc[i] = st[i] ^ st[i + 5] ^ st[i + 10] ^ st[i + 15] ^ st[i + 20];

        for (i = 0; i < 5; i++) {
            t = bc[(i + 4) % 5] ^ ROTL64(bc[(i + 1) % 5], 1);
            for (j = 0; j < 25; j += 5)
                st[j + i] ^= t;
        }

        // Rho Pi
        t = st[1];
        for (i = 0; i < 24; i++) {
            j = keccakf_piln[i];
            bc[0] = st[j];
            st[j] = ROTL64(t, keccakf_rotc[i]);
            t = bc[0];
        }

        //  Chi
        for (j = 0; j < 25; j += 5) {
            for (i = 0; i < 5; i++)
                bc[i] = st[j + i];
            for (i = 0; i < 5; i++)
                st[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
        }

        //  Iota
        st[0] ^= keccakf_rndc[r];
    }
}

// Initialize the context for SHA3

int sha3_init(sha3_ctx_t *c, int mdlen)
{
    int i;

    for (i = 0; i < 25; i++)
        c->st[i] = 0;
    c->mdlen = mdlen;
    c->rsiz = 200 - 2 * mdlen;
    c->pt = 0;

    return 1;
}

// update state with more data

int sha3_update(sha3_ctx_t *c, const unsigned char *data, size_t len)
{
    size_t i;
    int j;

    j = c->pt;
    for (i = 0; i < len; i++) {
        *((unsigned char*)c->st + j++) ^= data[i];
        if (j >= c->rsiz) {
            sha3_keccakf(c->st);
            j = 0;
        }
    }
    c->pt = j;

    return 1;
}

// finalize and output a hash

int sha3_final(unsigned char *md, sha3_ctx_t *c)
{
    int i;

    *((unsigned char*)c->st + c->pt) ^= 0x06;
    *((unsigned char*)c->st + c->rsiz - 1) ^= 0x80;
    sha3_keccakf(c->st);

    for (i = 0; i < c->mdlen; i++) {
        md[i] = *((unsigned char*)c->st + i);
    }

    return 1;
}

// compute a SHA-3 hash (md) of given byte length from "in"

void *sha3(const unsigned char *in, size_t inlen, unsigned char *md, int mdlen)
{
    sha3_ctx_t sha3;

    sha3_init(&sha3, mdlen);
    sha3_update(&sha3, in, inlen);
    sha3_final(md, &sha3);

    return md;
}
