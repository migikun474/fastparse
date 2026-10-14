/* Differential test: every u64 parser must match fp_u64_ref on random digit/junk strings. */
#include "helpers.h"
#include <stdio.h>

#define ITERS 1000000

static uint64_t seed = 42; /* fixed, so a failure replays */

static uint64_t next(void) { /* splitmix64 */
    uint64_t z = (seed += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

int main(void) {
    static const char junk[] = "-+. ,e\n\0a/:\xb5";
    char s[24];
    int fails = 0;
    for (long it = 0; it < ITERS; it++) {
        size_t n = next() % (sizeof s + 1); /* 0..24 bytes, crosses the 20-digit limit */
        for (size_t i = 0; i < n; i++) {
            uint64_t r = next(); /* 1 byte in 10 is junk */
            s[i] = r % 10 ? (char)('0' + r / 10 % 10) : junk[r / 10 % (sizeof junk - 1)];
        }
        char *b = padded(s, n);
        uint64_t want = 0, got = 0;
        int rw = fp_u64_ref(b, n, &want);
        for (size_t k = 1; k < LEN(u64_parsers); k++) {
            int rg = u64_parsers[k].f(b, n, &got);
            int bad = rw != rg || (rw > 0 && want != got);
            if (bad && ++fails <= 10) { /* print the first 10, count all */
                printf("FAIL %s iter %ld: got %d/%llu want %d/%llu, bytes:", u64_parsers[k].name,
                       it, rg, (unsigned long long)got, rw, (unsigned long long)want);
                for (size_t i = 0; i < n; i++)
                    printf(" %02x", (unsigned char)s[i]);
                putchar('\n');
            }
        }
        free(b);
    }
    printf("test_diff: %s (%d mismatches in %d strings)\n", fails ? "FAIL" : "ok", fails, ITERS);
    return fails != 0;
}
