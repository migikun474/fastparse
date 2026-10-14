#!/usr/bin/env bash
# Check that the build/test tooling itself works: good code passes, bad code fails.
# Runs on a throwaway copy of the repo, so nothing here is modified.
# Usage: CC=gcc|clang scripts/selftest.sh
set -euo pipefail
cd "$(dirname "$0")/.."

cc=${CC:-gcc}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cp -r include scripts tests "$tmp"/
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

echo 'int main(void) { int unused; return 0; }' >tests/test_warn.c
expect_fail "compiler warning fails build.sh" scripts/build.sh --cc "$cc"

exit "$((fails != 0))"
