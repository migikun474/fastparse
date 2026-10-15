/* Column parsers vs fp_px_column_ref.
 * 1. Random CSVs: fields of digits, dots, junk and '\r', random field counts, with and without a
 *    final '\n', and padding of valid fields ("5,\n5,\n...") that must be ignored. Every column
 * 0..3, with no limit and with max = 3.
 * 2. If data/binance_trades.txt exists (scripts/fetch_binance.sh): the price and qty columns of
 *    the real file must match the reference value for value, and every row must parse. */
#include "helpers.h"
#include <stdio.h>

static uint64_t seed = 7;
static uint64_t next(void) { /* splitmix64 */
    uint64_t z = (seed += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

/* Compare every parser with the reference on buf[0, len), column col, at most max values. */
static int compare(const char *buf, size_t len, int col, size_t max, int64_t *want, int64_t *got) {
    size_t kw = fp_px_column_ref(buf, len, col, want, max), fails = 0;
    for (size_t c = 1; c < LEN(column_parsers); c++) {
        if (!runnable(column_parsers[c].sse))
            continue;
        size_t kg = column_parsers[c].f(buf, len, col, got, max);
        if (kg != kw || memcmp(want, got, kw * sizeof *want)) {
            if (fails++ < 3)
                printf("FAIL %s col %d max %zu: %zu values, want %zu: \"%.*s\"\n",
                       column_parsers[c].name, col, max, kg, kw, (int)(len < 80 ? len : 80), buf);
        }
    }
    return fails != 0;
}

static int random_csvs(void) {
    enum { ITERS = 200000, MAXLEN = 300 };
    static const char other[] = "-a\r ";
    char *buf = aligned_alloc(64, (MAXLEN + FP_PAD + 63) / 64 * 64);
    if (!buf)
        abort();
    int64_t want[MAXLEN], got[MAXLEN];
    int fails = 0;
    for (int it = 0; it < ITERS && fails < 10; it++) {
        size_t len = next() % (MAXLEN + 1);
        for (size_t i = 0; i < len; i++) {
            uint64_t r = next(), c = r % 100;
            buf[i] = c < 60   ? (char)('0' + r / 100 % 10)
                     : c < 68 ? '.'
                     : c < 80 ? ','
                     : c < 90 ? '\n'
                              : other[r / 100 % (sizeof other - 1)];
        }
        for (size_t i = len; i < MAXLEN + FP_PAD; i++)   /* valid fields past the end: ignore */
            buf[i] = "5,\n"[(i - len + (size_t)it) % 3]; /* each byte kind lands at len */
        for (int col = 0; col < 4; col++)
            fails +=
                compare(buf, len, col, MAXLEN, want, got) + compare(buf, len, col, 3, want, got);
    }
    free(buf);
    return fails;
}

static int binance(void) {
    FILE *f = fopen("data/binance_trades.txt", "rb");
    if (!f) {
        puts("test_column: data/binance_trades.txt not found, skipping the Binance check");
        return 0;
    }
    fseek(f, 0, SEEK_END);
    size_t len = (size_t)ftell(f), rows = 0;
    rewind(f);
    char *buf = aligned_alloc(64, (len + FP_PAD + 63) / 64 * 64);
    if (!buf || fread(buf, 1, len, f) != len)
        abort();
    fclose(f);
    memset(buf + len, 0, FP_PAD);
    for (size_t i = 0; i < len; i++)
        rows += buf[i] == '\n';
    int64_t *want = malloc(rows * sizeof *want), *got = malloc(rows * sizeof *got);
    if (!want || !got)
        abort();
    int fails = 0;
    for (int col = 1; col <= 2; col++) { /* price, qty */
        size_t k = fp_px_column_ref(buf, len, col, want, rows);
        long double total = 0;
        for (size_t i = 0; i < k; i++)
            total += want[i];
        printf("test_column: binance col %d: %zu of %zu rows, total %.8Lf\n", col, k, rows,
               total / FP_SCALE);
        if (k != rows) {
            printf("FAIL reference parsed %zu of %zu rows\n", k, rows);
            fails++;
        }
        fails += compare(buf, len, col, rows, want, got);
    }
    free(want);
    free(got);
    free(buf);
    return fails;
}

int main(void) {
    int fails = random_csvs() + binance();
    printf("test_column: %s\n", fails ? "FAIL" : "ok");
    return fails != 0;
}
