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
