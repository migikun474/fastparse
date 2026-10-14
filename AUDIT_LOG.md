# Audit log

Every action taken on this repo, newest phase last. Times are IST.

## 2026-10-15 — Phase 0: repo + CI skeleton

Source of truth: `SIMD number parser plan.md`, section "Implementation phases", row 0.

### Actions

| Time | Action | Commands / files | Why |
| --- | --- | --- | --- |
| 02:13 | Inspected repo and tools | `ls -la`, `git log`, `git remote -v`, `git config user.name/user.email`, `which gcc clang shellcheck clang-format cppcheck` | Check state before changing anything |
| 02:13 | Git init and remote: **no action needed** | none | Repo already initialised (commit `18e8bbc`), `origin` already set to `https://github.com/migikun474/fastparse.git` |
| 02:15 | Asked user two questions | none | Commit identity was ambiguous; the plan names no clang-format style |
| 02:16 | Created branch | `git checkout -b phase-0-skeleton` | User asked for this branch |
| 02:16 | Checked tool versions | `gcc --version`, `clang --version`, `clang-format --version`, `uname -m` | Record the local toolchain: gcc 13.3.0, clang 18.1.3, clang-format 18.1.3, x86_64 |
| 02:17 | Created directories | `mkdir -p include tests scripts .github/workflows` | Only the dirs Phase 0 uses |
| 02:18 | Added files | `include/fastparse.h`, `tests/test_smoke.c`, `scripts/build.sh`, `scripts/test.sh`, `.clang-format`, `.gitignore` | See decisions |
| 02:19 | Added workflows | `.github/workflows/ci.yml`, `.github/workflows/lint.yml` | Phase 0 CI |
| 02:20 | Built and tested 8 configs | `chmod +x scripts/*.sh`; `scripts/build.sh --cc {gcc,clang} --march {x86-64,x86-64-v2,x86-64-v3,native}` + `scripts/test.sh` | All 8 passed |
| 02:20 | Format check failed, fixed | `clang-format --dry-run --Werror ...` reported comment alignment in `fastparse.h`; `clang-format -i include/fastparse.h tests/test_smoke.c` | Lint would have failed in CI |
| 02:20 | Broken-test check (local stand-in for the PR check) | `sed` changed `FP_PAD == 64` to `== 63` → `test.sh` exit 1; reverted → exit 0; `build.sh --bogus` → exit 2 | Shows the gate trips and clears |
| 02:20 | Stray commands, no effect | `git checkout -- .` (only tracked file was the unchanged `README.md`), `rm -rf /tmp/x` (path did not exist) | Leftovers in a test command; logged for completeness |
| 02:21 | Quoted `exit "$fail"` in `test.sh` | edit | Avoid a shellcheck SC2086 warning |
| 02:21 | Added "Build and test" section | `README.md` | One place that shows the two commands |
| 02:22 | Wrote this log and committed | `git add -A`, `git commit` (author `migikun474 <amirishettysaivignesh1@gmail.com>`, no co-author trailer) | User's instruction |
| 02:23 | Asked user two questions | none | No parsers exist yet, so "test cases" could mean skeleton tests or Phase 1 tests; running GitHub CI needs a push. User chose skeleton self-tests, and will push themselves |
| 02:24 | Added skeleton self-test | `scripts/selftest.sh`; `ci.yml` gets the step `CC=${{ matrix.cc }} ./scripts/selftest.sh`; README line | Checks the tooling itself in CI: good code passes, bad code fails |
| 02:25 | Ran selftest | `CC=gcc scripts/selftest.sh`, `CC=clang scripts/selftest.sh` | 8/8 checks passed on both compilers |
| 02:25 | Checked selftest catches a regression | `sed` made `test.sh` always `exit 0` → selftest exit 1 (2 checks failed); restored with `git checkout -- scripts/test.sh` | A test that can't fail proves nothing |
| 02:26 | Committed | `git add -A`, `git commit` (same author, no co-author trailer) | |
| 02:40 | Push and log read blocked, user pushed and opened the PR | user ran `git push`, `gh pr create` | Permission check stopped my push; user did it |
| 02:45 | Read failing lint log (user allowed it) | `gh run view <id> --log-failed` | `ci`: 14/14 green. `lint`: cppcheck `knownConditionTrueFalse` on `CHECK(FP_OK == 0)` in `tests/test_smoke.c` |
| 02:46 | Suppressed that rule for that file only | `lint.yml`: `--suppress=knownConditionTrueFalse:tests/test_smoke.c` | The smoke test compares constants on purpose; the rule stays on for every other file |

### Decisions

- **Commit identity:** `migikun474 <amirishettysaivignesh1@gmail.com>` from git config (user's choice; the initial commit used a different address). No `Co-Authored-By` line. Nothing pushed.
- **clang-format style:** LLVM, 4-space indent, 100 columns (user's choice).
- **Header:** constants, `fp_status` and the two function-pointer typedefs only. Parser prototypes are added in the phase that implements each parser, so the header never declares functions that don't exist.
- **Trivial test:** `tests/test_smoke.c` checks the header constants. Its only job is proving build → test → CI wiring.
- **`build.sh`:** compiles each `tests/*.c` with all `src/*.c` (none yet; `nullglob` makes the empty case work). Flags from the plan: `-O2 -march=<arch> -fno-omit-frame-pointer`, plus `-Wall -Wextra -Werror` so warnings fail CI. `--cc` and `--march` match the plan's `ci.yml`.
- **`ci.yml`:** the plan's YAML, minus the `upload-artifact` step for `build/failures/`. Nothing writes that directory until Phase 1 tests exist.
- **`lint.yml`:** runs on PRs, per the plan's table. clang-format and cppcheck run on `git ls-files` output so missing directories don't break them. cppcheck is installed with apt because the runner image doesn't include it.
- **`selftest.sh`:** copies `include/ scripts/ tests/` into a `mktemp -d` dir and checks: the header compiles alone, `build.sh` rejects bad options, `test.sh` fails with nothing built, the clean tree builds and passes, a broken test fails, the fix passes again, and a compiler warning fails the build. Because it runs on a copy, the real tree is never edited. It runs in every `ci` matrix job, using that job's compiler.
- **Skipped (YAGNI):** `Makefile`, the empty `src/`, `bench/`, `results/` and `data/` dirs, and the other 5 workflows. Each phase adds what it needs.

### Not verified locally

- `shellcheck` and `cppcheck` are not installed on this machine, so they were not run here. They will first run in `lint.yml` on the PR.
- The ARM job (`ubuntu-24.04-arm`) can only run on GitHub.

### Left for the user (needs a push or GitHub settings)

1. Push the branch and open a draft PR: `git push -u origin phase-0-skeleton`.
2. Branch protection on `main`: require a PR, and require the `lint` check plus each `ci` matrix check. Add these in Settings → Branches once the checks have run once, so GitHub shows their exact names.
3. Phase 0 "done when": push a commit that breaks `test_smoke.c` (for example `FP_PAD == 63`), confirm the PR is blocked, then push the fix and confirm it turns green.

## 2026-10-15 — Phase 1: reference + naive

Source of truth: plan row 1: `fp_u64_ref`, `fp_u64_naive`, `fp_px_ref`, `gen_corpus.sh`, differential + edge-case tests, `sanitizers.yml`. Done when all tests pass on gcc + clang, x86-64 + ARM, under ASan/UBSan.

### Actions

| Time | Action | Commands / files | Why |
| --- | --- | --- | --- |
| 02:30 | Checked Phase 0 merged | `git status`, `git fetch origin`, `git log origin/main` | PRs #1 and #2 merged into `main` (`967140e`) |
| 02:32 | Asked user three questions | none | The plan contradicts itself on field semantics, the 19 vs 20 digit limit, and EMPTY vs BAD_CHAR. Answers: whole field; full u64 up to 20 digits; EMPTY only for n=0 |
| 02:34 | Created branch from merged main | `git checkout --no-track -b phase-1-reference-naive origin/main`, `mkdir -p src` | One branch per phase |
| 02:35 | Added parsers | `include/fastparse.h` (contract comment + 3 prototypes), `src/parse_ref.c`, `src/parse_naive.c` | Phase 1 parsers |
| 02:35 | Added tests | `tests/padded.h`, `tests/test_edge.c`, `tests/test_diff.c` | Edge + differential tests |
| 02:36 | Added scripts and CI | `scripts/build.sh --san`, `scripts/gen_corpus.sh`, `.github/workflows/sanitizers.yml`, `scripts/selftest.sh` (copies `src/`, corpus checks), `.gitignore` (`data/`), `clang-format -i` | Phase 1 tooling |
| 02:36 | First build: clang failed | `-Wbitwise-instead-of-logical` on `mul_overflow(...) \| add_overflow(...)` in `parse_naive.c` | Split into two `overflow \|=` lines. `\|\|` would have been wrong: it skips the add after a multiply overflow |
| 02:37 | Full local matrix | gcc + clang × `x86-64`, `x86-64-v2`, `x86-64-v3`, `native`, plus `--san` for both; `selftest.sh` for both; `clang-format --dry-run --Werror`; `bash -n` | All pass. `test_diff`: 0 of 1,000,000 mismatched. Sanitizer run takes 0.5 s |
| 02:37 | Mutation check of the tests | 9 hand-made bugs via `sed` on `src/*.c`, originals restored from scratchpad copies | 8 caught. The 9th (dropping the `unsigned char` cast) behaves the same, since a negative byte still wraps to a large unsigned value, so it is not a bug |
| 02:38 | Generated full corpora | `scripts/gen_corpus.sh` | 3 × 10M lines, 300 MB, 1 min 56 s |
| 02:39 | **Found: corpus not deterministic** | two `gen_corpus.sh 5000` runs, `md5sum` differed | Ubuntu's mawk 1.3.4 ignores `srand(42)` |
| 02:40 | Fixed it | `gen_corpus.sh`: `rand()` replaced by Park-Miller `rnd()` (seed 42, exact in doubles) | Same corpus on every run and with any awk. Lengths come out evenly spread (checked with `uniq -c`) |
| 02:41 | Added a determinism check to selftest | `same_twice` in `selftest.sh`; checked that it fails when `rand()` is put back | Guards the bug just fixed |
| 02:41 | Removed generated data | `rm -rf data` | Corpora are not committed |
| 02:42 | Committed | `git add -A`, `git commit` (author `migikun474 <amirishettysaivignesh1@gmail.com>`, no co-author trailer) | Not pushed |
| 02:50 | User pushed; 2 checks failed | `gh run list --branch phase-1-reference-naive`, `gh run view <id> --log-failed` for `ci` and `lint` | `sanitizers` passed. `ci` push run passed, but the PR run failed on one job |
| 02:51 | Cause 1: clang 18 + `-march=native` on an AVX10.1 runner | job `test (ubuntu-latest, clang, x86-64-v2)`, step `selftest.sh` (which builds with `native`): `invalid feature combination: +avx10.1-256` under `-Werror` | Depends on which CPU the runner gets, which is why the same commit passed on push. Reproduced locally with `clang -march=sapphirerapids -mavx10.1-256 -Werror` (exit 74) |
| 02:52 | Fix 1 in `build.sh` | added `-Wno-invalid-feature-combination` (gcc accepts unknown `-Wno-*`) | Every build goes through `build.sh`, so this one line covers ci, sanitizers and selftest. Checked with a clang wrapper adding `-mavx10.1-256`: `build.sh` without the fix fails (via `git stash`), with it passes |
| 02:52 | Cause 2 + fix: shellcheck | SC2054 on `-fsanitize=address,undefined` in an array → quoted it. SC2317 on the selftest helpers called through `expect_ok` → file-level `# shellcheck disable=SC2317` | Both are false positives |
| 02:53 | Re-ran everything | gcc + clang, normal + `--san`, `selftest.sh` 12/12 for both, `bash -n` | All pass. shellcheck itself is still not available locally |
| 02:53 | Committed | same author, no trailer | Not pushed |

### Decisions

- **Field semantics (user):** a parser reads exactly `[p, p+n)` and returns `n` on success. Bytes past `n` are never interpreted. Errors are checked in this order: `n == 0` → EMPTY; any byte outside the format → BAD_CHAR; then length/value → OVERFLOW; then too many fraction digits → TOO_MANY_FRAC. `*out` is written only on success. All of this is in the `fastparse.h` comment.
- **u64 limit (user):** 1–20 digits and ≤ `UINT64_MAX`. Leading zeros count toward the 20.
- **BAD_CHAR beats OVERFLOW (my call, reported to user):** each case has exactly one right answer, so every future parser must match the reference exactly.
- **Price rules come from the plan's regex:** `.5` and `.` → BAD_CHAR (it needs at least one integer digit); `5.` is valid. 11+ integer digits → OVERFLOW.
- **Naive overflow:** `__builtin_mul_overflow` / `__builtin_add_overflow` plus the `n > 20` flag, so it matches the reference without `__int128`.
- **Tests pad every input with `'9'`** (`padded.h`, `aligned_alloc(64, …)` + FP_PAD). A parser that reads past `n` gets a different value, which the mutation check confirmed.
- **Differential test:** 1M random strings of 0–24 bytes, 1 byte in 10 junk, splitmix64 with seed 42. It prints up to 10 failing inputs as hex. That replaces the plan's `build/failures/` upload, which was not added.
- **Sanitizer job matrix:** x86 gcc + clang, ARM gcc, the same shape as `ci.yml`. `--san` = `-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all`, so any sanitizer report fails the run.
- **`test_smoke.c` kept unchanged:** `selftest.sh` uses it as the test it deliberately breaks.
- **Skipped (YAGNI):** the exhaustive 1–8 digit test (plan puts it in phase 3 with SWAR); the page-boundary edge case (reference and naive never read past `n`, and ASan already covers the buffer end; add it with SWAR); `fp_px_naive` (not in the plan).

### Not verified locally

- `shellcheck` and `cppcheck` are not installed here. Their first run is in `lint.yml` on the PR.
- The ARM jobs only run on GitHub.

### Left for the user

1. `git push -u origin phase-1-reference-naive`, then open the PR.
2. Add the three `sanitizers` checks to `main`'s branch protection once they have run.
