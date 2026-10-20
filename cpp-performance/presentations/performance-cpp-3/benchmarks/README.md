# Memory experiments

Self-contained Linux C++20 experiments for lecture 3. No benchmark library is
required. The driver uses `steady_clock`; kernels are in a separate translation
unit and the default build does not enable LTO. Returned values feed an observable
checksum; transpose writes are checked. The one-store-per-page experiment
deliberately uses volatile stores. Other kernels use ordinary optimized accesses.

From this directory:

```sh
make check
# Select an allowed logical CPU from /proc/self/status; recorded run used 2.
taskset -c 2 ./build/memory-bench > /tmp/memory-raw.csv
python3 summarize.py --input /tmp/memory-raw.csv --output /tmp/memory-summary.csv
```

The committed [recording](results/README.md) retains seven samples per case/size.
To regenerate the committed summary from that recording:

```sh
python3 summarize.py
```

The optional `--plot` flag requires Matplotlib and regenerates the standalone
`../public/figures/pointer-chase.svg`. Slidev needs neither Python nor Matplotlib
to display or build the deck. To install the plotting dependency separately:

```sh
python3 -m venv /tmp/memory-plot-venv
/tmp/memory-plot-venv/bin/pip install matplotlib
MPLCONFIGDIR=/tmp/memory-matplotlib /tmp/memory-plot-venv/bin/python summarize.py --plot
```

## Protocol and units

- `chase1`: one random cycle of 64-byte nodes; one dependent load per step.
- `chase8`: eight positions spaced around the **same** random cycle. Each call
  performs the same total number of dereferences as chase1. At large sizes the
  eight streams partition a traversal. ns/op is amortized time per dereference,
  not latency of an individual outstanding request.
- `aos`, `soa`: sum the same 1,048,576 unsigned 64-bit values. AoS occupies
  64 MiB in 64-byte records; dense values occupy 8 MiB. Ordinary optimization,
  including SIMD, is enabled for both.
- `transpose_*`: transpose a 2048-square `uint32_t` array, from a 16 MiB source
  to a separate 16 MiB destination. Tiled version uses 32-square tiles. ns/op
  is per matrix element; logical payload is one 4-byte read plus one 4-byte write.
- `first_write`, `resident_write`: write one byte per base page in a 64 MiB
  private anonymous mapping. `MADV_NOHUGEPAGE` is checked. Each repetition gets
  a fresh mapping; its first and second passes are timed **once each**. Mapping,
  advice, and unmapping are outside timing. ns/op is per page, not per byte of
  the mapping. Fault deltas come from `getrusage(RUSAGE_SELF)` around the pass.

Steady-state cases warm up once and repeat calls for at least 40 ms per sample.
The first-write pair instead uses the single-pass protocol above. Setup, shuffling,
and input initialization are outside timing. The timing loop includes function
calls, checksum updates, and a clock read per call; interpret tiny differences
accordingly. The fixed case order and short minimum duration make this a teaching
harness; use longer, randomized interleaving for close comparisons.

Raw CSV records elapsed seconds, number of iterations, normalized ns/op, useful
bytes per iteration, and minor faults over each sample. Useful bytes are logical
payload, **not measured cache or DRAM traffic**. For chains the useful field is
the pointer; for page touches it is one written byte per page. Faults in a
steady-state row cover all its iterations; first-write rows have one iteration.

## Correctness and inspection

`make check` verifies chain endpoints, eight-stream endpoints, matching AoS/SoA
sums, transpose coordinates and tails (1, 31, 32, 33, 65), and exact page-store
positions. Every normal run executes these checks before timing and validates
the final large transpose. The driver checks mapping/advice failures. These
checks validate semantics, not the hypothesized performance mechanism.

```sh
g++ -O3 -std=c++20 -march=native -S -masm=intel kernels.cpp -o /tmp/memory-kernels.s
./build/memory-bench --filter transpose --repetitions 7 --min-ms 300
./build/memory-bench --filter resident_write
```

The filter is a substring of the case name. The resident-only case still performs
the first pass to prepare the mapping, while omitting that pass's output row.
Data are deterministic for the same standard library (`mt19937_64` seed
20261018; `std::shuffle` permutations are library-dependent).

## Profiling and follow-up investigations

```sh
perf list
perf stat -r 5 -e cycles,instructions,page-faults,minor-faults,major-faults \
  -- taskset -c 2 ./build/memory-bench --filter transpose --min-ms 300
```

These counts include checks, setup, warm-up, and timed work. Do not divide them
by just the CSV's timed iterations. To attribute counts to only a kernel, add
explicit event gating or use a dedicated steady-state profiling driver. Event
support and permissions vary; the committed recording has no PMU evidence.

Useful extensions, preserving correctness and documenting each change:

1. Sweep record size and hot-field count; inspect SIMD in the generated assembly.
2. Sweep transpose dimensions (including 2047, 2048, 2049) and tile sizes. Compare
   tails, set conflicts, translation pressure, and working-set hypotheses.
3. Build a separate random one-node-per-page chain. Rotate offsets within pages
   so a fixed 4 KiB stride does not accidentally test only one L1 set.
4. Compare verified base/huge mappings, checking `AnonHugePages`, flags, and
   relevant kernel counters. `MADV_HUGEPAGE` alone is not proof of backing.
5. On a multi-node machine, compare local versus remote pages and parallel
   initialization. Inspect actual placement through `numa_maps` / `numastat`.
6. For false sharing, pin two workers to separate physical cores and compare
   adjacent versus separated counters; keep atomic operations and total work
   equivalent. Include an uncontended baseline and inspect coherence evidence.

No isolated TLB, huge-page, NUMA, or false-sharing speedup is asserted by these
measurements. See the deck's references for the primary OS and hardware sources.
