#!/usr/bin/env bash
# Write the benchmark corpora to data/<name>.txt, one number per line:
#   fixed8   exactly 8 digits
#   uniform  1 to 19 digits, length uniform at random
#   prices   1 to 10 integer digits, then 0 to 8 fraction digits (no '.' when 0)
# Usage: scripts/gen_corpus.sh [count]   (default 10000000 per corpus)
set -euo pipefail
cd "$(dirname "$0")/.."

count=${1:-10000000}
mkdir -p data
# awk's own rand() can't be trusted for this: Ubuntu's mawk ignores srand(42). rnd() is the
# Park-Miller generator; every step is exact in a double, so any awk gives the same corpus.
for kind in fixed8 uniform prices; do
    awk -v n="$count" -v kind="$kind" '
        function rnd() { seed = (seed * 16807) % 2147483647; return seed / 2147483647 }
        function digits(k, s) { while (length(s) < k) s = s int(rnd() * 10); return s }
        function num(k) { return digits(k, int(1 + rnd() * 9)) }
        BEGIN {
            seed = 42
            for (i = 0; i < n; i++) {
                if (kind == "fixed8") print num(8)
                else if (kind == "uniform") print num(1 + int(rnd() * 19))
                else { f = int(rnd() * 9); print num(1 + int(rnd() * 10)) (f ? "." digits(f, "") : "") }
            }
        }' >"data/$kind.txt"
    echo "data/$kind.txt: $count lines"
done
