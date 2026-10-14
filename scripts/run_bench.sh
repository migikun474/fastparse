#!/usr/bin/env bash
# Benchmark every parser on its corpora (see `build/bench list`), throughput and latency modes,
# pinned to one core. Writes results/<date>-<HHMM>-<cc>.json and prints a markdown table.
# Run `sudo scripts/tune_cpu.sh` first for stable numbers and perf counters.
# Usage: scripts/run_bench.sh [--cc gcc|clang] [--core N]
set -euo pipefail
cd "$(dirname "$0")/.."

cc=gcc
core=3
while [ $# -gt 0 ]; do
    case $1 in
        --cc) cc=$2; shift 2 ;;
        --core) core=$2; shift 2 ;;
        *) echo "usage: $0 [--cc CC] [--core N]" >&2; exit 2 ;;
    esac
done

for flag in constant_tsc nonstop_tsc; do
    if ! grep -qw "$flag" /proc/cpuinfo; then
        echo "CPU lacks $flag: TSC timings would be wrong" >&2
        exit 1
    fi
done
gov=$(cat "/sys/devices/system/cpu/cpu$core/cpufreq/scaling_governor" 2>/dev/null || echo unknown)
boost=$(cat /sys/devices/system/cpu/cpufreq/boost /sys/devices/system/cpu/intel_pstate/no_turbo 2>/dev/null | head -1 || true)
[ "$gov" = performance ] || echo "warning: governor is $gov; run: sudo scripts/tune_cpu.sh" >&2

for c in fixed8 uniform prices; do # regenerate if missing or left small by a test run
    if [ ! -f "data/$c.txt" ] || [ "$(wc -l <"data/$c.txt")" -ne 10000000 ]; then
        scripts/gen_corpus.sh
        break
    fi
done
built=$(scripts/build.sh --cc "$cc")
mkdir -p results
out="results/$(date +%F-%H%M)-$cc.json"

build/bench list | while read -r parser corpus; do
    for mode in throughput latency; do
        echo "  $parser $corpus $mode" >&2
        taskset -c "$core" build/bench "$parser" "data/$corpus.txt" "$mode"
    done
done | jq -s --arg cpu "$(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2- | sed 's/^ //')" \
    --arg compiler "$("$cc" --version | head -1)" --arg build "$built" --arg governor "$gov" \
    --arg boost "${boost:-unknown}" --arg kernel "$(uname -r)" --arg date "$(date -Is)" \
    --argjson core "$core" --argjson reps 10 \
    '{date: $date, cpu: $cpu, kernel: $kernel, compiler: $compiler, build: $build, core: $core,
      governor: $governor, boost: $boost, reps: $reps, runs: .}' >"$out"

echo "$out"
jq -r '"| parser | corpus | mode | Mnum/s | MB/s | TSC/num p50 | p99 | p99.9 | cycles/num | instr/num | br-miss/num |",
       "|---|---|---|--:|--:|--:|--:|--:|--:|--:|--:|",
       (.runs[] | "| \(.parser) | \(.corpus) | \(.mode) | \(.mnum_per_s) | \(.mb_per_s) | \(.tsc_per_num.p50) | \(.tsc_per_num.p99) | \(.tsc_per_num.p999) | \(.perf.cycles // "-") | \(.perf.instructions // "-") | \(.perf.branch_misses // "-") |")' "$out"
