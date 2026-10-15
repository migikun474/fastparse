/* Benchmark one parser on one corpus file (one number per line). Prints one JSON line.
 * Usage: bench <parser> <corpus.txt> throughput|latency|count
 *        bench list    parser/corpus pairs to run, used by run_bench.sh and count_instr.sh
 * count: one untimed pass (error count + checksum), for cachegrind instruction counts. */
#include "fastparse.h"
#include "tsc.h"
#include <linux/perf_event.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#define BATCH 1024 /* numbers per timed sample: one parse costs about as much as the timer */
#define REPS 10
#define WARMUP_NS 1000000000u

static const struct {
    const char *name;
    fp_u64_fn u64; /* exactly one of u64 / px / column is set */
    fp_px_fn px;
    size_t (*column)(const char *, size_t, int, int64_t *, size_t); /* price column (1) of a CSV */
    const char *corpora[2];
} parsers[] = {
    {"fp_u64_ref", fp_u64_ref, NULL, NULL, {"fixed8", "uniform"}},
    {"fp_u64_naive", fp_u64_naive, NULL, NULL, {"fixed8", "uniform"}},
    {"fp_u64_swar", fp_u64_swar, NULL, NULL, {"fixed8", "uniform"}},
    {"fp_u64_asm", fp_u64_asm, NULL, NULL, {"fixed8", "uniform"}},
    {"fp_u64_sse", fp_u64_sse, NULL, NULL, {"fixed8", "uniform"}}, /* needs SSSE3 + SSE4.1 */
    {"fp_px_ref", NULL, fp_px_ref, NULL, {"prices", "binance"}},
    {"fp_px_sse", NULL, fp_px_sse, NULL, {"prices", "binance"}},
    /* throughput only: one call parses a whole batch of rows */
    {"fp_px_column_ref", NULL, NULL, fp_px_column_ref, {"binance_trades", NULL}},
    {"fp_px_column_avx2", NULL, NULL, fp_px_column_avx2, {"binance_trades", NULL}}, /* AVX2 */
};
#define NPARSERS (sizeof parsers / sizeof *parsers)

static char *buf;              /* the corpus, followed by FP_PAD zero bytes */
static uint32_t *off, *len;    /* where each number starts, and its length */
static volatile size_t vzero;  /* always 0, but the compiler can't know that */
static volatile uint64_t sink; /* sum of every result, so no parse can be optimised away */
static int64_t *colbuf;        /* column parsers' output */

/* Latency mode adds (v & zero), which is 0, to the next input address: each parse then has to
 * wait for the previous result, so the CPU can't overlap calls. */
#define LOOP(f, T, DEP)                                                                            \
    do {                                                                                           \
        T v = 0;                                                                                   \
        for (size_t k = i; k < j; k++) {                                                           \
            errs += f(buf + off[k] + (DEP), len[k], &v) < 0;                                       \
            sum += (uint64_t)v;                                                                    \
        }                                                                                          \
    } while (0)

/* Parse numbers [i, j) with parser p; returns how many failed. */
static size_t parse(size_t p, size_t i, size_t j, int latency) {
    fp_u64_fn fu = parsers[p].u64;
    fp_px_fn fx = parsers[p].px;
    size_t errs = 0, zero = vzero;
    uint64_t sum = 0;
    if (parsers[p].column) { /* rows [i, j) as one CSV slice, without the last '\n' */
        size_t got =
            parsers[p].column(buf + off[i], off[j - 1] + len[j - 1] - off[i], 1, colbuf, j - i);
        sink += got ? (uint64_t)colbuf[got - 1] : 0;
        return j - i - got;
    }
    if (fu && !latency)
        LOOP(fu, uint64_t, 0);
    else if (fu)
        LOOP(fu, uint64_t, (size_t)v & zero);
    else if (!latency)
        LOOP(fx, int64_t, 0);
    else
        LOOP(fx, int64_t, (size_t)v & zero);
    sink += sum;
    return errs;
}

/* cycles, instructions, branches, branch misses for this process, as one group.
 * Returns -1 if perf_event_paranoid forbids it (run scripts/tune_cpu.sh). */
static int perf_open(void) {
    static const uint64_t ev[] = {PERF_COUNT_HW_CPU_CYCLES, PERF_COUNT_HW_INSTRUCTIONS,
                                  PERF_COUNT_HW_BRANCH_INSTRUCTIONS, PERF_COUNT_HW_BRANCH_MISSES};
    int leader = -1;
    for (size_t i = 0; i < 4; i++) {
        struct perf_event_attr a;
        memset(&a, 0, sizeof a);
        a.type = PERF_TYPE_HARDWARE;
        a.size = sizeof a;
        a.config = ev[i];
        a.disabled = leader < 0;
        a.exclude_kernel = 1;
        a.exclude_hv = 1;
        a.read_format = PERF_FORMAT_GROUP;
        int fd = (int)syscall(SYS_perf_event_open, &a, 0, -1, leader, 0);
        if (fd < 0)
            return -1;
        if (leader < 0)
            leader = fd;
    }
    return leader;
}

static int cmp_u64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

static int usage(void) {
    fprintf(stderr, "usage: bench <parser> <corpus.txt> throughput|latency|count\n"
                    "       bench list\n"
                    "column parsers have no latency mode\n");
    return 2;
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "list")) {
        for (size_t p = 0; p < NPARSERS; p++)
            for (size_t c = 0; c < 2 && parsers[p].corpora[c]; c++)
                printf("%s %s\n", parsers[p].name, parsers[p].corpora[c]);
        return 0;
    }
    if (argc != 4)
        return usage();
    size_t p = 0;
    while (p < NPARSERS && strcmp(parsers[p].name, argv[1]))
        p++;
    const char *mode = argv[3];
    int latency = !strcmp(mode, "latency");
    if (p == NPARSERS || (!latency && strcmp(mode, "throughput") && strcmp(mode, "count")) ||
        (latency && parsers[p].column))
        return usage();

    FILE *f = fopen(argv[2], "rb");
    if (!f || fseek(f, 0, SEEK_END) || ftell(f) < 0) {
        perror(argv[2]);
        return 1;
    }
    size_t size = (size_t)ftell(f);
    rewind(f);
    buf = aligned_alloc(64, (size + FP_PAD + 63) / 64 * 64);
    if (!buf || fread(buf, 1, size, f) != size) {
        perror(argv[2]);
        return 1;
    }
    fclose(f);
    memset(buf + size, 0, FP_PAD);

    size_t n = 1; /* upper bound on line count */
    for (size_t i = 0; i < size; i++)
        n += buf[i] == '\n';
    off = malloc(n * sizeof *off);
    len = malloc(n * sizeof *len);
    if (!off || !len)
        return 1;
    n = 0;
    for (size_t i = 0, start = 0; i <= size; i++)
        if (i == size ? i > start : buf[i] == '\n') {
            off[n] = (uint32_t)start;
            len[n++] = (uint32_t)(i - start);
            start = i + 1;
        }
    if (parsers[p].column && !(colbuf = malloc(n * sizeof *colbuf)))
        return 1;
    const char *corpus = strrchr(argv[2], '/');
    corpus = corpus ? corpus + 1 : argv[2];
    int clen = (int)strcspn(corpus, ".");

    size_t errors = parse(p, 0, n, 0); /* untimed pass: error count and checksum */
    if (!strcmp(mode, "count")) {
        printf("{\"parser\":\"%s\",\"corpus\":\"%.*s\",\"numbers\":%zu,\"errors\":%zu,"
               "\"sum\":%llu}\n",
               argv[1], clen, corpus, n, errors, (unsigned long long)sink);
        return 0;
    }

    size_t batches = n / BATCH, samples = batches * REPS;
    if (!batches) {
        fprintf(stderr, "%s: need at least %d numbers\n", argv[2], BATCH);
        return 1;
    }
    uint64_t *cyc = malloc(samples * sizeof *cyc), total = 0, ctr[5] = {0};
    if (!cyc)
        return 1;
    double ghz = tsc_ghz();
    for (uint64_t t0 = now_ns(); now_ns() - t0 < WARMUP_NS;)
        parse(p, 0, n, latency);

    int pe = perf_open();
    if (pe >= 0)
        ioctl(pe, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
    for (size_t r = 0; r < REPS; r++)
        for (size_t b = 0; b < batches; b++) {
            uint64_t t0 = tsc_start();
            parse(p, b * BATCH, (b + 1) * BATCH, latency);
            uint64_t t = tsc_end() - t0;
            cyc[r * batches + b] = t;
            total += t;
        }
    if (pe >= 0) {
        ioctl(pe, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
        if (read(pe, ctr, sizeof ctr) != (ssize_t)sizeof ctr)
            pe = -1;
    }

    qsort(cyc, samples, sizeof *cyc, cmp_u64);
    double timed = (double)samples * BATCH, bytes = 0, secs = (double)total / ghz / 1e9;
    for (size_t k = 0; k < batches * BATCH; k++)
        bytes += len[k] + (parsers[p].column != NULL); /* CSV rows: count the '\n' too */
    bytes *= REPS;
#define PCT(q) ((double)cyc[(size_t)((q) * (double)(samples - 1))] / BATCH)
    printf("{\"parser\":\"%s\",\"corpus\":\"%.*s\",\"mode\":\"%s\",\"numbers\":%zu,"
           "\"errors\":%zu,\"tsc_ghz\":%.3f,\"mnum_per_s\":%.1f,\"mb_per_s\":%.1f,"
           "\"tsc_per_num\":{\"p50\":%.2f,\"p99\":%.2f,\"p999\":%.2f,\"max\":%.2f},"
           "\"ns_per_num_p50\":%.2f,",
           argv[1], clen, corpus, mode, n, errors, ghz, timed / secs / 1e6, bytes / secs / 1e6,
           PCT(0.5), PCT(0.99), PCT(0.999), PCT(1.0), PCT(0.5) / ghz);
    if (pe >= 0)
        printf("\"perf\":{\"cycles\":%.2f,\"instructions\":%.2f,\"branches\":%.2f,"
               "\"branch_misses\":%.4f}}\n",
               (double)ctr[1] / timed, (double)ctr[2] / timed, (double)ctr[3] / timed,
               (double)ctr[4] / timed);
    else
        printf("\"perf\":null}\n");
    free(cyc);
    return 0;
}
