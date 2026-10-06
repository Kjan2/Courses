# Facilitator notes: reveal after the investigation

Use [README.md](README.md) as the student handout. Aim for evidence-supported
explanations, not predetermined speedup numbers. Ask students to state what
would falsify their explanation before showing the next variant. The completed
kernels are reference solutions; students can write alternatives during the lab.

## 1. Recurrence latency → independent accumulators

`product_serial` has a true RAW dependency: each multiply needs the previous
product. Renaming cannot manufacture that value. The small working set and a
predictable loop branch help exclude competing explanations, but verify both.

`product_four` offers four independent product registers. Out-of-order scheduling
can overlap them and move toward multiplier throughput rather than one-chain
latency. The final combination and tail add overhead, especially on tiny inputs.
The supplied scalar flags prevent SIMD from obscuring this experiment; verify
that the compiler preserved the recurrence rather than assuming it did.

Evidence: a hot multiply loop; one recurrence becoming four; lower cycles/item
without a corresponding reduction in useful multiply work; low miss rates.
IPC may improve, but IPC is supporting evidence rather than the diagnosis.
Avoid promising a fourfold gain: execution-unit throughput, load throughput,
loop overhead, or front-end width may become the limit.

All input factors are odd to avoid products quickly becoming zero modulo 2^64.
Unsigned multiplication wraps with defined semantics and can be reassociated
exactly. Signed overflow and floating-point rounding would require different
correctness arguments. For actual multiply latency/throughput on the measured
CPU, consult the original measurements at [uops.info](https://uops.info/).

Discussion: would eight accumulators help? Test only after identifying the new
limit. More live registers and larger code can introduce costs.

## 2. Unpredictable control → eager evaluation, with a counterexample

The scalar `active_branch` should contain both a loop branch and a conditional
jump around `mix`. Random 50/50 activity generally offers less predictable
direction than a long blocked region. Alternating is balanced but structured;
do not demand a particular miss rate from every predictor. Repeated inputs and
predictor capacity also affect the result.

`active_eager` computes `mix` for every element and multiplies by a 0/1 activity
flag. This trades control for arithmetic and dependencies. With random activity,
less wrong-path work/recovery can outweigh doing additional mixes. With rare
activity, a predictable branch can skip nearly all mixes, so eager work may lose.
The transformation is valid because `mix` is pure and valid on inactive values.
It would be invalid for a guarded operation with side effects or invalid inputs.

Evidence: the conditional jump in assembly, increased misses/item for random
versus blocked activity, and a matched-input change that reduces misses and time.
Expect eager evaluation to execute more scalar arithmetic; fewer instructions
are not required for an improvement. Generic branch-miss percentage includes
the loop branch and is not the miss rate of the source `if` alone.

Repeat with the native build. Eager evaluation can expose SIMD while the
branching form retains control flow. If it does, say both prediction and
vectorization changed; do not attribute that whole gain solely to branch removal.

Discussion: increasing the cost of `mix` can reverse the decision. A good
optimization recommendation includes the activity distribution and mix cost.

## 3. Dependent addresses → parallel walks → compact representation

`chase_one` must load the next index before forming the address for the next
iteration. A cache miss on that chain has little independent traversal work to
hide behind. Low IPC and cache misses suggest waiting, but neither alone proves
the cause. Use the assembly dependency, working-set sweep, and intervention.

`chase_four` starts at positions 0, floor(N/4), 2×floor(N/4), and 3×floor(N/4)
of the same random cycle. Each consumes floor(N/4) nodes and the last also
consumes the remainder. This visits each node once and preserves the result.
Four independent chains provide potential outstanding loads on the same thread.
The node layout and address set stay the same, although access interleaving and
loop overhead change. A useful signature is fewer cycles/item despite comparable
memory work or miss counts. No claim that all four misses always overlap is
needed: queue sizes, bandwidth, and cache/TLB behavior may constrain that.

`packed_sum` is a larger design optimization: store only values in traversal
order. Predictable addresses permit prefetch and independent loads; removing
links/padding reduces the active footprint from 64N to 4N bytes. The scalar build
keeps vectorization out of this comparison. In real code, a compact array of
records retaining links is another option; this lab chooses the minimal data
needed for a sum. The 64-byte slot is a teaching choice, not a claim that every
CPU uses 64-byte cache lines.

The Google Benchmark cases build and validate representations before the state
loop, so packed
data already exists for the comparison. Report it as steady-state traversal
cost, not end-to-end conversion cost. It is most useful when many traversals can
share the packed representation. Mutation, required traversal order, object
identity, or conversion cost can constrain an application's choice.

Discussion: `prefetch(nodes[next])` right before following `next` offers little
lead time. Prefetch cannot know an arbitrary address before its predecessor
load supplies it. More independent queries or a layout change can expose work
sooner. Generic cache events do not establish whether the precise culprit is
L1, LLC, translation, or DRAM; identify supported events before being that specific.

## 4. Scalar instruction work → SIMD → bandwidth limit

There is no cross-element dependency in `transform`. GCC may generate vector
loads, packed shifts/adds, and vector stores. On an AVX2 target, 256-bit vectors
contain eight 32-bit lanes; other targets may choose other widths. Identify the
actual loop and do not count only the scalar alias-check fallback or tail.

Both binaries target the same host and differ in kernel optimization passes;
the transform has no data-dependent `if`, so vectorization is the intended main
change. The compiler may check pointer overlap before selecting the vector
path. No `restrict` promise or hand-written intrinsics are necessary here.

Evidence: SIMD in the native hot loop and lower instructions/item, accompanied
by lower time on a cache-resident input. For larger arrays, memory/store traffic
may become the limiting resource, shrinking the speedup. Cache capacity alone
does not prove DRAM bandwidth saturation; that requires appropriate bandwidth
measurements for the target machine.

Discussion: does SIMD remove the multiply dependency from exercise 1? No;
multiple vector accumulators may still be needed to overlap vector recurrences.

## If a comparison does not behave as predicted

Google Benchmark supplies run duration, repetitions, and median/statistical
reports. Increase `--benchmark_min_time` for timing runs when needed. For perf,
select exactly one case, use an explicit iteration count (`Nx`), one repetition,
and no framework warm-up; normalize by N times input size. Otherwise external
counters include calibration/warm-up work omitted from the reported iteration
count.

Check generated assembly, then data footprint, then run length and scheduling
noise. Verify that samples are long enough and comparisons use the same input.
Try both execution orders and retain the spread. A surprising result is an
opportunity to refine the hypothesis rather than adjust the data until it agrees.
When counters are unavailable, distinguish an assembly-based explanation from
a hardware-counter-confirmed bottleneck. Do not invent measurements.

The common endpoint is a new limit: after a dependency is shortened or misses
are avoided, load throughput, execution resources, front-end work, or bandwidth
may dominate. Ask each pair to explain why their optimization eventually stops
helping.
