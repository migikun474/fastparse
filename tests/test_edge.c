/* Edge cases from the plan, run against every parser. Bytes after n are padding ('9'). */
#include "padded.h"
#include <stdio.h>

#define LEN(a) (sizeof(a) / sizeof *(a))
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
    C("999999999999999999999x", B, 0), /* bad char wins over overflow */
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

static const struct {
    const char *name;
    fp_u64_fn f;
} u64_fns[] = {{"fp_u64_ref", fp_u64_ref}, {"fp_u64_naive", fp_u64_naive}};

static const struct {
    const char *name;
    fp_px_fn f;
} px_fns[] = {{"fp_px_ref", fp_px_ref}};

int main(void) {
    int fails = 0;
    for (size_t k = 0; k < LEN(u64_fns); k++)
        for (size_t i = 0; i < LEN(u64_cases); i++) {
            char *b = padded(u64_cases[i].s, u64_cases[i].len);
            uint64_t v = SENTINEL;
            int r = u64_fns[k].f(b, u64_cases[i].n, &v);
            uint64_t want = u64_cases[i].ret > 0 ? u64_cases[i].v : SENTINEL;
            if (r != u64_cases[i].ret || v != want) {
                printf("FAIL %s u64_cases[%zu]: got %d/%llu want %d/%llu\n", u64_fns[k].name, i, r,
                       (unsigned long long)v, u64_cases[i].ret, (unsigned long long)want);
                fails++;
            }
            free(b);
        }
    for (size_t k = 0; k < LEN(px_fns); k++)
        for (size_t i = 0; i < LEN(px_cases); i++) {
            char *b = padded(px_cases[i].s, px_cases[i].len);
            int64_t v = (int64_t)SENTINEL;
            int r = px_fns[k].f(b, px_cases[i].n, &v);
            int64_t want = px_cases[i].ret > 0 ? px_cases[i].v : (int64_t)SENTINEL;
            if (r != px_cases[i].ret || v != want) {
                printf("FAIL %s px_cases[%zu]: got %d/%lld want %d/%lld\n", px_fns[k].name, i, r,
                       (long long)v, px_cases[i].ret, (long long)want);
                fails++;
            }
            free(b);
        }
    puts(fails ? "test_edge: FAIL" : "test_edge: ok");
    return fails != 0;
}
