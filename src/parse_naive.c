/* Baseline: v = v * 10 + digit, one data-dependent branch per byte. */
#include "fastparse.h"

int fp_u64_naive(const char *p, size_t n, uint64_t *out) {
    if (n == 0)
        return -FP_EMPTY;
    uint64_t v = 0;
    int overflow = n > 20; /* leading zeros count toward the 20 */
    for (size_t i = 0; i < n; i++) {
        unsigned d = (unsigned char)p[i] - '0';
        if (d > 9)
            return -FP_BAD_CHAR;
        overflow |= __builtin_mul_overflow(v, 10, &v);
        overflow |= __builtin_add_overflow(v, d, &v);
    }
    if (overflow)
        return -FP_OVERFLOW;
    *out = v;
    return (int)n;
}
