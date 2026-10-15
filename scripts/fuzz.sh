#!/usr/bin/env bash
# Fuzz one parser group with libFuzzer (clang, ASan + UBSan). A mismatch with the reference
# traps; the input is saved as build/fuzz/crash-* and the script exits non-zero.
# Usage: scripts/fuzz.sh u64|px|column [seconds]   (default 600)
set -euo pipefail
cd "$(dirname "$0")/.."

group=${1:-}
secs=${2:-600}
case $group in
    u64 | px | column) ;;
    *) echo "usage: $0 u64|px|column [seconds]" >&2; exit 2 ;;
esac
srcs=(src/*.c)
if [ "$(uname -m)" = x86_64 ]; then srcs+=(src/*.S); fi

mkdir -p "build/fuzz/corpus-$group"
clang -O1 -g -march=native -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all \
    -Wno-invalid-feature-combination -Iinclude "-DFUZZ_${group^^}" \
    tests/fuzz_parse.c "${srcs[@]}" -o "build/fuzz/fuzz-$group"
"build/fuzz/fuzz-$group" -max_total_time="$secs" -max_len=512 -artifact_prefix=build/fuzz/ \
    "build/fuzz/corpus-$group"
