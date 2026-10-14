/* Serialised TSC reads and a one-off calibration against CLOCK_MONOTONIC. x86-64 only.
 * TSC ticks equal core cycles only with turbo off (scripts/tune_cpu.sh), and need constant_tsc
 * and nonstop_tsc, which scripts/run_bench.sh checks. */
#include <stdint.h>
#include <time.h>

/* lfence first: earlier instructions finish before the clock is read. */
static inline uint64_t tsc_start(void) {
    uint32_t lo, hi;
    __asm__ volatile("lfence\n\trdtsc" : "=a"(lo), "=d"(hi)::"memory");
    return (uint64_t)hi << 32 | lo;
}

/* rdtscp waits for earlier instructions; lfence stops later ones starting before the read. */
static inline uint64_t tsc_end(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtscp\n\tlfence" : "=a"(lo), "=d"(hi)::"rcx", "memory");
    return (uint64_t)hi << 32 | lo;
}

static inline uint64_t now_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000u + (uint64_t)t.tv_nsec;
}

/* TSC ticks per nanosecond, measured over 100 ms. */
static inline double tsc_ghz(void) {
    uint64_t n0 = now_ns(), t0 = tsc_start(), n1;
    while ((n1 = now_ns()) - n0 < 100000000u)
        ;
    return (double)(tsc_end() - t0) / (double)(n1 - n0);
}
