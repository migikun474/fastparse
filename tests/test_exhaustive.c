/* Every digit string of 1 to 8 digits, leading zeros included (111,111,110 strings), through
 * every u64 parser, against its known value. */
#include "helpers.h"
#include <stdio.h>

int main(void) {
    char *b = padded("00000000", 8); /* bytes past the field are digits too */
    long fails = 0;
    for (size_t n = 1; n <= 8; n++) {
        memset(b, '0', n);
        for (uint64_t want = 0;; want++) {
            for (size_t k = 0; k < LEN(u64_parsers); k++) {
                uint64_t v = ~0ULL;
                if ((u64_parsers[k].f(b, n, &v) != (int)n || v != want) && ++fails <= 10)
                    printf("FAIL %s \"%.*s\": got %llu\n", u64_parsers[k].name, (int)n, b,
                           (unsigned long long)v);
            }
            size_t i = n; /* next string: decimal increment, stop after all 9s */
            while (i && b[i - 1] == '9')
                b[--i] = '0';
            if (!i)
                break;
            b[i - 1]++;
        }
    }
    free(b);
    printf("test_exhaustive: %s (%ld failures)\n", fails ? "FAIL" : "ok", fails);
    return fails != 0;
}
