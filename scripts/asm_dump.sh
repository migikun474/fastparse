#!/usr/bin/env bash
# Disassemble each u64 parser as built by gcc and clang, plus the hand-written fp_u64_asm, into
# build/asm/<function>-<compiler>.s, and print a markdown table of instruction counts.
# These are static counts (instructions in the function body, alignment nops excluded); the
# per-number executed counts come from scripts/count_instr.sh.
# Usage: scripts/asm_dump.sh [--march ARCH]   (default x86-64-v3, so runs are comparable; x86-64 only)
set -euo pipefail
cd "$(dirname "$0")/.."

march=x86-64-v3
if [ "${1:-}" = --march ]; then march=$2; fi
mkdir -p build/asm

dump() { # binary, function, output name -> instruction count
    objdump -d --no-show-raw-insn --disassemble="$2" "$1" | sed -n '/>:$/,$p' >"build/asm/$3.s"
    grep -E '^ +[0-9a-f]+:' "build/asm/$3.s" | grep -cvw nop || true
}

echo "| function | built by | instructions |"
echo "|---|---|--:|"
for cc in gcc clang; do
    scripts/build.sh --cc "$cc" --march "$march" >/dev/null
    cp build/bench "build/asm/bench-$cc"
    for fn in fp_u64_naive fp_u64_swar fp_u64_sse fp_px_sse; do
        echo "| $fn | $cc | $(dump "build/asm/bench-$cc" "$fn" "$fn-$cc") |"
    done
done
echo "| fp_u64_asm | hand-written | $(dump build/asm/bench-gcc fp_u64_asm fp_u64_asm) |"
