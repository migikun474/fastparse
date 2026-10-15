/* CSV column parsers: price column `col` (0-based) of every line of buf into out[], at most max.
 * Fields end at ',' or '\n'; the last line may lack its '\n'. A row whose field is missing or
 * fails the price rules (header, empty, '\r', junk) is skipped. Returns how many were stored.
 * buf needs FP_PAD readable bytes after buf + len. */
#include "fastparse.h"
#ifdef __x86_64__
#include <immintrin.h>
#endif

typedef struct {
    const char *buf;
    int64_t *out;
    size_t start, k, max; /* start of the current field; values stored so far */
    int col, cur;         /* wanted column; column of the current field */
    fp_px_fn parse;
} scan;

/* A ',' or '\n' at pos ends the field [start, pos). */
static void delimiter(scan *s, size_t pos, int newline) {
    int64_t v;
    if (s->cur == s->col && s->k < s->max && s->parse(s->buf + s->start, pos - s->start, &v) > 0)
        s->out[s->k++] = v;
    s->cur = newline ? 0 : s->cur + 1;
    s->start = pos + 1;
}

size_t fp_px_column_ref(const char *buf, size_t len, int col, int64_t *out, size_t max) {
    scan s = {buf, out, 0, 0, max, col, 0, fp_px_ref};
    for (size_t i = 0; i < len && s.k < max; i++)
        if (buf[i] == ',' || buf[i] == '\n')
            delimiter(&s, i, buf[i] == '\n');
    if (s.start < len) /* last line without '\n' */
        delimiter(&s, len, 1);
    return s.k;
}

#ifdef __x86_64__
/* 32 bytes per step: compare against ',' and '\n', then walk the set bits with tzcnt. Fields go
 * to the SSE price kernel (every AVX2 CPU has SSE4.1). Only call on an AVX2 CPU. */
__attribute__((target("avx2,bmi"))) size_t fp_px_column_avx2(const char *buf, size_t len, int col,
                                                             int64_t *out, size_t max) {
    scan s = {buf, out, 0, 0, max, col, 0, fp_px_sse};
    const __m256i comma = _mm256_set1_epi8(','), nl = _mm256_set1_epi8('\n');
    for (size_t i = 0; i < len && s.k < max; i += 32) {
        /* Up to 31 bytes past len: allowed by FP_PAD, and masked off below. */
        __m256i x = _mm256_loadu_si256((const __m256i *)(buf + i));
        uint32_t lines = (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(x, nl));
        uint32_t m = (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(x, comma)) | lines;
        if (len - i < 32)
            m &= (1u << (len - i)) - 1;
        for (; m; m &= m - 1) {
            unsigned b = (unsigned)_tzcnt_u32(m);
            delimiter(&s, i + b, lines >> b & 1);
        }
    }
    if (s.start < len)
        delimiter(&s, len, 1);
    return s.k;
}
#endif
