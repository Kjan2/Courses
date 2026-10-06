#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
if (( $# < 2 )); then
  echo "Usage: bash profile.sh stat|record [perf options] -- BENCHMARK [arguments]" >&2
  exit 1
fi
mode=$1
shift
if [[ $mode != stat && $mode != record ]]; then
  echo "Mode must be stat or record" >&2
  exit 1
fi
# Open both ends before starting perf/the benchmark, avoiding FIFO-open deadlock.
mkdir -p build
# WSL checkouts on Windows drives may not support FIFOs. Use Linux temporary
# storage rather than the repository filesystem for these transient pipes.
control_dir=$(mktemp -d "${TMPDIR:-/tmp}/seminar-perf.XXXXXX")
trap 'rm -f -- "$control_dir/control" "$control_dir/ack"; rmdir -- "$control_dir"' EXIT
mkfifo "$control_dir/control" "$control_dir/ack"
exec {control_fd}<>"$control_dir/control"
exec {ack_fd}<>"$control_dir/ack"
export SEMINAR_PERF_CONTROL="$control_dir/control"
export SEMINAR_PERF_ACK="$control_dir/ack"
# Benchmark functions enable after setup/checks and disable after the state loop.
perf "$mode" --delay=-1 --control="fifo:$control_dir/control,$control_dir/ack" "$@"
