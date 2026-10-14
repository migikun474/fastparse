/* Edge cases from the plan, run against every parser. Bytes after n are padding ('9'). */
#include "helpers.h"
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#define SENTINEL 0x5A5A5A5A5A5A5A5AULL /* *out must keep this on error */
/* C: the whole literal is the field. P: only its first n bytes are; the rest follows in memory. */
#define C(lit, ret, v)                                                                             \
    { lit, sizeof(lit) - 1, sizeof(lit) - 1, ret, v }
#define P(lit, n, ret, v)                                                                          \
    { lit, sizeof(lit) - 1, n, ret, v }

enum { E = -FP_EMPTY, B = -FP_BAD_CHAR, O = -FP_OVERFLOW, F = -FP_TOO_MANY_FRAC };

static const struct {
    const char *s;
    size_t len, n;
    int ret;
    uint64_t v;
} u64_cases[] = {
    C("", E, 0),
    C("0", 1, 0),
    C("7", 1, 7),
    C("007", 3, 7),
    C("12345678", 8, 12345678),
    C("9999999999999999999", 19, 9999999999999999999ULL),
    C("18446744073709551615", 20, UINT64_MAX),
    C("18446744073709551616", O, 0),
    C("99999999999999999999", O, 0),
    C("00000000000000000001", 20, 1),
    C("000000000000000000001", O, 0),
    C("100000000000000000000", O, 0),
    C("999999999999999999999x", B, 0),           /* bad char wins over overflow */
    C("1234567890123456", 16, 1234567890123456), /* one full SSE load */
    C("12345678901234567", 17, 12345678901234567ULL),
    C("x234567890123456", B, 0),
    C("123456789012345x", B, 0),
    C("1234567890123456x", B, 0),
    C("5,", B, 0),
    C("5\n", B, 0),
    C("5.", B, 0),
    C("5\0", B, 0),
    C("-5", B, 0),
    C("+5", B, 0),
    C(" 5", B, 0),
    C("5 ", B, 0),
    C("1e5", B, 0),
    C("/", B, 0), /* '0' - 1 */
    C(":", B, 0), /* '9' + 1 */
    C("\xb5", B, 0),
    P("5,", 1, 1, 5),
    P("5\n", 1, 1, 5),
    P("5.", 1, 1, 5),
    P("5\0", 1, 1, 5),
    P("12", 0, E, 0),
};

static const struct {
    const char *s;
    size_t len, n;
    int ret;
    int64_t v;
} px_cases[] = {
    C("", E, 0),
    C("0", 1, 0),
    C("5", 1, 500000000),
    C("5.", 2, 500000000),
    C("1.000", 5, 100000000),
    C("27431.55", 8, 2743155000000),
    C("0.00000001", 10, 1),
    C("1.12345678", 10, 112345678),
    C("0000000000.1", 12, 10000000),
    C("9999999999.99999999", 19, 999999999999999999),
    C("1234567890.12345", 16, 123456789012345000), /* longest price one SSE load takes */
    C("1234567890.123456", 17, 123456789012345600),
    C("123456789.123456", 16, 12345678912345600),
    C("12345.6789012", 13, 1234567890120),
    C("1.2345678", 9, 123456780),
    C("12345678901.2345", O, 0),
    C("1234567890.1234.", B, 0),
    C("1.123456789", F, 0),
    C("12345678901", O, 0),
    C("12345678901.5", O, 0),
    C("12345678901.123456789", O, 0), /* overflow wins over too many fraction digits */
    C("1.12345678x", B, 0),           /* bad char wins over everything */
    C("123456789012x", B, 0),
    C(".5", B, 0),
    C(".", B, 0),
    C("5..", B, 0),
    C("1.2.3", B, 0),
    C("-1.5", B, 0),
    C("+1", B, 0),
    C(" 1.5", B, 0),
    C("1.5 ", B, 0),
    C("1.5e3", B, 0),
    C("1,5", B, 0),
    C("\xb5", B, 0),
    P("1.5,", 3, 3, 150000000),
    P("1.5\0", 3, 3, 150000000),
    P("1.5.", 3, 3, 150000000),
    P("1.5", 1, 1, 100000000),
};

/* '7' * n (and, as a price, with a '.' second to last) placed so the field ends (a) exactly at a
 * page boundary, with the next page as padding, and (b) FP_PAD bytes before an unreadable page: a
 * parser that reads further crashes. */
static int page_edge_fails(void) {
    size_t page = (size_t)sysconf(_SC_PAGESIZE);
    char *m = mmap(NULL, 3 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (m == MAP_FAILED || mprotect(m + 2 * page, page, PROT_NONE))
        abort();
    memset(m, '7', 2 * page);
    char *ends[] = {m + page, m + 2 * page - FP_PAD};
    int fails = 0;
    for (size_t e = 0; e < LEN(ends); e++)
        for (size_t n = 1; n <= 24; n++) {
            uint64_t want = 0, v = 0;
            int rw = fp_u64_ref(ends[e] - n, n, &want);
            for (size_t k = 1; k < LEN(u64_parsers); k++)
                if (runnable(u64_parsers[k].sse) &&
                    (u64_parsers[k].f(ends[e] - n, n, &v) != rw || (rw > 0 && v != want))) {
                    printf("FAIL %s page edge %zu, n=%zu\n", u64_parsers[k].name, e, n);
                    fails++;
                }
            int64_t pwant = 0, pv = 0;
            ends[e][-2] = '.'; /* as a price: 7..7.7 */
            rw = fp_px_ref(ends[e] - n, n, &pwant);
            for (size_t k = 1; k < LEN(px_parsers); k++)
                if (runnable(px_parsers[k].sse) &&
                    (px_parsers[k].f(ends[e] - n, n, &pv) != rw || (rw > 0 && pv != pwant))) {
                    printf("FAIL %s page edge %zu, n=%zu\n", px_parsers[k].name, e, n);
                    fails++;
                }
            ends[e][-2] = '7';
        }
    munmap(m, 3 * page);
    return fails;
}

int main(void) {
    int fails = page_edge_fails();
    const char *want_impl = runnable(1) && !getenv("FP_FORCE_SCALAR") ? "sse" : "scalar";
    if (strcmp(fp_impl(), want_impl)) {
        printf("FAIL dispatch picked %s, want %s\n", fp_impl(), want_impl);
        fails++;
    }
    for (size_t k = 0; k < LEN(u64_parsers); k++)
        for (size_t i = 0; i < LEN(u64_cases) && runnable(u64_parsers[k].sse); i++) {
            char *b = padded(u64_cases[i].s, u64_cases[i].len);
            uint64_t v = SENTINEL;
            int r = u64_parsers[k].f(b, u64_cases[i].n, &v);
            uint64_t want = u64_cases[i].ret > 0 ? u64_cases[i].v : SENTINEL;
            if (r != u64_cases[i].ret || v != want) {
                printf("FAIL %s u64_cases[%zu]: got %d/%llu want %d/%llu\n", u64_parsers[k].name, i,
                       r, (unsigned long long)v, u64_cases[i].ret, (unsigned long long)want);
                fails++;
            }
            free(b);
        }
    for (size_t k = 0; k < LEN(px_parsers); k++)
        for (size_t i = 0; i < LEN(px_cases) && runnable(px_parsers[k].sse); i++) {
            char *b = padded(px_cases[i].s, px_cases[i].len);
            int64_t v = (int64_t)SENTINEL;
            int r = px_parsers[k].f(b, px_cases[i].n, &v);
            int64_t want = px_cases[i].ret > 0 ? px_cases[i].v : (int64_t)SENTINEL;
            if (r != px_cases[i].ret || v != want) {
                printf("FAIL %s px_cases[%zu]: got %d/%lld want %d/%lld\n", px_parsers[k].name, i,
                       r, (long long)v, px_cases[i].ret, (long long)want);
                fails++;
            }
            free(b);
        }
    puts(fails ? "test_edge: FAIL" : "test_edge: ok");
    return fails != 0;
}
