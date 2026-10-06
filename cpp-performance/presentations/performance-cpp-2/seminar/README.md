# Seminar 2: find the CPU bottleneck

A 90-minute practical follow-up to [lecture 2](../slides.md), for C++ programmers
on Linux x86-64. Work in pairs: one person measures, the other challenges the
explanation. Switch roles between exercises.

The seminar follows the same investigation each time:

**Benchmark → locate the hot loop → inspect assembly and counters → name a
bottleneck → change one relevant property → measure again.**

| Time | Investigation | Lecture connection |
|---|---|---|
| 0–15 min | Build, check results, learn the profiling workflow | A practical investigation loop |
| 15–30 min | A small arithmetic loop is unexpectedly slow | Latency, throughput, true dependencies, renaming |
| 30–50 min | Identical activity rates have different performance | Prediction, speculative execution, branchless tradeoffs |
| 50–75 min | A traversal stalls despite doing little arithmetic | Finite OoO window, memory-level parallelism, caches |
| 75–85 min | An element-wise loop leaves execution capacity unused | SIMD versus instruction-level parallelism |
| 85–90 min | Present evidence and limits of each optimization | Bottlenecks can move |

Start with the baseline commands below. Before opening an optimized function,
write down your hypothesis and the evidence that would disprove it. Finished
implementations are supplied so the session can focus on investigation; an
optional coding round is to implement your own change before comparing them.
Facilitators can use [INSTRUCTOR.md](INSTRUCTOR.md) for the explanation and
follow-up questions.

## Build and establish the measurement conditions

Requirements: GCC with C++20, CMake (3.16+), Git, GNU Make, binutils
(`objdump`), Bash, Linux `perf`, and **Google Benchmark**. CMake uses an installed
Google Benchmark 1.9+ or fetches release v1.9.4 into the ignored `build/` directory.
The first build needs network access if the library is not already installed.
Google Test is not required. On Debian/Ubuntu the compiler tools are normally
supplied by `build-essential` and `binutils`; install `cmake`, `git`, and the
`perf` package appropriate to your distribution and kernel as needed.

From the repository root:

```sh
cd cpp-performance/presentations/performance-cpp-2/seminar
make -j2
make check
make asm
lscpu
g++ --version
perf --version
perf stat -e cycles:u,instructions:u -- true
```

Record CPU model, compiler version, optimization flags in `CMakeLists.txt`,
cache sizes, OS, and virtualization. Choose an allowed logical CPU with
`taskset -pc $$`; prefix timing and profiling commands with, for example,
`taskset -c 2` if CPU 2 is allowed. Avoid load on its SMT sibling. Repeat runs
in both orders (A/B, then B/A) to detect frequency, temperature, and background
load effects. Google Benchmark selects iteration counts for the requested run time.

Two optimized builds isolate mechanisms:

| Binary | Kernel flags in addition to `-O3 -g -march=native` | Purpose |
|---|---|---|
| `build/bench-scalar` | Vectorization and if-conversion disabled | Inspect scalar dependencies and actual branches |
| `build/bench-native` | Normal GCC optimizations | Check what happens when the compiler can transform the loop |

The scalar build is a controlled experiment. It is **not** an unoptimized debug
build. GCC's optimization passes can change the mechanism; always inspect the
generated loop. [GCC optimization options](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html)
describe the flags used here. `-march=native` targets the build machine; rebuild
when moving to another CPU.

Google Benchmark owns timing, iteration selection, repetitions, statistics, and
reports. The functions use `for (auto _ : state)`, `DoNotOptimize`, and
`ClobberMemory`; input generation and correctness checks sit outside that loop.
Kernels remain separate translation units without LTO. Full SIMD output is
checked outside timing. Matched variants produce equivalent results.

Set workshop defaults in your shell, then select named cases using the standard
Google Benchmark CLI:

```sh
export BENCHMARK_MIN_TIME=0.2s
export BENCHMARK_REPETITIONS=7
export BENCHMARK_MIN_WARMUP_TIME=0.05
./build/bench-scalar --benchmark_list_tests
./build/bench-scalar --help
```

Names have the form `dependency/serial/4096`, `branch/branch/random/1048576`,
`memory/one/1048576`, or `simd/transform/4096`. The final number is input size.
Small sizes 4, 5, 7, 31, and 4097 are registered for correctness/tail checks;
`make check` runs these through Google Benchmark with three iterations each.

The `Time` and `CPU` columns are nanoseconds per kernel call (wall and CPU time).
Use the repeated run's `_median` row for comparisons and divide by input size
for ns/item. `items_per_second` counts input elements; SIMD also reports useful
bytes/sec. `--benchmark_out=build/results.json` saves the framework's JSON report.
For example, compare variants in one process and randomize repetition order:

```sh
./build/bench-scalar --benchmark_filter='^dependency/(serial|four)/4096$' \
  --benchmark_enable_random_interleaving=true \
  --benchmark_out=build/dependency.json --benchmark_out_format=json
```

See the official [Google Benchmark user guide](https://google.github.io/benchmark/user_guide.html)
for framework options. Add further input sizes with `Arg()` in `benchmark.cpp`.

All exercises are single-threaded. Four accumulators or four traversals expose
parallelism **inside one CPU core**, rather than adding software threads.

## The profiling workflow

First find the hot function, then inspect why it waits:

```sh
# Count a long baseline. Each perf repetition starts a fresh benchmark process.
bash profile.sh stat -r 3 -e cycles:u,instructions:u -- \
  ./build/bench-scalar --benchmark_filter='^dependency/serial/4096$' \
  --benchmark_min_time=131072x --benchmark_repetitions=1 --benchmark_min_warmup_time=0

# Sample the selected kernel; symbols and frame pointers are in both builds.
bash profile.sh record -e cycles:u -F 999 -g -o build/dependency.data -- \
  ./build/bench-scalar --benchmark_filter='^dependency/serial/4096$' \
  --benchmark_min_time=131072x --benchmark_repetitions=1 --benchmark_min_warmup_time=0
perf report --stdio -i build/dependency.data
perf annotate --stdio -M intel -i build/dependency.data
```

`profile.sh` uses perf's control FIFOs around the Google Benchmark state loop,
excluding data setup, checks, and reporting. Profiling selects **one named case**
and a fixed iteration count with `--benchmark_min_time=131072x`. Setting
repetitions to one and framework warm-up to zero avoids calibration/warm-up
iterations contaminating counter normalization; the selected kernel is still
called once before the loop to warm its data.

Normalize counters by **size × iterations**: here `4096 × 131072` items per
process. `perf stat -r 3` runs three fresh processes and reports an average;
do not multiply the denominator by three. Counts include a little framework
and control overhead. Increase fixed iterations for very fast candidates.
Time-based Google Benchmark runs may invoke a case multiple times for
calibration/warm-up, so their displayed iterations are not a valid denominator
for external perf totals. See the primary
[perf stat](https://man7.org/linux/man-pages/man1/perf-stat.1.html) and
[perf record](https://man7.org/linux/man-pages/man1/perf-record.1.html) manuals.

`perf report` locates cost; `perf annotate` and `build/scalar.asm` reveal the
loop. Sample attribution can skid to nearby instructions, so a sampled branch
is not automatically the cause of waiting. Require a controlled comparison.
For each candidate optimization, repeat the same stat and record commands with
its variant substituted.

If hardware events are unavailable, try `cpu-clock` for sampling:

```sh
bash profile.sh record -e cpu-clock -F 999 -g -o build/software.data -- \
  ./build/bench-scalar --benchmark_filter='^dependency/serial/4096$' \
  --benchmark_min_time=131072x --benchmark_repetitions=1 --benchmark_min_warmup_time=0
```

This locates time but supplies no branch/cache-miss evidence. WSL and virtual
machines may lack PMU access or a matching `perf` executable; a Linux machine
with supported counters is preferable for the full workshop. If perf itself
is missing or access is denied, continue with timings and `objdump`, label the
explanation a hypothesis, and collect counter evidence on a suitable machine.
No permission or system settings are changed by the scripts.

## Exercise 1: why is arithmetic waiting? (15 min)

Scenario: a batch computes the product of 32-bit factors modulo 2^64. Its 16 KiB
input should fit in many L1 data caches. The CPU has multiple execution resources,
yet the baseline makes little progress per cycle.

```sh
./build/bench-scalar --benchmark_filter='^dependency/serial/4096$'
```

1. Profile `serial` using the workflow above. Is `product_serial` the hot function?
2. Find the loop in `build/scalar.asm`. Draw an arrow from each multiply result
   to the next use of that register. Which dependency survives register renaming?
3. Count cycles/item and instructions/item. Check branches/misses in a separate
   stat pass; is unpredictable control flow a plausible explanation?
4. Propose a way to keep multiple independent multiply chains in flight.
   Implement it, or inspect `product_four` after writing your hypothesis.

```sh
./build/bench-scalar --benchmark_filter='^dependency/four/4096$'
```

Repeat profiling with `four`. Did cycles/item fall despite roughly similar useful
arithmetic? Does the assembly show independent recurrence registers? Compare
with the normal build as a final check:

```sh
./build/bench-native --benchmark_filter='^dependency/serial/4096$'
./build/bench-native --benchmark_filter='^dependency/four/4096$'
```

**Deliverable:** an annotated dependency chain and evidence for or against a
recurrence-latency bottleneck. Explain why the result remains exact with unsigned
arithmetic, and why the same rewrite needs more care with floating point.

## Exercise 2: why does the same `if` sometimes lose? (20 min)

Scenario: mix each active record into a checksum. `random`, `blocked`, and
`alternating` each mark exactly floor(N/2) records active. `blocked` has one long
active region; `alternating` repeats 0,1; `random` shuffles the flags.

```sh
./build/bench-scalar --benchmark_filter='^branch/branch/blocked/1048576$'
./build/bench-scalar --benchmark_filter='^branch/branch/random/1048576$'
./build/bench-scalar --benchmark_filter='^branch/branch/alternating/1048576$'
bash profile.sh stat -r 3 -e cycles:u,instructions:u,branches:u,branch-misses:u -- \
  ./build/bench-scalar --benchmark_filter='^branch/branch/random/1048576$' \
  --benchmark_min_time=128x --benchmark_repetitions=1 --benchmark_min_warmup_time=0
```

1. Repeat the stat command for `blocked`. Locate and annotate `active_branch`
   with the record/report workflow. Identify the data-dependent conditional jump
   separately from the loop-back branch.
2. Explain why equal active rates need not have equal miss rates. The reported
   branch-miss percentage includes loop/harness branches; also compute misses/item.
3. Predict whether computing `mix` for every item, then masking the result, will
   help. Inspect `active_eager`, test it, and repeat counters for the same input:

```sh
./build/bench-scalar --benchmark_filter='^branch/eager/random/1048576$'
./build/bench-scalar --benchmark_filter='^branch/eager/blocked/1048576$'
./build/bench-scalar --benchmark_filter='^branch/branch/rare/1048576$'
./build/bench-scalar --benchmark_filter='^branch/eager/rare/1048576$'
```

4. Check `always` too. Does eliminating a branch compensate for extra computation
   when only floor(N/100) records are active? Finally compare `branch` and `eager`
   in `bench-native`; look for SIMD or masks before attributing the difference.

**Deliverable:** timings, misses/item, and instructions/item for matched variants
on random and rare inputs. Recommend an optimization for a specified activity
distribution. Checksums may differ between patterns because different records
are selected; they must agree between variants for the **same** pattern.

Matching active counts controls the number of mixes, but scattered selection
can touch more cache lines than one blocked region. Check cache behavior or
repeat with a smaller working set before assigning the entire difference to
prediction. Repeating a short random pattern can also let a predictor learn it.

## Exercise 3: why does a tiny loop stall on memory? (25 min)

Scenario: sum records through a random linked cycle. Each node occupies a 64-byte
slot with a `next` index and a value. Every node is visited exactly once per call.
This deliberately sparse layout makes latency visible; an index-based link has
the same dependent-address problem as a pointer-based link.

```sh
./build/bench-scalar --benchmark_filter='^memory/one/1048576$'
bash profile.sh stat -r 3 -e cycles:u,instructions:u -- \
  ./build/bench-scalar --benchmark_filter='^memory/one/1048576$' \
  --benchmark_min_time=16x --benchmark_repetitions=1 --benchmark_min_warmup_time=0
bash profile.sh stat -r 3 -e cache-references:u,cache-misses:u -- \
  ./build/bench-scalar --benchmark_filter='^memory/one/1048576$' \
  --benchmark_min_time=16x --benchmark_repetitions=1 --benchmark_min_warmup_time=0
```

1. Sample/annotate `chase_one`. Trace `load next → compute address → load next`.
   Can the CPU issue the next traversal load before this one completes?
2. Sweep sizes **256, 4096, 65536, 1048576** (16 KiB to 64 MiB of nodes).
   Use time-based runs for timing and fixed iterations for profiling. Compare
   the sizes with your CPU's cache capacities. Are stalls sensitive to working-set size?
3. Keep the same nodes and links, but start four walkers at quarter-cycle
   boundaries. Each walker consumes its partition; together they visit exactly
   the same N nodes. Predict what extra independent addresses allow.

```sh
./build/bench-scalar --benchmark_filter='^memory/four/1048576$'
./build/bench-scalar --benchmark_filter='^memory/packed/1048576$'
```

4. Repeat counts and sampling for `four`. Does it improve time while doing a
   comparable number of node accesses? Then inspect `packed_sum`, which stores
   just the values in traversal order and sums a contiguous array.

`one` versus `four` mainly tests memory-level parallelism with the same 64N-byte
node allocation. `packed` changes several properties together: predictable
addresses, a smaller 4N-byte working set, and more useful values per cache line.
Its footprint excludes links because this operation only needs values. It is a
representation change, with packing performed before timing; it is not proof
that OoO alone accounts for the whole speedup.

Generic cache events vary by CPU and may not describe a specific cache level.
Use `perf list` for supported L1/LLC/TLB events and consult the target CPU's
manual before naming an exact level. A miss count alone doesn't distinguish
latency from bandwidth. Large random traversal can also involve TLB misses.

**Deliverable:** a size-versus-time table, the load dependency chain, and a
comparison of `one`, `four`, and `packed`. Explain when you can pay the packing
cost once and amortize it over repeated traversals, and when you cannot.

## Exercise 4: what did the compiler leave unused? (10 min)

Scenario: transform independent values with `output[i] = input[i] * 5u + 3u`.
Start with a cache-resident pair of arrays (32 KiB total at N=4096; reduce N to
2048 if needed for your L1 size). Both builds run the same source and input.

```sh
./build/bench-scalar --benchmark_filter='^simd/transform/4096$'
./build/bench-native --benchmark_filter='^simd/transform/4096$'
cat build/vectorization.txt
```

1. Sample `transform` in both binaries. Compare `build/scalar.asm` and
   `build/native.asm`: scalar load/`lea`/store versus packed integer operations.
   Identify the vector width, lanes per iteration, alias guard, and scalar tail.
2. Repeat stat for each build with cycles/instructions. Does instructions/item
   fall? Distinguish SIMD lanes from OoO overlap of separate instructions.
3. Select size 16777216 (128 MiB across input/output).
   Does the ratio change once the arrays exceed cache capacity? The useful
   traffic is at least 8N bytes/call; write allocation/writeback may add traffic,
   so this is not a measurement of physical DRAM bandwidth.

**Deliverable:** the actual scalar and vector loop, a small/large-input timing
comparison, and an explanation of why wider vectors do not guarantee a matching
end-to-end speedup. Here the optimization is letting the compiler vectorize;
intrinsics are not required.

## Report the investigation

Copy this table for each lab; calculate speedup as baseline median / candidate
median. Preserve Google Benchmark JSON reports and perf output in `build/` if needed.

| Input/build | Variant | Median ns/item | Cycles/item | Instructions/item | Relevant misses/item | Result check |
|---|---|---|---|---|---|---|
| | Baseline | | | | | |
| | Candidate | | | | | |

Finish with four sentences: **the hot function was …; the suspected bottleneck
was …; the supporting intervention and evidence were …; this optimization stops
helping when …**. Record unavailable counters as unavailable, not zero. Low IPC,
fewer instructions, and a faster run alone are insufficient to identify a cause.

## Files and publishing boundary

`cases.cpp` contains the kernels; `benchmark.cpp` registers Google Benchmark
cases; `CMakeLists.txt` links the library; `check.sh` exercises equivalent results
and tails; `profile.sh` gates perf events.
Binaries, assembly, vectorization reports, and the profiling data paths above
use the already ignored `build/` directory. The profiler's temporary control
FIFOs use Linux temporary storage and are removed on exit, including on WSL
checkouts where the repository filesystem may not support FIFOs.

Run the workshop locally with Make. The runnable code and raw reports stay
outside `../public/`; the final section of `../slides.md` introduces Google
Benchmark and perf. Practical investigations and [recorded results](results/README.md)
remain in this seminar folder. The existing GitHub Pages workflow builds the
tool introduction with presentation 2. The benchmark code,
dependency downloads, and raw reports are not part of the static site's assets.
