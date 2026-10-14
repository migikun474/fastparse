/* SSE parsers: 16 bytes per step. Built for SSSE3 + SSE4.1 on every x86-64 build, whatever
 * -march says, so only call them on a CPU that has both: fp_u64 / fp_px (src/dispatch.c) check. */
#ifdef __x86_64__
#include "fastparse.h"
#include <immintrin.h>

#define SSE __attribute__((target("ssse3,sse4.1")))

static const int64_t scale[9] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000};

/* Lanes of d (byte - '0') that hold a digit, as a 16-bit mask: unsigned d <= 9. */
SSE static unsigned digit_mask(__m128i d) {
    __m128i nine = _mm_set1_epi8(9);
    return (unsigned)_mm_movemask_epi8(_mm_cmpeq_epi8(_mm_max_epu8(d, nine), nine));
}

/* Value of 16 digit lanes, most significant in lane 0. */
SSE static uint64_t combine16(__m128i d) {
    __m128i x = _mm_maddubs_epi16(d, _mm_set1_epi16(0x010A)); /* pairs: 10 * a + b */
    x = _mm_madd_epi16(x, _mm_set1_epi32(0x00010064));        /* quads: 100 * a + b */
    x = _mm_packus_epi32(x, x);                               /* back to 16-bit lanes */
    x = _mm_madd_epi16(x, _mm_set1_epi32(0x00012710));        /* eights: 10000 * a + b */
    uint64_t r = (uint64_t)_mm_cvtsi128_si64(x);              /* two 8-digit halves */
    return (r & 0xFFFFFFFF) * 100000000 + (r >> 32);
}

/* pshufb index that moves m digits to lanes 16-m..15. Below them the index is negative (high
 * bit set), and pshufb writes 0 there: leading zeros. Lanes from `skip` on are taken one lane
 * further along, which steps over a '.' at lane `skip`. */
SSE static __m128i right_align(unsigned m, unsigned skip) {
    __m128i s = _mm_add_epi8(_mm_setr_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15),
                             _mm_set1_epi8((char)(m - 16)));
    return _mm_sub_epi8(s, _mm_cmpgt_epi8(s, _mm_set1_epi8((char)(skip - 1))));
}

SSE int fp_u64_sse(const char *p, size_t n, uint64_t *out) {
    if (n == 0)
        return -FP_EMPTY;
    if (n > 20) /* ponytail: always an error; SWAR finds which one */
        return fp_u64_swar(p, n, out);
    uint64_t lead = 0; /* 17-20 digits: the first 1-4 one at a time, then 16 */
    size_t k = n;
    for (; k > 16; k--, p++) {
        unsigned d = (unsigned char)*p - '0';
        if (d > 9)
            return -FP_BAD_CHAR;
        lead = lead * 10 + d;
    }
    /* Loads 16 bytes, up to 15 past the field: allowed by FP_PAD. */
    __m128i d = _mm_sub_epi8(_mm_loadu_si128((const __m128i *)p), _mm_set1_epi8('0'));
    unsigned lanes = (1u << k) - 1;
    if ((digit_mask(d) & lanes) != lanes)
        return -FP_BAD_CHAR;
    uint64_t v = combine16(_mm_shuffle_epi8(d, right_align((unsigned)k, 16)));
    int overflow = __builtin_mul_overflow(lead, 10000000000000000ULL, &lead);
    overflow |= __builtin_add_overflow(lead, v, &v);
    if (overflow)
        return -FP_OVERFLOW;
    *out = v;
    return (int)n;
}

SSE int fp_px_sse(const char *p, size_t n, int64_t *out) {
    if (n == 0)
        return -FP_EMPTY;
    if (n > 16) /* ponytail: 17-19 byte prices (long integer and fraction) take the scalar path */
        return fp_px_ref(p, n, out);
    __m128i x = _mm_loadu_si128((const __m128i *)p); /* up to 15 bytes past the field: FP_PAD */
    __m128i d = _mm_sub_epi8(x, _mm_set1_epi8('0'));
    unsigned lanes = (1u << n) - 1;
    unsigned digits = digit_mask(d) & lanes;
    unsigned dots = (unsigned)_mm_movemask_epi8(_mm_cmpeq_epi8(x, _mm_set1_epi8('.'))) & lanes;
    /* Valid: only digits and dots, at most one dot, and not in the first byte. */
    if ((digits | dots) != lanes || (dots & (dots - 1)) || (dots & 1))
        return -FP_BAD_CHAR;
    unsigned dot = dots ? (unsigned)__builtin_ctz(dots) : (unsigned)n; /* = integer digits */
    unsigned frac = dots ? (unsigned)n - dot - 1 : 0;
    if (dot > 10)
        return -FP_OVERFLOW;
    if (frac > 8)
        return -FP_TOO_MANY_FRAC;
    unsigned m = (unsigned)n - (dots != 0); /* digits, without the dot */
    int64_t v = (int64_t)combine16(_mm_shuffle_epi8(d, right_align(m, dot)));
    *out = v * scale[8 - frac]; /* table lookup instead of a branch */
    return (int)n;
}
#endif
