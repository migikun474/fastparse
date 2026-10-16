# fastparse

[![ci](https://github.com/migikun474/fastparse/actions/workflows/ci.yml/badge.svg)](https://github.com/migikun474/fastparse/actions/workflows/ci.yml)
[![sanitizers](https://github.com/migikun474/fastparse/actions/workflows/sanitizers.yml/badge.svg)](https://github.com/migikun474/fastparse/actions/workflows/sanitizers.yml)
[![lint](https://github.com/migikun474/fastparse/actions/workflows/lint.yml/badge.svg)](https://github.com/migikun474/fastparse/actions/workflows/lint.yml)
[![fuzz](https://github.com/migikun474/fastparse/actions/workflows/fuzz.yml/badge.svg)](https://github.com/migikun474/fastparse/actions/workflows/fuzz.yml)

A small C library that turns market-data text such as `"27431.55"` into fixed-point integers. The same parser is written several ways, each faster than the last:

1. a naive digit loop,
2. an 8-digits-at-a-time SWAR trick,
3. that SWAR trick in hand-written x86-64 assembly,
4. SSE kernels that take 16 bytes per step,
5. an AVX2 CSV column scanner.

Every version must return exactly what a deliberately simple reference parser returns, on every input. Every speed claim comes with percentiles, hardware counters, the CPU, the compiler and the CPU settings it was measured with.

```
Binance BTCUSDT trades CSV, price column, 3.56M rows    reference  973 MB/s ─▶ AVX2 4,071 MB/s (gcc) / 5,311 MB/s (clang)
Integers, 1–19 digits of random length                  naive 63.3 ─▶ SSE 19.7 TSC ticks per number (gcc, p50)
Prices, 1–10 integer + 0–8 fraction digits              ref  100.2 ─▶ SSE 28.8 TSC ticks per number (gcc, p50)
```

---

## Contents

- [Why this exists](#why-this-exists)
- [Results](#results)
- [Quick start](#quick-start)
- [API and input contract](#api-and-input-contract)
- [How each parser works](#how-each-parser-works)
- [What the numbers say](#what-the-numbers-say)
- [How correctness is checked](#how-correctness-is-checked)
- [How performance is measured](#how-performance-is-measured)
- [CI/CD](#cicd)
- [Repository layout](#repository-layout)
- [Scripts](#scripts)
- [Limitations and known corners](#limitations-and-known-corners)
- [How it was built](#how-it-was-built)
- [Further reading](#further-reading)

---

## Why this exists

Text formats sit on the hot path of trading systems: FIX order messages, exchange CSV and JSON feeds (crypto venues especially), and replayed historical files. Each tick carries several prices and quantities, so the cost of turning digits into integers is paid millions of times a day. Real data varies in length, and a parser that branches once per digit mispredicts whenever the length changes.

fastparse avoids floating point entirely. Prices become integers scaled by 10⁸, the way trading systems store them. The library is also a worked study of *why* each technique is faster, read from the assembly and the CPU's performance counters.

**Non-goals:** floating-point output, scientific notation, locales, signs, full FIX/JSON message parsing, and beating [fast_float](https://github.com/fastfloat/fast_float) or [simdjson](https://github.com/simdjson/simdjson). Those are references to read, not targets.

---

## Results

AMD Ryzen 7 7840HS (Zen 4), Linux 7.0, pinned to core 3, performance governor, boost off, 10 repetitions after a 1 s warm-up, batches of 1,024 numbers. Throughput mode. "Ticks" are TSC ticks per number at the median. With boost off they match core cycles from `perf` to within about 5%. Full tables, including p99/p99.9 and latency mode: **[results.md](results.md)**.

### Integers (`uint64_t`)

| parser | `fixed8` ticks | `uniform` ticks | instr/num (`uniform`) | branch misses/num (`uniform`) | speed vs naive (`fixed8` / `uniform`) |
|---|--:|--:|--:|--:|--:|
| `fp_u64_ref` (`__int128`) | 58.0 | 100.4 | 278 | 1.01 | 0.68× / 0.64× |
| `fp_u64_naive` | 39.2 | 63.3 | 181 | 0.99 | 1× / 1× |
| `fp_u64_swar` | 18.4 | 36.0 | 124 | 0.63 | 2.05× / 1.76× |
| `fp_u64_asm` (hand-written) | 17.2 | 36.1 | 117 | 0.63 | 2.23× / 1.77× |
| **`fp_u64_sse`** | **13.2** | **19.7** | **73** | **0.28** | **2.78× / 3.19×** |

gcc 13.3, `-O2 -march=native`. With clang 18 SSE reaches 13.1 / 18.0 ticks (2.83× / 3.23× naive).

### Prices (`int64_t`, ×10⁸)

| parser | `prices` (synthetic) ticks | `binance` (real) ticks | speed vs ref (`prices` / `binance`) |
|---|--:|--:|--:|
| `fp_px_ref` | 100.2 | 65.5 | 1× / 1× |
| **`fp_px_sse`** | **28.8** | **17.1** | **3.41× / 3.71×** |

gcc. With clang: SSE 24.3 / 17.5 ticks (4.10× / 3.09× ref).

### CSV column (Binance trades, price column, 3,558,844 rows)

| parser | gcc MB/s | gcc ticks/row | clang MB/s | clang ticks/row |
|---|--:|--:|--:|--:|
| `fp_px_column_ref` | 973 | 292.5 | 755 | 376.2 |
| **`fp_px_column_avx2`** | **4,071** | **68.3** | **5,311** | **53.0** |

The AVX2 scanner reads whole ~76-byte trade rows at 4–5 GB/s, 4.2× (gcc) and 7.1× (clang) the reference.

---

## Quick start

Needs Linux, gcc or clang, and bash. The SIMD paths and the benchmark need x86-64; everything else, ARM included, uses the scalar paths.

```bash
./scripts/build.sh                      # gcc, -O2 -march=native; tests + build/bench
./scripts/test.sh                       # edge, differential, exhaustive, column, smoke tests
./scripts/build.sh --cc clang --san && ./scripts/test.sh   # clang with ASan + UBSan
./scripts/selftest.sh                   # checks that the tooling itself catches broken code
```

`build.sh` options: `--cc gcc|clang`, `--march x86-64|x86-64-v2|x86-64-v3|native`, `--san`, and `--lib` (also builds `build/libfastparse.a`).

### Use it as a library

```bash
./scripts/build.sh --lib
cc -O2 -Iinclude my_app.c build/libfastparse.a -o my_app
```

```c
#include "fastparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const char *text = "27431.55";
    size_t n = strlen(text);

    /* Every input buffer needs FP_PAD (64) readable bytes after its end. */
    char *buf = aligned_alloc(64, (n + FP_PAD + 63) / 64 * 64);
    memcpy(buf, text, n);
    memset(buf + n, 0, FP_PAD);

    int64_t price;
    int r = fp_px(buf, n, &price);           /* r == n on success, else -fp_status */
    if (r > 0)
        printf("%lld (= %s * 10^8), using %s\n", (long long)price, text, fp_impl());
    free(buf);
}
```

```
2743155000000 (= 27431.55 * 10^8), using avx2
```

For a whole CSV, `fp_px_column(csv, len, col, out, max)` fills `out[]` with column `col` (0-based) of every row. With `col = 1` on a Binance trades file, that is the price column.

---

## API and input contract

All of it is in [`include/fastparse.h`](include/fastparse.h).

### Formats

| kind | accepted field | range | output |
|---|---|---|---|
| integer | `[0-9]{1,20}` | 0 … 18,446,744,073,709,551,615 (`UINT64_MAX`) | `uint64_t` |
| price | `[0-9]{1,10}(\.[0-9]{0,8})?` | 0 … 9,999,999,999.99999999 | `int64_t`, value × 10⁸ (`FP_SCALE`) |

- Leading zeros are allowed and count toward the digit limits.
- `5.` is a valid price. `.5` is not, because a price needs at least one integer digit.
- No signs, whitespace, exponents or thousands separators. Anything outside the format is an error, never a guess.

### Every parser reads exactly the field `[p, p + n)`

Bytes after `n` are never interpreted, even when they are digits. On success a parser returns `n` and writes `*out`. On failure it returns a negative `fp_status`, leaves `*out` untouched, and the error is decided in this order:

| order | condition | result |
|--:|---|---|
| 1 | `n == 0` | `-FP_EMPTY` |
| 2 | any byte outside the format | `-FP_BAD_CHAR` |
| 3 | integer: more than 20 digits, or above `UINT64_MAX`; price: more than 10 integer digits | `-FP_OVERFLOW` |
| 4 | price: more than 8 fraction digits | `-FP_TOO_MANY_FRAC` |

Because every case has exactly one right answer, every fast parser can be held to *exactly* the reference's output, error codes included.

### Padding: `FP_PAD`

The SIMD parsers load 8, 16 or 32 bytes even for a 3-digit number. **Every buffer must have `FP_PAD` (64) readable bytes after its end** (simdjson uses the same rule). The extra bytes are read but masked off and never affect the result. A test places fields exactly 64 bytes before an unreadable page to enforce this.

### Functions

| function | what it is |
|---|---|
| `fp_u64`, `fp_px` | **use these.** The fastest available parser, picked once at load time |
| `fp_px_column` | **use this** for CSV. AVX2 scanner when the CPU has it |
| `fp_impl()` | `"avx2"`, `"sse"` or `"scalar"`: what the dispatcher picked |
| `fp_u64_ref`, `fp_px_ref`, `fp_px_column_ref` | references: slow and plainly correct |
| `fp_u64_naive`, `fp_u64_swar`, `fp_u64_asm` | the scalar steps (asm: x86-64 only) |
| `fp_u64_sse`, `fp_px_sse` | SSE kernels (x86-64, SSSE3 + SSE4.1 CPUs only) |
| `fp_px_column_avx2` | AVX2 CSV scanner (x86-64, AVX2 + BMI CPUs only) |

**Dispatch** ([`src/dispatch.c`](src/dispatch.c)) runs once in a load-time constructor using `__builtin_cpu_supports`. Every x86-64 build contains the SSE and AVX2 code, compiled through `__attribute__((target(...)))`, so even a `-march=x86-64` library uses them on a CPU that has them. Setting `FP_FORCE_SCALAR=1` in the environment forces the scalar parsers. The tests use this to cover the fallback path.

**CSV column rules:** fields end at `,` or `\n`, and the last line may lack its `\n`. A row whose wanted field is missing or invalid is **skipped**: a header row, an empty field, a `\r` from CRLF line endings, or junk. The return value is how many values were stored, at most `max`.

---

## How each parser works

### 1. Reference: `fp_u64_ref`, `fp_px_ref` ([`src/parse_ref.c`](src/parse_ref.c))

The reference checks every byte, then accumulates digits in an `unsigned __int128`. Twenty digits are below 10²⁰ < 2¹²⁷, so the accumulation cannot overflow, and a single final comparison with `UINT64_MAX` decides overflow. It is meant to be read and believed, not to be fast.

### 2. Naive: `fp_u64_naive` ([`src/parse_naive.c`](src/parse_naive.c))

```c
for each byte:  d = byte - '0';  if (d > 9) return -FP_BAD_CHAR;  v = v * 10 + d;   // + overflow builtins
```

It has one data-dependent branch per byte, and the loop's trip count is the number's length. When lengths vary, the branch predictor guesses the loop exit wrong about once per number. That miss is the cost the other parsers remove.

### 3. SWAR, 8 digits per step: `fp_u64_swar` ([`src/parse_swar.c`](src/parse_swar.c))

SWAR ("SIMD within a register") loads 8 bytes into one `uint64_t` (little-endian, so the first digit is the lowest byte) and combines neighbours in three multiply-shift-mask steps. For `"12345678"`:

```
bytes - '0'               01 02 03 04 05 06 07 08        (lowest byte first)
v = v*10 + (v >> 8)       12 .. 34 .. 56 .. 78 ..        keep 0x00FF00FF00FF00FF   → pairs
v = v*100 + (v >> 16)     1234 ..... 5678 .....          keep 0x0000FFFF0000FFFF   → quads
v = v*10000 + (v >> 32)   12345678                       keep 0xFFFFFFFF           → eight
```

- **Digit check:** all 8 bytes are checked at once. Every byte must have high nibble `3`, and adding 6 must keep it `3`: `(x & 0xF0…) | ((x + 0x06…) & 0xF0…) >> 4 == 0x33…`.
- **Short numbers:** a number shorter than 8 is shifted into the top bytes, and the bytes below are filled with `'0'` (leading zeros). One code path then handles every length.
- **Longer numbers:** the first chunk takes the leading `(n − 1) % 8 + 1` digits, then full 8-digit chunks follow: `v = v × 10⁸ + chunk`.

### 4. Hand-written assembly: `fp_u64_asm` ([`src/swar_x86_64.S`](src/swar_x86_64.S))

This is the same algorithm in AT&T syntax, following the System V ABI. The arguments arrive in `rdi`, `rsi` and `rdx`, and the result goes in `eax`. It implements the *full* contract (every length, every error code), so it runs through every test the C parsers do.

- **Overflow:** detected with an unsigned `mul` (the high half lands in `rdx`) plus `adc`.
- **Registers:** `rbx` is saved and restored to hold the end pointer, because `mul` takes `rdx`.
- **Baseline instructions only:** no BMI2, so it runs under every `-march` in CI.

[`scripts/asm_dump.sh`](scripts/asm_dump.sh) puts it next to what gcc and clang emit for the C version. The compilers do several things it doesn't:

- **Peel the first chunk:** the first, partial chunk is handled before the loop, so the loop body never shifts or fills.
- **Keep constants in registers:** the six 64-bit constants stay in registers across chunks, paid for with push/pop. The hand-written version reloads them every chunk.
- **Use `lea` chains:** `lea (x,x,4)` then `lea (y,x5,2)` multiply by ×10 and ×100 instead of `imul`.
- **Use BMI2:** `bzhi`/`shlx` handle the fill mask and the variable shift.

The result is that the hand-written version wins on `fixed8` with gcc (213 vs 196 Mnum/s; 81 vs 97 instructions per number) and ties on `uniform`, but clang's SWAR beats it.

### 5. SSE, 16 bytes per step: `fp_u64_sse`, `fp_px_sse` ([`src/parse_sse.c`](src/parse_sse.c))

```
load 16 bytes             _mm_loadu_si128                      (up to 15 bytes past the field: FP_PAD)
digit values              _mm_sub_epi8(x, '0')
which lanes are digits    max_epu8(d, 9) == 9  →  movemask      (one compare: unsigned d ≤ 9)
right-align, zero-fill    _mm_shuffle_epi8(d, index)
pairs                     _mm_maddubs_epi16(d, (10,1)…)
quads                     _mm_madd_epi16(x, (100,1)…)
back to 16-bit lanes      _mm_packus_epi32(x, x)
eights                    _mm_madd_epi16(x, (10000,1)…)
result                    hi8 * 10^8 + lo8
```

- **Computed shuffle index, no lookup table:** `index = lane + m − 16`. Below the m digits it is negative, so its high bit is set, and `pshufb` writes 0 for such lanes. The same instruction right-aligns the digits and zero-fills the leading lanes.
- **Prices:** `pcmpeqb` against `'.'` plus `movemask` finds the dot, and `tzcnt` gives its position, which is the integer digit count. The same index minus `cmpgt(index, dot − 1)` makes lanes at or after the dot read one lane further, deleting the dot in the same shuffle. Scaling is a table lookup, `value × pow10[8 − fraction_digits]`, not a branch.
- **Long inputs:** integers of 17–20 digits take their first 1–4 digits one at a time, then one 16-byte step. Prices longer than 16 bytes go to the reference (see [limitations](#limitations-and-known-corners)).

### 6. AVX2 CSV column: `fp_px_column_avx2` ([`src/parse_column.c`](src/parse_column.c))

The scanner loads 32 bytes at a time, compares them against `,` and `\n`, and turns each comparison into a 32-bit mask with `movemask`. It then walks the set bits with `tzcnt`, and every field in the wanted column goes to the SSE price kernel. Bits past the end of the buffer are masked off. The reference version walks bytes one by one and shares the same per-field step (`delimiter()`), so both follow identical rules.

---

## What the numbers say

- **Branch misses explain most of the gap.** On `fixed8`, where every number has 8 digits, all parsers mispredict about 0.001 times per number. On `uniform` (1–19 digits) naive and ref mispredict once per number, and their cost rises by about 60% (39 → 63 ticks for naive).
- **8-digit SWAR is not flat.** It still mispredicts 0.63 times per number on `uniform`, because its chunk loop runs 1, 2 or 3 times depending on the length. So it is twice as fast as naive on `fixed8` but only 1.76× on `uniform`.
- **SSE is close to length-independent.** It runs 70–73 instructions per number whatever the length (cachegrind: 60 vs 63), and 13 → 20 ticks. The remaining 0.28 misses per number come from the `n > 16` branch, which 3 of the 19 possible lengths take.
- **Latency mode reverses the ranking.** When each input depends on the previous result, SSE costs about 32 ticks, *slower* than SWAR's 25. SSE's single dependency chain is long: vector load, subtract, compare, `movemask`, shuffle, three multiply-adds, then back to scalar. It only wins when the CPU can overlap several parses. If a strategy parses one field and acts on it immediately, measure latency mode, not throughput.
- **Real data is easier than synthetic data.** Every Binance price is 14 bytes (`83169.01000000`), so `fp_px_sse` almost never mispredicts on it (0.001 misses per number, against 0.26 on the synthetic `prices` corpus) and takes 17 ticks instead of 29. The reference gains too (65 vs 100 ticks).
- **Compilers differ more than expected.** clang turns the `__int128` reference into code as fast as naive on `fixed8` (34.7 vs 35.3 ticks), and its AVX2 column scanner is 30% faster than gcc's.

---

## How correctness is checked

A fast version only counts once it returns exactly what the reference returns: same value, same error code, for every input.

| check | what it covers |
|---|---|
| [`test_edge.c`](tests/test_edge.c) | Every edge case from the plan: empty field, leading zeros, `UINT64_MAX` and one above it, 19 / 20 / 21 digits, `.5`, `5.`, 8 vs 9 fraction digits, a digit followed by `,` `\n` `.` or NUL inside and just past the field, high bytes, SSE boundaries (16/17 bytes, a bad first or last lane). A **page test** places fields so they end at a page boundary, and also exactly `FP_PAD` bytes before a `PROT_NONE` page, so any read past the padding contract crashes. It also checks that dispatch picked what the CPU supports. |
| [`test_exhaustive.c`](tests/test_exhaustive.c) | **Every** digit string of 1–8 digits, leading zeros included (111,111,110 strings), through every integer parser, with digits placed after the field to catch reads past `n`. About 3 s. |
| [`test_diff.c`](tests/test_diff.c) | Differential fuzz: random digit/dot/junk strings of 0–24 bytes, every parser against its reference, with a fixed seed so failures replay. 1M by default, **100M integers + 100M prices in CI** (`FP_FUZZ_ITERS`). |
| [`test_column.c`](tests/test_column.c) | 200k random CSVs × columns 0–3 × (no limit, `max = 3`), with valid-looking fields *in the padding* that must be ignored. If `data/binance_trades.txt` exists, the price and qty columns of the real file must match the reference value for value. **They do, on all 3,558,844 rows.** |
| [`fuzz_parse.c`](tests/fuzz_parse.c) | A libFuzzer target per group (integers, prices, CSV columns) under ASan + UBSan. It traps on any disagreement with the reference. Runs nightly in CI for 10 minutes per group. |
| Sanitizers | The whole suite under `-fsanitize=address,undefined -fno-sanitize-recover=all`, gcc and clang, x86-64 and ARM. |
| [`selftest.sh`](scripts/selftest.sh) | Tests the tests. On a throwaway copy of the repo it checks that a broken test fails, a compiler warning fails the build, the corpus generator is deterministic, the benchmark rejects bad input, and so on (17 checks). |

**Mutation testing.** Every parser was checked by planting bugs by hand and confirming the suite fails:

| parsers | planted bugs | caught |
|---|--:|--:|
| reference + naive | 9 | 8 |
| SWAR + asm | 12 | 12 |
| SSE + dispatch | 14 | 14 (dispatch only after adding a check) |
| CSV column | 8 | 7 |

The misses changed nothing observable, so they were equivalent to the original code, not bugs. Some planted bugs exposed holes in the tests themselves, which were then fixed:

- **Dispatch:** choosing the wrong parser still gives right answers, so a dispatch check was added.
- **CSV padding:** delimiter-only padding hides end-of-buffer bugs, so the padding now holds digits too.

[AUDIT_LOG.md](AUDIT_LOG.md) lists each one.

---

## How performance is measured

**Clock.** `lfence; rdtsc` before and `rdtscp; lfence` after, in inline asm ([`bench/tsc.h`](bench/tsc.h)). It is calibrated once against `CLOCK_MONOTONIC` over 100 ms. `run_bench.sh` refuses to run without `constant_tsc` and `nonstop_tsc`.

**Batching.** One parse takes a few nanoseconds, about as long as the timer itself, so batches of 1,024 numbers are timed and divided. Every batch time is kept and sorted. The reports give **p50, p99, p99.9 and max**, never only the mean.

**Two modes.**
- *Throughput:* independent calls.
- *Latency:* each next input address depends on the previous result. The code adds `v & zero`, where `zero` comes from a `volatile` that is always 0, so the CPU cannot overlap calls. Results go into a `volatile` sink, so the compiler cannot delete the work.

**Hardware counters.** cycles, instructions, branches and branch misses come from `perf_event_open`, opened as a group around the timed loop only, so file loading, calibration and warm-up are excluded.

**Machine setup.** [`tune_cpu.sh`](scripts/tune_cpu.sh) sets:
- the performance governor on every core;
- turbo/boost off, so TSC ticks equal core cycles;
- `perf_event_paranoid=2`.

It saves the old values, and `--restore` puts them back. `run_bench.sh` pins to core 3 and records CPU, kernel, compiler, flags, governor and boost in each results JSON. On laptops, `power-profiles-daemon` can reset the governor behind your back: run `powerprofilesctl set performance` first and check it afterwards.

**Corpora.**

| corpus | contents | made by | what it tests |
|---|---|---|---|
| `fixed8` | 10M integers, exactly 8 digits | `gen_corpus.sh` | best case for branch prediction |
| `uniform` | 10M integers, 1–19 digits uniformly | `gen_corpus.sh` | branch mispredicts |
| `prices` | 10M prices, 1–10 integer and 0–8 fraction digits | `gen_corpus.sh` | the decimal-point path |
| `binance` | 3.56M real BTCUSDT prices | `fetch_binance.sh` | real length distribution |
| `binance_trades` | the full trades CSV, 270 MB | `fetch_binance.sh` | the AVX2 column scanner |

`gen_corpus.sh` uses its own fixed-seed generator (Park–Miller), because Ubuntu's `mawk` ignores `srand()`. `fetch_binance.sh` checks the file against Binance's published sha256.

**Reproduce.**

```bash
powerprofilesctl set performance        # laptops with power-profiles-daemon
sudo ./scripts/tune_cpu.sh
./scripts/run_bench.sh --cc gcc         # ~2 min corpus generation the first time
./scripts/run_bench.sh --cc clang
./scripts/render_results.sh             # → results.md
sudo ./scripts/tune_cpu.sh --restore
```

**Caveats.** p99.9 is noisy because core 3 isn't isolated from interrupts. "Ticks" are TSC ticks, which equal core cycles only with boost off; the `perf` columns give true cycles. Numbers are from one machine, so measure your own.

---

## CI/CD

GitHub Actions calls the same scripts you run locally, so CI cannot drift from your laptop.

| workflow | trigger | what it does | blocks merge |
|---|---|---|---|
| [`ci.yml`](.github/workflows/ci.yml) | push, PR | Builds and tests with gcc and clang at `-march=x86-64` / `x86-64-v2` / `x86-64-v3`, plus gcc on ARM (`ubuntu-24.04-arm`). Runs the 100M-string fuzz, a forced-scalar pass on `x86-64`, `selftest.sh`, and the Binance totals check. | yes |
| [`sanitizers.yml`](.github/workflows/sanitizers.yml) | PR | Full suite under ASan + UBSan: gcc and clang on x86, gcc on ARM | yes |
| [`lint.yml`](.github/workflows/lint.yml) | PR | `clang-format --dry-run --Werror`, `shellcheck`, `cppcheck` | yes |
| [`asm.yml`](.github/workflows/asm.yml) | PR touching `src/` | gcc vs clang vs hand-written disassembly and instruction counts, posted as one PR comment; disassembly uploaded as an artifact | no |
| [`bench.yml`](.github/workflows/bench.yml) | push to main, PR, manual | Instructions per number with cachegrind, compared with the last main run (warns on a rise of more than 5%), plus each parser's speed relative to naive on the same runner | warns |
| [`fuzz.yml`](.github/workflows/fuzz.yml) | nightly, manual | libFuzzer, 10 minutes per parser group; uploads the crash and opens an issue | no |
| [`release.yml`](.github/workflows/release.yml) | tag `v*` | Builds a baseline x86-64 `libfastparse.a` (SIMD still chosen at runtime), runs the tests, and publishes a GitHub Release with the library, header, README and `results.md` | — |

Shared CI runners give unreliable wall-clock times, so CI tracks two stable signals instead: **instruction counts** (deterministic for the same binary and input) and **speed relative to naive** (the runner's speed cancels out). Real timing happens locally.

---

## Repository layout

```
include/fastparse.h          the whole API, the input contract, error codes, FP_PAD
src/
  parse_ref.c                references (__int128 integers, prices)
  parse_naive.c              per-digit loop
  parse_swar.c               8 digits per step in a uint64_t
  swar_x86_64.S              the same, hand-written x86-64 assembly
  parse_sse.c                16-byte SSSE3/SSE4.1 kernels for integers and prices
  parse_column.c             CSV column: byte-loop reference + AVX2 scanner
  dispatch.c                 fp_u64 / fp_px / fp_px_column / fp_impl, picked at load time
tests/
  helpers.h                  parser lists, CPU checks, padded() for the FP_PAD contract
  test_edge.c  test_exhaustive.c  test_diff.c  test_column.c  test_smoke.c
  fuzz_parse.c               libFuzzer target
bench/
  tsc.h                      fenced rdtsc / rdtscp, calibration
  bench.c                    timing harness: batches, percentiles, perf counters, JSON
scripts/                     every build, test, benchmark and release step (see below)
results/                     committed JSON from tuned local runs
results.md                   rendered from results/
AUDIT_LOG.md                 dated log of every action, command and decision
.github/workflows/           the seven workflows above
```

---

## Scripts

| script | purpose |
|---|---|
| `build.sh [--cc] [--march] [--san] [--lib]` | build tests, `build/bench` (x86) and optionally `libfastparse.a` |
| `test.sh` | run every test binary; fails if any fails |
| `selftest.sh` | check that the tooling catches broken code |
| `gen_corpus.sh [count]` | `fixed8`, `uniform`, `prices` (10M each by default, deterministic) |
| `fetch_binance.sh [SYMBOL] [DATE]` | one day of Binance spot trades, sha256-checked (default BTCUSDT 2026-10-14) |
| `tune_cpu.sh [--restore]` | (root) governor, boost, perf counters for benchmarking |
| `run_bench.sh [--cc] [--core]` | full benchmark, pinned; writes `results/<date>-<HHMM>-<cc>.json` |
| `render_results.sh` | newest results per compiler → `results.md` |
| `count_instr.sh [baseline.tsv]` | cachegrind instructions per number; warns on a rise of more than 5% |
| `asm_dump.sh [--march]` | gcc / clang / hand-written disassembly into `build/asm/`, with instruction counts |
| `fuzz.sh u64\|px\|column [seconds]` | libFuzzer run; crashes saved to `build/fuzz/` |

---

## Limitations and known corners

- **SIMD is x86-64 only.** ARM builds use SWAR and the reference parsers; a NEON port is a listed stretch goal.
- **Little-endian only:** the SWAR code refuses to compile otherwise.
- **Prices longer than 16 bytes** (10 integer digits plus 6–8 fraction digits) take the scalar reference path inside `fp_px_sse`. That is about 7% of the synthetic `prices` corpus and none of the Binance data. A second load would remove it.
- **Integer fields longer than 20 bytes** are always errors, and `fp_u64_sse` hands them to SWAR to determine which error.
- **CRLF files:** with the skip-bad-rows rule, a `\r` makes the last field of a row invalid. Strip `\r` first, or read a column that isn't last.
- **`FP_PAD` is a hard requirement.** Passing an unpadded buffer near the end of a page can crash.
- **No license file yet.** Add one before reusing the code.

---

## How it was built

The project was built in six phases. Each phase was one or more PRs, opened only once CI passed, with a before/after table in the PR description:

| phase | PR | what landed |
|---|---|---|
| 0. Repo + CI skeleton | [#1](https://github.com/migikun474/fastparse/pull/1), [#2](https://github.com/migikun474/fastparse/pull/2) | scripts, `ci.yml`, `lint.yml`, the tooling self-test |
| 1. Reference + naive | [#3](https://github.com/migikun474/fastparse/pull/3) | reference and naive parsers, edge and differential tests, sanitizers, corpus generator |
| 2. Benchmark harness | [#4](https://github.com/migikun474/fastparse/pull/4) | rdtsc harness, perf counters, CPU tuning, cachegrind in CI |
| 3. SWAR + asm | [#5](https://github.com/migikun474/fastparse/pull/5) | SWAR in C and assembly, exhaustive test, `asm.yml` |
| 4. SSE | [#6](https://github.com/migikun474/fastparse/pull/6) | SSE integers and prices, runtime dispatch, 100M fuzz |
| 5. AVX2 + release | [#7](https://github.com/migikun474/fastparse/pull/7), [#8](https://github.com/migikun474/fastparse/pull/8) | AVX2 CSV scanner, Binance data, libFuzzer, release workflow, `results.md` |

[AUDIT_LOG.md](AUDIT_LOG.md) records every command, file change and decision, with dates. It also records where the plan was ambiguous and how that was resolved, and every mistake found along the way.

---

## Further reading

- Daniel Lemire's blog posts on SWAR and SIMD number parsing
- [fast_float](https://github.com/fastfloat/fast_float): fast, exact decimal-to-float parsing
- [simdjson](https://github.com/simdjson/simdjson): SIMD JSON parsing; the source of the padding rule
- Intel's [Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/) for every `_mm_*` / `_mm256_*` used here
