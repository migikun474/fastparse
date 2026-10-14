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

/* Every u64 parser. The first is the reference the others are compared with. */
static const struct {
    const char *name;
    fp_u64_fn f;
} u64_parsers[] = {
    {"fp_u64_ref", fp_u64_ref},
    {"fp_u64_naive", fp_u64_naive},
    {"fp_u64_swar", fp_u64_swar},
#ifdef __x86_64__
    {"fp_u64_asm", fp_u64_asm},
#endif
};
