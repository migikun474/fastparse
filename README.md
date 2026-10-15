# fastparse
Branchless SWAR, x86-64 assembly and SSE/AVX2 parsers that turn market-data text into fixed-point integers, benchmarked with rdtsc percentiles and checked against a reference on every PR.

## Build and test

```bash
./scripts/build.sh --cc gcc --march native   # or --cc clang, --march x86-64|x86-64-v2|x86-64-v3
./scripts/test.sh
./scripts/build.sh --san && ./scripts/test.sh   # ASan + UBSan
./scripts/selftest.sh   # checks the tooling: bad code must fail
```

Benchmark corpora (10M lines each, about 300 MB in `data/`, not committed): `./scripts/gen_corpus.sh`

## Benchmark (x86-64, Linux)

```bash
sudo ./scripts/tune_cpu.sh             # performance governor, turbo off, perf counters allowed
./scripts/run_bench.sh                 # or --cc clang; pinned to core 3, writes results/<date>-<HHMM>-<cc>.json
sudo ./scripts/tune_cpu.sh --restore
```

Assembly: `./scripts/asm_dump.sh` writes gcc, clang and hand-written disassembly to `build/asm/` and prints instruction counts.

## Library

```c
#include "fastparse.h"        // link build/libfastparse.a (./scripts/build.sh --lib)
int64_t px;
int r = fp_px(buf, n, &px);   // r == n on success; buf needs FP_PAD readable bytes after n
size_t k = fp_px_column(csv, len, 1, prices, max_rows);  // price column of a CSV
```

`fp_u64`, `fp_px` and `fp_px_column` pick SSE / AVX2 at load time when the CPU has them (`fp_impl()` says which).

Real data: `./scripts/fetch_binance.sh` (one day of BTCUSDT trades, sha256-checked). Fuzzing: `./scripts/fuzz.sh u64|px|column [seconds]`. Results page: `./scripts/render_results.sh`.
