#!/usr/bin/env bash
# Instructions per number inside each parser function, counted by cachegrind: deterministic for
# the same binary and input, so usable on noisy CI runners. Prints TSV: parser, corpus, count.
# With a baseline TSV, prints a GitHub ::warning:: for any rise of more than 5%.
# Usage: scripts/count_instr.sh [baseline.tsv]   (needs valgrind, build/bench and data/)
set -euo pipefail
cd "$(dirname "$0")/.."

baseline=${1:-/dev/null}
[ -f "$baseline" ] || baseline=/dev/null
tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT

build/bench list | while read -r parser corpus; do
    valgrind --tool=cachegrind --cache-sim=no --cachegrind-out-file="$tmp" \
        build/bench "$parser" "data/$corpus.txt" count >/dev/null 2>&1
    # cachegrind.out: "fn=<name>" starts a function, then "<line> <Ir>" rows
    awk -v fn="fn=$parser" -v n="$(wc -l <"data/$corpus.txt")" -v p="$parser" -v c="$corpus" '
        /^fn=/ { on = ($0 == fn) }
        on && /^[0-9]/ { ir += $2 }
        END { printf "%s\t%s\t%.2f\n", p, c, ir / n }' "$tmp"
done | awk -F'\t' -v base="$baseline" '
    FILENAME == base { old[$1 FS $2] = $3; next }
    { print }
    ($1 FS $2) in old && $3 > old[$1 FS $2] * 1.05 {
        printf "::warning::%s on %s: %.2f -> %.2f instructions/number (+%.1f%%)\n",
            $1, $2, old[$1 FS $2], $3, ($3 / old[$1 FS $2] - 1) * 100 > "/dev/stderr"
    }' "$baseline" -
