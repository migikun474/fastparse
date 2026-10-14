/* Reference parsers: slow and plainly correct. Every other parser must match them exactly. */
#include "fastparse.h"

static int is_digit(char c) { return c >= '0' && c <= '9'; }

int fp_u64_ref(const char *p, size_t n, uint64_t *out) {
    if (n == 0)
        return -FP_EMPTY;
    for (size_t i = 0; i < n; i++)
        if (!is_digit(p[i]))
            return -FP_BAD_CHAR;
    if (n > 20)
        return -FP_OVERFLOW;
    unsigned __int128 v = 0; /* 20 digits < 10^20 < 2^127, so this cannot overflow */
    for (size_t i = 0; i < n; i++)
        v = v * 10 + (unsigned)(p[i] - '0');
    if (v > UINT64_MAX)
        return -FP_OVERFLOW;
    *out = (uint64_t)v;
    return (int)n;
}

int fp_px_ref(const char *p, size_t n, int64_t *out) {
    if (n == 0)
        return -FP_EMPTY;
    size_t dot = n; /* index of the '.', or n if there is none */
    for (size_t i = 0; i < n; i++) {
        if (p[i] == '.' && dot == n && i > 0)
            dot = i;
        else if (!is_digit(p[i]))
            return -FP_BAD_CHAR;
    }
    size_t frac = dot == n ? 0 : n - dot - 1;
    if (dot > 10)
        return -FP_OVERFLOW;
    if (frac > 8)
        return -FP_TOO_MANY_FRAC;
    int64_t v = 0; /* at most 18 digits, fits in int64 */
    for (size_t i = 0; i < n; i++)
        if (i != dot)
            v = v * 10 + (p[i] - '0');
    for (size_t i = frac; i < 8; i++)
        v *= 10;
    *out = v;
    return (int)n;
}
