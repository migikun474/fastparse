#ifndef FASTPARSE_H
#define FASTPARSE_H

#include <stddef.h>
#include <stdint.h>

#define FP_PAD 64            /* bytes readable past the end of any input */
#define FP_SCALE 100000000LL /* prices are stored as value * 10^8 */

typedef enum { FP_OK = 0, FP_EMPTY, FP_BAD_CHAR, FP_OVERFLOW, FP_TOO_MANY_FRAC } fp_status;

/* Returns bytes consumed (>0) or a negative fp_status. */
typedef int (*fp_u64_fn)(const char *p, size_t n, uint64_t *out);
typedef int (*fp_px_fn)(const char *p, size_t n, int64_t *out);

/* Parser prototypes are added in the phase that implements them (phase 1 onward). */

#endif
