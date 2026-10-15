/* libFuzzer target: every parser must match its reference on the fuzzer's input, or trap.
 * scripts/fuzz.sh builds it with -DFUZZ_U64, -DFUZZ_PX or -DFUZZ_COLUMN (one CI job each). */
#include "helpers.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    char *b = padded((const char *)data, size); /* honours FP_PAD; bytes past the input are '9' */
#if defined(FUZZ_U64)
    uint64_t want = 0, got = 0;
    int rw = fp_u64_ref(b, size, &want);
    for (size_t k = 1; k < LEN(u64_parsers); k++)
        if (runnable(u64_parsers[k].sse)) {
            int rg = u64_parsers[k].f(b, size, &got);
            if (rg != rw || (rw > 0 && got != want))
                __builtin_trap();
        }
#elif defined(FUZZ_PX)
    int64_t want = 0, got = 0;
    int rw = fp_px_ref(b, size, &want);
    for (size_t k = 1; k < LEN(px_parsers); k++)
        if (runnable(px_parsers[k].sse)) {
            int rg = px_parsers[k].f(b, size, &got);
            if (rg != rw || (rw > 0 && got != want))
                __builtin_trap();
        }
#elif defined(FUZZ_COLUMN)
    for (size_t i = size; i < size + FP_PAD; i++) /* valid fields past the end: must be ignored */
        b[i] = "5,\n"[i % 3];
    int64_t *want = malloc((size + 1) * sizeof *want), *got = malloc((size + 1) * sizeof *got);
    if (!want || !got)
        abort();
    for (int col = 0; col < 4; col++) {
        size_t kw = fp_px_column_ref(b, size, col, want, size + 1);
        for (size_t k = 1; k < LEN(column_parsers); k++)
            if (runnable(column_parsers[k].sse) &&
                (column_parsers[k].f(b, size, col, got, size + 1) != kw ||
                 memcmp(want, got, kw * sizeof *want)))
                __builtin_trap();
    }
    free(want);
    free(got);
#else
#error "build with -DFUZZ_U64, -DFUZZ_PX or -DFUZZ_COLUMN"
#endif
    free(b);
    return 0;
}
