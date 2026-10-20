# Teaching memory from C++ code

Use the core as three sessions. The appendix is optional depth, not a prerequisite
for following the core. The intended outcome is expert reasoning about memory;
students are not expected to arrive knowing OS memory terminology.

## Session boundaries

| Session | Slides | Suggested allocation |
|---|---|---|
| 1: OS storage | 1–27 | 10 min orientation; 20 min addresses/pages; 25 min allocation/faults; 15 min prediction/measurement; 15 min sharing/pressure; 5 min recap |
| 2: CPU access | 28–59 | 20 min lines/layout; 15 min sets/ways; 15 min transpose/reuse; 20 min request overlap; 25 min translation; 5 min recap |
| 3: C++ design | 60–91 | 20 min coherent writes; 15 min NUMA; 25 min representation/allocation; 15 min contracts; 20 min diagnosis; 5 min recap |

These are 90–100 minute teaching budgets including questions. A class with strong
prerequisites can move faster; allow an additional session for extensive hands-on
tool setup. Stop on each session summary. The full deck is not one sitting.

## The recurring example

The C++20 buffer contains 1,048,576 `uint64_t` elements: 8 MiB. All elements are
written before reads. `make_unique_for_overwrite` avoids mandatory zero-initialization
of these integers, but does not guarantee a fresh OS mapping. The first-write
lesson explicitly switches to a fresh private anonymous mapping. Reused allocator
storage and newly supplied physical backing must remain distinct.

Keep the same illustrative address correspondence on the board:

```text
virtual 0x10034 = page 16, offset 0x34
page 16 maps to frame 42, whose base is 0x2A000
physical address = 0x2A000 + 0x34 = 0x2A034
```

In session 2, draw that correspondence as a TLB entry while drawing data-cache
entries as bytes. In session 3, start the placement example with fresh backing;
rewriting existing pages does not guarantee moving them to the current worker.

Experiments are controlled variations, not measurements of precisely the same
allocation: first writes use 64 MiB, scans use the stated record/dense layouts,
and pointer chains use 64-byte nodes. Each result labels its size and units.

## Vocabulary navigation

Definitions appear on the slides themselves. This table helps instructors find
their introductions; it is not an acronym memorization assignment.

| Concepts | First explanation | Worked use / check |
|---|---|---|
| Byte, KiB/MiB/GiB, ns, footprint | 5 | Buffer size; page count |
| Process, address space, virtual/physical address | 7–8 | 10–11 |
| Page, frame, offset | 9 | 10–11 |
| Mapping, virtual memory area (VMA), page-table entry (PTE) | 12 | 13, 16 |
| Mapped, resident, backing; anonymous/private/shared | 13–14 | 15–22 |
| Page fault, minor/major fault, I/O; zero page | 16–18 | 20–22 |
| Repetition, median, warm-up, steady state | 19 | Measurements throughout |
| RSS/PSS/VSZ and their full names | 23 | Shared-memory arithmetic |
| Page cache, clean/dirty; copy-on-write | 24–25 | Reclaim in 26 |
| Working set, reclaim, swap, thrashing | 26 | Reuse distance in 44 |
| CPU cache/line, hit/miss, hierarchy and SRAM/DRAM | 29–31 | Read trace 30 |
| Eviction, locality, hot/cold; AoS/SoA | 32–33 | Layout experiment 34–35 |
| Set, way, index, tag; LRU; associativity | 36–39 | Trace 37, exercise 38 |
| Miss causes, stride, row-major, tile, reuse distance | 40–44 | Transpose 41–43 |
| Latency, bandwidth, throughput, stream, MLP | 45–47 | Timeline and prediction 48 |
| TLB, page walk, reach, huge page | 52–58 | Hit 53, walk 54, exercise 56 |
| Coherence, line ownership, write-back, RFO | 61–62 | Ownership trace 65 |
| Data race, atomic, relaxed; false sharing | 63–65 | Separate writers 66 |
| NUMA, node, local/remote, interconnect; first touch | 68–69 | 70 |
| Compact index; fusion and aliasing | 73–74 | Representation/loop changes |
| Size class, arena, thread cache; fragmentation | 75–76 | Partly occupied page diagram |
| Monotonic allocation, PMR | 77 | Code in 78 |
| Alignment | Brief explanation 34, use 66; numerical definition 80 | Contracts 81 |
| Prefetch; FLOP and arithmetic intensity | 82–83 | Guarded hint; bandwidth calculation |
| p99, profile, performance monitoring unit (PMU) | 84 | Diagnostic case 89 |

Ask learners what information is stored, who does the work, and what changes in
the example. Those explanations matter more than recalling an abbreviation.

## Exercises and feedback

Pause before reveals. Ask students to commit to an answer, then explain their
reasoning. Allow 45–90 seconds for arithmetic and 2–3 minutes for diagnosis.

| Slide | Task | What the answer should demonstrate |
|---|---|---|
| 11 | Translate neighboring addresses | Frame base changes; offset stays |
| 20 | Predict fresh/resident write passes | Count pages; identify allocation work |
| 22 | Explain read-only preparation | Zero-page reads need not supply private writable backing |
| 30 | Trace three array reads | A line fetch can satisfy a neighboring access |
| 33 | Predict a layout change | Count useful bytes, fetched lines, and pages separately |
| 38 | Simulate a cache | Set selection and replacement are different decisions |
| 48 | Predict independent streams | Request latency differs from throughput per result |
| 51 | Calculate useful/fetched bytes | State the traffic boundary |
| 56 | Identify the missing mechanism | Data-cache miss, page walk, and page fault differ |
| 70 | Predict physical placement | Initialization and allocation policy affect locality |
| 88 | Explain a slow traversal | Dependencies and multiple layers can interact |
| 89 | Diagnose p99 | Give competing hypotheses and a discriminating test |
| 90 | Correct design-review claims | Apply the models in unfamiliar wording |
| 92 | Choose one lab family | Preserve correctness, units, protocol, uncertainty |

Slide 89 gives an investigation plan rather than a unique diagnosis. For stronger
students, require one measurement that could favor each competing explanation.
The lab chooses one family; all experiments together exceed a one-hour session.

## Optional appendix selections

| Interest | Slides |
|---|---|
| Drepper reading strategy | 94 |
| Linux mappings, pressure, allocation, residency tools | 95–102 |
| Topology, DRAM, request limits, cache policy, store buffers | 103–108 |
| x86 tables, table footprint, tags, VIPT, shootdowns | 109–115 |
| Linux huge pages, nested translation, translation experiments | 116–118 |
| Streaming stores and ordering requirements | 119 |
| Release/acquire, ordering litmus, contended atomics | 120–123 |
| NUMA binding and scaling | 124, 126 |
| MESI protocol vocabulary | 125 |
| Measurement conditions, perf, correctness tools | 127–129 |
| Primary references | 130–132 |

Teach the ordinary model before its implementation exceptions: complete the toy
page walk before listing PML4/PDPT names, and demonstrate line ownership changes
before asking learners to interpret protocol details in processor manuals.
