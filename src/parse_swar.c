/* SWAR: 8 digits per step inside one uint64_t, no per-digit branch. */
#include "fastparse.h"
#include <string.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "the SWAR parsers assume little-endian loads"
#endif

/* Load k (1..8) digits into the top k bytes. The bytes below them become '0', i.e. leading
 * zeros. Reads 8 bytes, up to 7 past the field: allowed by FP_PAD. */
static uint64_t load_digits(const char *p, size_t k) {
    uint64_t x;
    memcpy(&x, p, 8);
    unsigned s = 8 * (8 - (unsigned)k);
    return x << s | (0x3030303030303030ULL & ((1ULL << s) - 1));
}

/* Every byte is '0'..'9': its high nibble is 3, and adding 6 keeps it 3. */
static int all_digits(uint64_t x) {
    const uint64_t hi = 0xF0F0F0F0F0F0F0F0ULL;
    return ((x & hi) | (((x + 0x0606060606060606ULL) & hi) >> 4)) == 0x3333333333333333ULL;
}

/* 8 digit bytes, most significant in the lowest byte, to their value. */
static uint64_t swar8(uint64_t x) {
    x -= 0x3030303030303030ULL;
    x = (x * 10 + (x >> 8)) & 0x00FF00FF00FF00FFULL;        /* pairs */
    x = (x * 100 + (x >> 16)) & 0x0000FFFF0000FFFFULL;      /* quads */
    return (x * 10000 + (x >> 32)) & 0x00000000FFFFFFFFULL; /* eight */
}

int fp_u64_swar(const char *p, size_t n, uint64_t *out) {
    if (n == 0)
        return -FP_EMPTY;
    uint64_t v = 0;
    int overflow = n > 20;
    /* The first chunk takes the leading (n - 1) % 8 + 1 digits, so the rest are full 8s. */
    for (size_t i = 0, k = (n - 1) % 8 + 1; i < n; i += k, k = 8) {
        uint64_t x = load_digits(p + i, k);
        if (!all_digits(x))
            return -FP_BAD_CHAR;
        overflow |= __builtin_mul_overflow(v, 100000000, &v);
        overflow |= __builtin_add_overflow(v, swar8(x), &v);
    }
    if (overflow)
        return -FP_OVERFLOW;
    *out = v;
    return (int)n;
}
