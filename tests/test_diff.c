/* Differential fuzz: every parser must match its reference (fp_u64_ref / fp_px_ref) on random
 * strings of digits, dots and junk. FP_FUZZ_ITERS sets how many of each (default 1M; ci.yml
 * runs 100M). The seed is fixed, so a failure replays. */
#include "helpers.h"
#include <stdio.h>

#define MAX 24 /* longest string: past the 20-digit and 19-byte price limits */

static uint64_t seed = 42;
static long it, fails;

static uint64_t next(void) { /* splitmix64 */
    uint64_t z = (seed += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

/* Random string in b, 0..MAX bytes; bytes after it up to MAX are '9'. Per byte: 1 in 10 junk,
 * and with dots, 1 in 10 a '.'; the rest digits. */
static size_t gen(char *b, int dots) {
    static const char junk[] = "-+ ,e\n\0a/:\xb5";
    size_t n = next() % (MAX + 1);
    for (size_t i = 0; i < n; i++) {
        uint64_t r = next();
        unsigned c = r % 10;
        b[i] = c == 0           ? junk[r / 10 % (sizeof junk - 1)]
               : c == 1 && dots ? '.'
                                : (char)('0' + r / 10 % 10);
    }
    memset(b + n, '9', MAX - n);
    return n;
}

static void check(const char *name, const char *b, size_t n, int rw, int rg,
                  unsigned long long want, unsigned long long got) {
    if ((rw == rg && (rw < 0 || want == got)) || ++fails > 10)
        return;
    printf("FAIL %s iter %ld: got %d/%llu want %d/%llu, bytes:", name, it, rg, got, rw, want);
    for (size_t i = 0; i < n; i++)
        printf(" %02x", (unsigned char)b[i]);
    putchar('\n');
}

int main(void) {
    const char *env = getenv("FP_FUZZ_ITERS");
    long iters = env ? atol(env) : 1000000;
    static const char zeros[MAX];
    char *b = padded(zeros, MAX); /* one buffer for every string: no malloc per string */
    for (it = 0; it < iters; it++) {
        size_t n = gen(b, 0);
        uint64_t want = 0, got = 0;
        int rw = fp_u64_ref(b, n, &want);
        for (size_t k = 1; k < LEN(u64_parsers); k++)
            if (runnable(u64_parsers[k].sse)) {
                got = 0;
                int rg = u64_parsers[k].f(b, n, &got); /* before check(): argument order */
                check(u64_parsers[k].name, b, n, rw, rg, want, got);
            }
        n = gen(b, 1);
        int64_t pwant = 0, pgot = 0;
        rw = fp_px_ref(b, n, &pwant);
        for (size_t k = 1; k < LEN(px_parsers); k++)
            if (runnable(px_parsers[k].sse)) {
                pgot = 0;
                int rg = px_parsers[k].f(b, n, &pgot);
                check(px_parsers[k].name, b, n, rw, rg, (unsigned long long)pwant,
                      (unsigned long long)pgot);
            }
    }
    free(b);
    printf("test_diff: %s (%ld mismatches; %ld u64 + %ld price strings, fp_impl %s)\n",
           fails ? "FAIL" : "ok", fails, iters, iters, fp_impl());
    return fails != 0;
}
