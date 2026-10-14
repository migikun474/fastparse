#!/usr/bin/env bash
# Build each tests/*.c (linked with src/*.c) into build/, plus build/bench on x86-64.
# Usage: scripts/build.sh [--cc gcc|clang] [--march x86-64|x86-64-v2|x86-64-v3|native] [--san]
#   --san: -O1 -g with ASan + UBSan; any sanitizer report aborts the test
set -euo pipefail
cd "$(dirname "$0")/.."

cc=${CC:-gcc}
march=native
opt=(-O2)
while [ $# -gt 0 ]; do
    case $1 in
        --cc) cc=$2; shift 2 ;;
        --march) march=$2; shift 2 ;;
        --san) opt=(-O1 -g "-fsanitize=address,undefined" -fno-sanitize-recover=all); shift ;;
        *) echo "usage: $0 [--cc CC] [--march ARCH] [--san]" >&2; exit 2 ;;
    esac
done

shopt -s nullglob
srcs=(src/*.c)
x86=
if [ "$(uname -m)" = x86_64 ]; then
    x86=yes
    srcs+=(src/*.S) # hand-written x86-64 assembly
fi
# -Wno-invalid-feature-combination: clang 18 with -march=native on AVX10.1 CPUs (some GitHub
# runners) warns about its own feature detection, which -Werror would turn into a build failure.
flags=("${opt[@]}" -march="$march" -fno-omit-frame-pointer -Wall -Wextra -Werror
    -Wno-invalid-feature-combination -Iinclude)
mkdir -p build
for t in tests/*.c; do
    "$cc" "${flags[@]}" "$t" "${srcs[@]}" -o "build/$(basename "$t" .c)"
done
if [ -n "$x86" ]; then # bench/tsc.h reads the x86 TSC
    "$cc" "${flags[@]}" bench/bench.c "${srcs[@]}" -o build/bench
fi
echo "built with $cc -march=$march ${opt[*]}"
