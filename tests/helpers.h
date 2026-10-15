/* Shared by the tests: the parsers under test, and the FP_PAD input contract. */
#include "fastparse.h"
#include <stdlib.h>
#include <string.h>

/* Copy len bytes of s into a 64-byte-aligned buffer with at least FP_PAD bytes after them.
 * The padding is '9', so a parser that reads past n returns a wrong value. Caller frees. */
static inline char *padded(const char *s, size_t len) {
    size_t size = (len + FP_PAD + 63) / 64 * 64;
    char *b = aligned_alloc(64, size);
    if (!b)
        abort();
    memset(b, '9', size);
    memcpy(b, s, len);
    return b;
}

#define LEN(a) (sizeof(a) / sizeof *(a))

/* Every parser. The first of each list is the reference the others are compared with.
 * sse = 1: needs SSSE3 + SSE4.1; 2: also AVX2 + BMI. runnable() skips them on CPUs without. */
static const struct {
    const char *name;
    fp_u64_fn f;
    int sse;
} u64_parsers[] = {
    {"fp_u64_ref", fp_u64_ref, 0},   {"fp_u64_naive", fp_u64_naive, 0},
    {"fp_u64_swar", fp_u64_swar, 0},
#ifdef __x86_64__
    {"fp_u64_asm", fp_u64_asm, 0},   {"fp_u64_sse", fp_u64_sse, 1},
#endif
    {"fp_u64", fp_u64, 0},
};

static const struct {
    const char *name;
    fp_px_fn f;
    int sse;
} px_parsers[] = {
    {"fp_px_ref", fp_px_ref, 0},
#ifdef __x86_64__
    {"fp_px_sse", fp_px_sse, 1},
#endif
    {"fp_px", fp_px, 0},
};

static const struct {
    const char *name;
    size_t (*f)(const char *, size_t, int, int64_t *, size_t);
    int sse;
} column_parsers[] = {
    {"fp_px_column_ref", fp_px_column_ref, 0},
#ifdef __x86_64__
    {"fp_px_column_avx2", fp_px_column_avx2, 2},
#endif
    {"fp_px_column", fp_px_column, 0},
};

static inline int runnable(int sse) {
#ifdef __x86_64__
    return !sse || (__builtin_cpu_supports("ssse3") && __builtin_cpu_supports("sse4.1") &&
                    (sse < 2 || (__builtin_cpu_supports("avx2") && __builtin_cpu_supports("bmi"))));
#else
    return !sse;
#endif
}
