# Performance C++ — lecture 3: memory

A 132-slide course unit continuing the first two lectures, in the same dark
course style: **91 core slides**, one optional lab slide, and a **40-slide
optional appendix**. The core is organized into three 75–100 minute sessions,
with explicit stopping points. Prerequisites are C++ pointers, arrays, object
lifetime, basic threads, and lecture 2's CPU execution model. Prior OS memory
management knowledge is not assumed.

The running example follows one buffer through allocation, first writes,
repeated scans, address translation, and use by multiple workers. Measurements
vary the size or layout as stated on each experiment. Definitions and worked
examples precede the checkpoints that use them. The
[teaching guide](TEACHING.md) gives session boundaries, exercises, vocabulary
introductions, and suggested appendix selections.

From the repository root:

```sh
npm run dev:3
npm run build:3
```

The static build goes to `dist/performance-cpp-3`. The existing GitHub Pages
workflow includes this deck under `performance-cpp-3/` on the next deployment.
Lectures 1 and 2 keep their existing commands and paths.

## Content

- **Session 1, slides 1–27:** addresses, pages and frames, mapping and residency,
  first writes and faults, measured fault costs, accounting, sharing, pressure.
- **Session 2, slides 28–59:** cache lines, a small cache trace, layout and
  transpose, request-overlap timelines, TLB hit/walk traces, huge-page motivation.
- **Session 3, slides 60–91:** cache-line write permission, false sharing, NUMA,
  representation, allocation lifetime, C++ contracts, and guided diagnosis.
- **Optional lab, slide 92:** select one experimental family for 45–60 minutes.
- **Optional appendix, slides 93–132:** implementation details, Linux tools,
  advanced synchronization, measurement conditions, and primary references.

The complete unit covers:

- OS view: virtual addresses, pages, VMAs and page tables, allocation and demand
  paging, faults, anonymous/file/private/shared backing, page cache, COW, RSS/PSS,
  reclaim/swap, overcommit, cgroups, allocator retention, and prefaulting.
- Hardware view: cache topology, SRAM/DRAM, channels and banks, latency versus
  bandwidth, memory-level parallelism, lines/sets/ways, misses, cache policies,
  ownership/RFO, store buffering, and working sets.
- Translation: x86-64 page tables and their footprint, hardware walks, TLB
  hierarchies and reach, VIPT, shootdowns, huge pages and modern THP, nested paging.
- Measurements: dependent pointer chains, eight independent streams, dense hot
  fields versus 64-byte records, tiled transpose, and first-write page faults.
- C++ engineering: hot/cold splitting, AoS/SoA/AoSoA, compact data structures,
  blocking and fusion, PMR/lifetime, container growth, alignment and aliasing,
  prefetching, streaming stores, Roofline, and choosing an intervention.
- Multiple cores: coherence, false sharing, atomics and ordering, release/acquire
  publication, ownership partitioning, NUMA placement, first touch, and scaling.
- Investigation: OS and PMU evidence, correctness versus timing tools, benchmark
  discipline, checkpoints, a design-review case study, and practical exercises.

The deck uses original diagrams, code, and local measurements. It draws the
mechanisms and experiment ideas from Ulrich Drepper's *What Every Programmer
Should Know About Memory*, with primary Linux, processor, compiler, and C++
references for current details. Historical northbridge diagrams and old timing
constants are not treated as current platform specifications.

## Evidence and practical work

[benchmarks/README.md](benchmarks/README.md) documents the standalone Linux C++20
harness, correctness checks, profiling commands, units, limitations, and follow-up
experiments. [Recorded results](benchmarks/results/README.md) include every raw
repetition and the environment metadata. The chart's source is the raw recording;
`summarize.py --plot` regenerates it with Matplotlib. No extra dependencies are
needed to build or view the deck.

Timing tables identify this WSL2 guest and the measured protocols. The deck
distinguishes those results from arithmetic teaching models and unmeasured
follow-up experiments. There is no claimed isolated TLB, huge-page, false-sharing,
or remote-NUMA result. Speaker notes explain confounders and suggest the next
discriminating measurement. Fourteen exercise slides include prediction,
calculation, tracing, diagnosis, and the optional lab; thirteen have
click-to-reveal explanations. Required vocabulary is introduced on visible
slides rather than only in presenter notes.
