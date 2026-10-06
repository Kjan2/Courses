#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"

# Google Benchmark drives these short checks. Kernel checks report SkipWithError
# and cause a nonzero process exit; no timings or statistics are implemented here.
for binary in build/bench-scalar build/bench-native; do
  "$binary" --benchmark_filter='/(4|5|7|31|4097)$' \
    --benchmark_min_time=3x --benchmark_repetitions=1 \
    --benchmark_min_warmup_time=0 > /dev/null
done
echo "All variants agree, including short inputs and tails."
