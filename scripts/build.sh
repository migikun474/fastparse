#!/usr/bin/env bash
# Build each tests/*.c (linked with src/*.c) into build/.
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
        --san) opt=(-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all); shift ;;
        *) echo "usage: $0 [--cc CC] [--march ARCH] [--san]" >&2; exit 2 ;;
    esac
done

shopt -s nullglob
srcs=(src/*.c)
mkdir -p build
for t in tests/*.c; do
    "$cc" "${opt[@]}" -march="$march" -fno-omit-frame-pointer -Wall -Wextra -Werror -Iinclude \
        "$t" "${srcs[@]}" -o "build/$(basename "$t" .c)"
done
echo "built with $cc -march=$march ${opt[*]}"
