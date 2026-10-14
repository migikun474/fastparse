/* Phase 0 smoke test: proves build.sh, test.sh and CI wiring work end to end. */
#include "fastparse.h"
#include <stdio.h>

static int fails;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);                                    \
            fails++;                                                                               \
        }                                                                                          \
    } while (0)

int main(void) {
    CHECK(FP_PAD == 64);
    CHECK(FP_SCALE == 100000000LL);
    CHECK(FP_OK == 0);
    puts(fails ? "test_smoke: FAIL" : "test_smoke: ok");
    return fails != 0;
}
