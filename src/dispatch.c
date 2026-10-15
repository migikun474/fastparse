/* fp_u64 / fp_px / fp_px_column: the fastest parser this CPU supports, picked once at load time.
 * FP_FORCE_SCALAR in the environment picks the scalar ones, so their path can be tested too. */
#include "fastparse.h"
#include <stdlib.h>

static fp_u64_fn u64_best = fp_u64_swar;
static fp_px_fn px_best = fp_px_ref;
static size_t (*column_best)(const char *, size_t, int, int64_t *, size_t) = fp_px_column_ref;

__attribute__((constructor)) static void pick(void) {
#ifdef __x86_64__
    __builtin_cpu_init(); /* needed before cpu_supports inside a constructor */
    if (!getenv("FP_FORCE_SCALAR") && __builtin_cpu_supports("ssse3") &&
        __builtin_cpu_supports("sse4.1")) {
        u64_best = fp_u64_sse;
        px_best = fp_px_sse;
        if (__builtin_cpu_supports("avx2") && __builtin_cpu_supports("bmi"))
            column_best = fp_px_column_avx2;
    }
#endif
}

int fp_u64(const char *p, size_t n, uint64_t *out) { return u64_best(p, n, out); }
int fp_px(const char *p, size_t n, int64_t *out) { return px_best(p, n, out); }
size_t fp_px_column(const char *buf, size_t len, int col, int64_t *out, size_t max) {
    return column_best(buf, len, col, out, max);
}
const char *fp_impl(void) {
#ifdef __x86_64__
    if (u64_best == fp_u64_sse && px_best == fp_px_sse)
        return column_best == fp_px_column_avx2 ? "avx2" : "sse";
#endif
    return "scalar";
}
