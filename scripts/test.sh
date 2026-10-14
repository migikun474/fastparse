#!/usr/bin/env bash
# Run every test binary built by build.sh; exit non-zero if any fails.
set -euo pipefail
cd "$(dirname "$0")/.."

fail=0
for t in build/test_*; do
    "$t" || fail=1
done
exit "$fail"
