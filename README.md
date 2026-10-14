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
./scripts/run_bench.sh                 # or --cc clang; pinned to core 3, writes results/<date>-<cc>.json
sudo ./scripts/tune_cpu.sh --restore
```
