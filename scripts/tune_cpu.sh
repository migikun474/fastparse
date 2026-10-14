#!/usr/bin/env bash
# Put the CPU in a steady state for benchmarks (needs root): performance governor on every
# core, turbo/boost off, and perf_event_paranoid=2 so bench can read its own hardware counters.
# The old values are saved to /run (cleared on reboot, like the settings) for --restore.
# Usage: sudo scripts/tune_cpu.sh [--restore]
set -euo pipefail

saved=/run/fastparse-tune.saved
if [ "$(id -u)" != 0 ]; then
    echo "run with sudo" >&2
    exit 1
fi

if [ "${1:-}" = --restore ]; then
    while read -r f v; do echo "$v" >"$f"; done <"$saved"
    rm "$saved"
    echo "restored"
    exit 0
fi

files=(/sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
    /sys/devices/system/cpu/cpufreq/boost
    /sys/devices/system/cpu/intel_pstate/no_turbo
    /proc/sys/kernel/perf_event_paranoid)
if [ ! -e "$saved" ]; then # keep the first, untuned values if run twice
    for f in "${files[@]}"; do
        if [ -e "$f" ]; then echo "$f $(cat "$f")"; fi
    done >"$saved"
fi

put() {
    if [ -e "$1" ]; then
        echo "$2" >"$1"
        echo "$1 = $2"
    fi
}
for g in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do put "$g" performance; done
put /sys/devices/system/cpu/cpufreq/boost 0          # AMD and acpi-cpufreq
put /sys/devices/system/cpu/intel_pstate/no_turbo 1  # Intel
put /proc/sys/kernel/perf_event_paranoid 2
echo "undo with: sudo $0 --restore"
