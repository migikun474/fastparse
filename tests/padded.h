/* Test helper for the FP_PAD input contract. */
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
