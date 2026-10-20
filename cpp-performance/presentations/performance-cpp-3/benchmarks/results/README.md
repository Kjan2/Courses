# Recorded memory experiments

These are actual local measurements, not Drepper's historical results or
estimated timings. [raw.csv](raw.csv) retains **154 samples**: seven repetitions
for each of 22 case/footprint combinations. [summary.csv](summary.csv) contains
the median and min/max for each combination; [metadata.json](metadata.json)
records the environment, timestamp, and exact command.

## Conditions

- Reported processor: AMD Ryzen 5 4600H, x86-64; Microsoft hypervisor / WSL2.
- Kernel: Linux 6.18.54.1-microsoft-standard-WSL2.
- Guest topology: 32 KiB L1D and 512 KiB L2 per core; 4 MiB L3; one NUMA node.
- One benchmark thread pinned to guest logical CPU 2 with `taskset`.
- GCC 13.3.0; C++20; `-O3 -g -march=native -fno-omit-frame-pointer`;
  separate translation units, no LTO, normal vectorization enabled.
- Steady-state cases warm up and run for at least 40 ms per sample. First-write
  and resident-write cases are paired single passes over a fresh mapping per
  repetition. Both protocols have seven repetitions.
- Base page size 4 KiB. Global THP setting was `madvise`. First-write mappings
  explicitly disabled THP with checked `MADV_NOHUGEPAGE`; ordinary heap mapping
  backing was not verified.
- Host activity, CPU frequency, and physical placement are uncontrolled. There
  are no hardware-counter measurements in this recording.

## Selected medians

| Experiment | Baseline | Comparison | Ratio |
|---|---:|---:|---:|
| 64 MiB random nodes: one → eight streams | 131.412 ns/dereference | 19.570 ns/dereference | 6.71× throughput |
| One-field scan: 64-byte records → dense values | 4.374 ns/record | 0.503 ns/record | 8.70× faster |
| 2048-square transpose: naive → 32-square tiles | 14.849 ns/element | 7.125 ns/element | 2.08× faster |
| One store/page: first → resident pass | 1,704.182 ns/page | 46.149 ns/page | 36.93× faster |

For the first-write experiment every recorded first pass had 16,384 minor faults;
every resident pass had zero. First writes include allocation, zeroing, accounting,
mapping, and data-access costs. This is not a pure page-fault-handler latency.

The eight-stream number is amortized throughput: individual memory requests do
not become 6.71× lower latency. The AoS/SoA comparison changes footprint, useful
line fraction, and potentially SIMD. The transpose comparison changes reuse
distance and can affect both cache and translation behavior.

## Variation and interpretation

The largest outlier is `chase1` at 2 MiB: median 17.456 ns, range
13.615–80.262 ns. The chart retains the min–max band. Smaller working sets also
show substantial relative variation (for example 32 KiB: 1.185–2.474 ns).
These runs show large qualitative differences; they do not establish precise
cache hit latencies or small optimization wins. Repeat longer, randomize order,
and control the host before relying on close comparisons.

The pointer-chain allocation sweep changes cache and translation footprints
together. No curve knee is proof of an exact cache capacity. No physical DRAM
bandwidth, isolated TLB cost, huge-page improvement, false-sharing penalty, or
NUMA remote/local ratio is inferred from these timings alone.

## Reproduce

From `benchmarks/`:

```sh
make check
taskset -c 2 ./build/memory-bench > /tmp/memory-raw.csv
python3 summarize.py --input /tmp/memory-raw.csv --output /tmp/memory-summary.csv
# Rebuild the committed summary/chart from the committed recording:
MPLCONFIGDIR=/tmp/memory-matplotlib /tmp/memory-plot-venv/bin/python summarize.py --plot
```

Use an allowed CPU on your machine. The optional plotting command assumes the
temporary Matplotlib environment described in [the harness README](../README.md).
New measurements do not update the deck's numerical tables automatically; keep
their stated environment and values consistent if replacing this recording.
