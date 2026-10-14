#ifndef FASTPARSE_H
#define FASTPARSE_H

#include <stddef.h>
#include <stdint.h>

#define FP_PAD 64            /* bytes readable past the end of any input */
#define FP_SCALE 100000000LL /* prices are stored as value * 10^8 */

typedef enum { FP_OK = 0, FP_EMPTY, FP_BAD_CHAR, FP_OVERFLOW, FP_TOO_MANY_FRAC } fp_status;

/* Every parser reads exactly the field [p, p + n) and returns n on success, or a negative
 * fp_status, checked in this order:
 *   n == 0                                -> FP_EMPTY
 *   any byte outside the format           -> FP_BAD_CHAR
 *   u64:   more than 20 digits, or > UINT64_MAX              -> FP_OVERFLOW
 *   price: [0-9]{1,10}(\.[0-9]{0,8})?, more than 10 integer digits -> FP_OVERFLOW,
 *          then more than 8 fraction digits                  -> FP_TOO_MANY_FRAC
 * *out is written only on success. The buffer must have FP_PAD readable bytes after p + n. */
typedef int (*fp_u64_fn)(const char *p, size_t n, uint64_t *out);
typedef int (*fp_px_fn)(const char *p, size_t n, int64_t *out);

int fp_u64_ref(const char *p, size_t n, uint64_t *out); /* __int128, obviously correct */
int fp_u64_naive(const char *p, size_t n, uint64_t *out);

int fp_px_ref(const char *p, size_t n, int64_t *out);

#endif
