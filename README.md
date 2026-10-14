# fastparse
Branchless SWAR, x86-64 assembly and SSE/AVX2 parsers that turn market-data text into fixed-point integers, benchmarked with rdtsc percentiles and checked against a reference on every PR.

## Build and test

```bash
./scripts/build.sh --cc gcc --march native   # or --cc clang, --march x86-64|x86-64-v2|x86-64-v3
./scripts/test.sh
```
