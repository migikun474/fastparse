#!/usr/bin/env bash
# shellcheck disable=SC2317  # helper functions are called through expect_ok
# Check that the build/test tooling itself works: good code passes, bad code fails.
# Runs on a throwaway copy of the repo, so nothing here is modified.
# Usage: CC=gcc|clang scripts/selftest.sh
set -euo pipefail
cd "$(dirname "$0")/.."

cc=${CC:-gcc}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cp -r bench include scripts src tests "$tmp"/
cd "$tmp"

fails=0
pass() { echo "ok   $1"; }
fail() { echo "FAIL $1"; fails=$((fails + 1)); }
expect_ok() { if "${@:2}" >/dev/null 2>&1; then pass "$1"; else fail "$1"; fi; }
expect_fail() { if "${@:2}" >/dev/null 2>&1; then fail "$1"; else pass "$1"; fi; }

expect_ok "header compiles on its own" \
    sh -c "echo '#include \"fastparse.h\"' | $cc -fsyntax-only -Wall -Wextra -Werror -Iinclude -x c -"
expect_fail "build.sh rejects unknown option" scripts/build.sh --bogus
expect_fail "test.sh fails when nothing is built" scripts/test.sh
expect_ok "clean tree builds" scripts/build.sh --cc "$cc"
expect_ok "clean tree passes tests" scripts/test.sh

sed -i 's/FP_PAD == 64/FP_PAD == 63/' tests/test_smoke.c
scripts/build.sh --cc "$cc" >/dev/null
expect_fail "broken test fails test.sh" scripts/test.sh
sed -i 's/FP_PAD == 63/FP_PAD == 64/' tests/test_smoke.c
scripts/build.sh --cc "$cc" >/dev/null
expect_ok "fixed test passes test.sh again" scripts/test.sh

scripts/gen_corpus.sh 5000 >/dev/null
corpus_ok() { [ "$(wc -l <"data/$1.txt")" -eq 5000 ] && ! grep -Eqv "$2" "data/$1.txt"; }
expect_ok "fixed8 corpus: 5000 8-digit numbers" corpus_ok fixed8 '^[1-9][0-9]{7}$'
expect_ok "uniform corpus: 5000 1-19 digit numbers" corpus_ok uniform '^[1-9][0-9]{0,18}$'
expect_ok "prices corpus: 5000 valid prices" corpus_ok prices '^[1-9][0-9]{0,9}(\.[0-9]{1,8})?$'
same_twice() { cksum data/*.txt >sum && scripts/gen_corpus.sh 5000 >/dev/null && cksum data/*.txt | cmp -s - sum; }
expect_ok "gen_corpus.sh output is the same every run" same_twice

if [ "$(uname -m)" = x86_64 ]; then # build/bench is x86-only
    bench_clean() { build/bench "$1" "data/$2.txt" count | grep -q '"numbers":5000,"errors":0,'; }
    expect_ok "bench: naive parses fixed8 with no errors" bench_clean fp_u64_naive fixed8
    expect_ok "bench: px_ref parses prices with no errors" bench_clean fp_px_ref prices
    expect_fail "bench: naive on prices reports errors" bench_clean fp_u64_naive prices
    expect_fail "bench rejects an unknown parser" build/bench nope data/fixed8.txt count
    expect_ok "bench: latency mode runs" build/bench fp_u64_naive data/uniform.txt latency
fi

echo 'int main(void) { int unused; return 0; }' >tests/test_warn.c
expect_fail "compiler warning fails build.sh" scripts/build.sh --cc "$cc"

exit "$((fails != 0))"
