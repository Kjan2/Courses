# Recorded seminar results — 2026-10-06

These are actual Google Benchmark measurements for the separate seminar.
[Presentation 2](../../slides.md) ends with a tool introduction; the practical
results are kept here. The primary evidence is
[scalar.json](scalar.json) and [native.json](native.json), including every
repetition and the framework's aggregate statistics.

## Measurement conditions

- Reported CPU: AMD Ryzen 5 4600H with Radeon Graphics, x86-64.
- OS/kernel: Linux 6.18.40.1-microsoft-standard-WSL2, Microsoft hypervisor.
- One benchmark thread, pinned to allowed logical CPU 2 with `taskset`.
- Reported caches: 32 KiB L1 data per core, 512 KiB L2 per core, 4 MiB shared L3.
- GCC 13.3.0; Google Benchmark v1.9.4 built in Release mode.
- Kernel flags: `-O3 -g -march=native -fno-omit-frame-pointer`; no LTO.
- Scalar kernels additionally disable tree/SLP vectorization and if-conversion.
  Native kernels use normal GCC optimizations. See [CMakeLists.txt](../CMakeLists.txt).
- Seven repetitions, 0.2 s minimum CPU time and 0.05 s framework warm-up;
  randomized repetition order within each binary. Scalar and native processes
  run sequentially. Input seeds are fixed in [benchmark.cpp](../benchmark.cpp).
- Setup, allocations, correctness checks, and packing are outside the state loop.
  Every registered benchmark checks its result. No benchmark/library code changed
  during collection.
- ASLR is enabled. CPU frequency was not explicitly fixed. Virtualization,
  scheduling, cache sharing, and other host activity can affect these timings.

**Metric used below:** framework `real_time` median (wall ns/call), divided
by input size. Speedup is baseline median divided by candidate median. No timing
or repetition logic is implemented by this report: all timings and aggregate
statistics come from Google Benchmark.

**Profiler limitation:** `perf --version` exits with “perf not found for kernel
6.18.40.1-microsoft”. No cycles, instructions, branch misses, cache misses, or IPC
were collected. Measured timing changes and inspected assembly do not establish
hardware-counter hypotheses. The student handout gives commands
for collecting those counters on a supported machine.

## Reproduce

From `performance-cpp-2/seminar/`, after `make`:

```sh
mkdir -p results
# Choose a CPU allowed by taskset -pc $$; CPU 2 was allowed on this host.
taskset -c 2 ./build/bench-scalar \
  --benchmark_filter='^(dependency/(serial|four)/4096|branch/(branch|eager)/(random|blocked|rare|always)/1048576|memory/(one|four|packed)/(256|4096|65536|1048576)|simd/transform/(4096|16777216))$' \
  --benchmark_min_time=0.2s --benchmark_min_warmup_time=0.05 \
  --benchmark_repetitions=7 --benchmark_enable_random_interleaving=true \
  --benchmark_out=results/scalar.json --benchmark_out_format=json

taskset -c 2 ./build/bench-native \
  --benchmark_filter='^simd/transform/(4096|16777216)$' \
  --benchmark_min_time=0.2s --benchmark_min_warmup_time=0.05 \
  --benchmark_repetitions=7 --benchmark_enable_random_interleaving=true \
  --benchmark_out=results/native.json --benchmark_out_format=json
```

These commands replace the recorded reports; choose new output names to retain
the original data. Ratios are observations for these builds and inputs, not
expected scores for another CPU.

## All measured cases

The final component of each name is the input size. Min/max describe the seven
repetitions' wall ns/item; CV is the framework's wall-time coefficient of variation.
For small differences, inspect variation before deciding which version wins.

| Build / case | Min ns/item | Median ns/item | Max ns/item | CV |
|---|---:|---:|---:|---:|
| scalar: `branch/branch/always/1048576` | 1.195 | 1.213 | 1.418 | 6.3% |
| scalar: `branch/branch/blocked/1048576` | 0.859 | 0.890 | 0.926 | 2.9% |
| scalar: `branch/branch/random/1048576` | 3.710 | 3.736 | 3.898 | 1.9% |
| scalar: `branch/branch/rare/1048576` | 0.763 | 0.806 | 0.925 | 8.0% |
| scalar: `branch/eager/always/1048576` | 1.108 | 1.145 | 1.169 | 1.7% |
| scalar: `branch/eager/blocked/1048576` | 1.133 | 1.153 | 1.263 | 3.8% |
| scalar: `branch/eager/random/1048576` | 1.112 | 1.124 | 1.174 | 1.9% |
| scalar: `branch/eager/rare/1048576` | 1.114 | 1.123 | 1.188 | 2.5% |
| scalar: `dependency/four/4096` | 0.260 | 0.261 | 0.267 | 1.1% |
| scalar: `dependency/serial/4096` | 0.768 | 0.770 | 0.792 | 1.4% |
| scalar: `memory/four/1048576` | 28.146 | 28.754 | 29.126 | 1.3% |
| scalar: `memory/four/256` | 0.379 | 0.385 | 0.397 | 1.9% |
| scalar: `memory/four/4096` | 0.950 | 0.976 | 1.058 | 3.5% |
| scalar: `memory/four/65536` | 8.887 | 10.202 | 12.774 | 11.6% |
| scalar: `memory/one/1048576` | 112.337 | 117.629 | 120.047 | 2.2% |
| scalar: `memory/one/256` | 1.251 | 1.275 | 1.286 | 1.1% |
| scalar: `memory/one/4096` | 3.511 | 3.534 | 3.720 | 2.7% |
| scalar: `memory/one/65536` | 50.276 | 59.804 | 66.379 | 8.9% |
| scalar: `memory/packed/1048576` | 0.360 | 0.371 | 0.374 | 1.3% |
| scalar: `memory/packed/256` | 0.286 | 0.292 | 0.302 | 1.8% |
| scalar: `memory/packed/4096` | 0.261 | 0.267 | 0.274 | 1.9% |
| scalar: `memory/packed/65536` | 0.333 | 0.335 | 0.344 | 1.2% |
| scalar: `simd/transform/16777216` | 0.722 | 0.747 | 0.805 | 3.8% |
| scalar: `simd/transform/4096` | 0.515 | 0.533 | 0.546 | 2.3% |
| native: `simd/transform/16777216` | 0.559 | 0.570 | 0.583 | 1.2% |
| native: `simd/transform/4096` | 0.070 | 0.075 | 0.085 | 6.4% |

## Interpretation limits

- Four product accumulators expose independent recurrences, while also changing
  loop-control overhead. The measured ratio is not an isolated multiply-latency ratio.
- Branch/eager comparisons use matched data for each pattern. Changing patterns
  also changes which value cache lines are accessed. The always-active difference
  is small relative to the branching case's variation (6.3% CV).
- Four walkers traverse the same nodes in a different interleaving. Packed data
  changes layout and active footprint from 64N to 4N bytes; its conversion cost
  is excluded. Its speedup cannot be attributed to memory-level parallelism alone.
- Working-set sweeps do not measure pure cache latency or prove an exact cache
  boundary. The 4 MiB node point has about 9% / 12% CV for one / four walkers.
- SIMD uses eight 32-bit lanes in the inspected native loop. The smaller gain
  for large arrays is consistent with another limiting resource, but no direct
  DRAM-bandwidth measurement was made. Binary runs were sequential, so there
  can also be host-state drift between scalar and native measurements.

## Source identity

SHA-256 of the source/build configuration used for these runs:

| File (relative to seminar/) | SHA-256 |
|---|---|
| `benchmark.cpp` | `7e6e46633ab977ce4549af3db8fc2585c32b54a6fd79f8ec53f468f33076a0c3` |
| `cases.cpp` | `8258acdda84248101af570dffdad431bc656f307b34c4f0ead999ca6c9551aeb` |
| `cases.hpp` | `cc2b18ec7345b0ba94345b09d7a6a39718c729216264e12f868601f3c4096096` |
| `CMakeLists.txt` | `e861aa8c2f24ebc5039cb910878b9e8510657f40cfe24563fddc810dae52bb47` |
