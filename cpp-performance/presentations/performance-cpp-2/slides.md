---
theme: default
title: "Performance C++ 2 — From assembly to the CPU"
layout: cover
highlighter: shiki
fonts:
  sans: Arial
  mono: DejaVu Sans Mono
  provider: none
transition: fade
mdc: true
---

# Performance C++
# 2

From assembly to the CPU

<div class="subtitle">Pipelines · branch prediction · out-of-order execution</div>

<style>
:global(.slidev-layout) { padding: 38px 44px; background: radial-gradient(ellipse at top right, #153b59, transparent 65%), #07111c; color: #e7edf5; font: 22px/1.4 Arial, sans-serif; }
:global(.slidev-layout h1) { color: #71c6ff; font-size: 34px; line-height: 1.15; margin-bottom: 22px; }
:global(.slidev-layout h2) { font-size: 25px; color: #71c6ff; }
:global(.slidev-layout strong) { color: #71c6ff; }
:global(.slidev-layout p) { margin: 12px 0; }
:global(.slidev-layout li) { margin: 9px 0; }
:global(.slidev-layout pre), :global(.slidev-layout pre code) { font-size: 16px !important; line-height: 1.5 !important; }
:global(.slidev-layout .slidev-code) { background: #101b2a !important; border: 1px solid #2b4056; padding: 16px !important; }
:global(.slidev-layout .shiki span) { color: var(--shiki-dark, #d7e5f3) !important; }
:global(.slidev-layout :not(pre) > code) { background: #1b3045; color: #b8dfff; }
:global(.slidev-layout table) { width: 100%; font-size: 19px; background: #101b2a; }
:global(.slidev-layout th), :global(.slidev-layout td) { padding: 8px 12px; border-color: #2b4056; }
:global(.slidev-layout th) { color: #71c6ff; }
:global(.slidev-layout .note), :global(.slidev-layout .subtitle) { color: #a8bed0; font-size: 17px; }
:global(.slidev-layout .callout) { margin-top: 20px; padding: 14px 18px; background: #132c3b; border-left: 4px solid #71c6ff; }
:global(.slidev-layout .grid2) { display: grid; grid-template-columns: 1fr 1fr; gap: 24px; }
:global(.slidev-layout .flow) { display: flex; align-items: center; gap: 10px; margin: 28px 0; font-size: 19px; }
:global(.slidev-layout .flow b) { padding: 16px 12px; background: #153b59; border: 1px solid #39759c; border-radius: 8px; text-align: center; flex: 1; }
:global(.slidev-layout.exercise) { border-top: 6px solid #52c7ad; }
:global(.slidev-layout.cover h1) { font-size: 58px; }
:global(.slidev-layout a) { color: #93d4ff; }
</style>

<!--
75–90 minutes including questions. Audience: C++ programmers; no assembly prerequisite.
Use the diagrams as models, not literal layouts of a particular processor.
-->

---

# Why did lecture 1 have surprises?

| Observation | Mechanism to investigate today |
|---|---|
| A lookup can lose to arithmetic | Memory latency and independent work |
| Removing an `if` can help or hurt | Prediction versus extra computation |
| Four accumulators can disappoint | Dependencies, vectorization, bottlenecks |
| Manual unrolling can regress | Front-end pressure and register pressure |

<div class="callout">Source code describes a result. Performance depends on how the CPU produces it.</div>

<!-- Recall two examples from lecture 1. These are hypotheses, not retrospective proof of a measured bottleneck. -->

---

# The route through this lecture

1. **Read assembly:** registers, memory, arithmetic, branches, calls.
2. **Overlap work:** pipelines, latency, throughput, superscalar execution.
3. **Find independent work:** dependencies, renaming, out-of-order scheduling.
4. **Keep work coming:** branch prediction and speculative execution.
5. **Apply the model:** memory, SIMD, bottlenecks, measurement.

<div class="note">Examples: baseline x86-64, Intel syntax, Linux System V calling convention. CPU diagrams and timing exercises are simplified models.</div>

<!-- About 20 minutes assembly, 15 pipelines, 20 OoO, 15 prediction, 15 application. -->

---

# From C++ to instructions

<div class="flow"><b>C++ source</b> → <b>Compiler</b> → <b>Assembly</b> → <b>Assembler</b> → <b>Machine code</b></div>

- Assembly is a readable notation for machine instructions.
- The linker combines object files and resolves symbols.
- The CPU fetches encoded bytes; it does not execute C++ statements.
- Optimizations can remove, combine, reorder, or vectorize operations.

<div class="callout">One C++ line is neither one instruction nor one cycle.</div>

<!-- Assembly output is optional in a real compiler pipeline; an integrated assembler can produce objects directly. -->

---

# ISA versus microarchitecture

| ISA: the software contract | Microarchitecture: the implementation |
|---|---|
| Instructions and their results | Decode and execution units |
| Architectural registers | Physical registers and renaming |
| Addressing and exceptions | Caches, queues, predictors |
| Memory-ordering rules | Scheduling and speculation |

The same x86-64 binary can run on different CPU designs with different performance.

<div class="note">The concepts also apply to many Arm and RISC-V cores; particular instructions and implementation details differ.</div>

<!-- Some cores are in order. Do not imply that an ISA mandates an out-of-order design. -->

---

# Registers: small, named pieces of state

| Name | Role in these examples |
|---|---|
| `rax`, `rbx`, `rcx`, `rdx`, `r8` … | General-purpose integer registers |
| `rip` | Instruction pointer |
| `rsp` | Stack pointer |
| `rflags` | Condition flags, including zero, carry, overflow |
| `xmm0`, `ymm0` … | Floating-point and SIMD registers |

`rax` = 64 bits · `eax` = its low 32 bits · `ax` = low 16 · `al` = low 8

<div class="callout">Writing `eax` clears the upper 32 bits of `rax`. Writing `ax` or `al` does not.</div>

<!-- Architectural names are what assembly exposes. Later we introduce a larger, hidden pool of physical registers. -->

---
class: book-figure
---

# The x86-64 integer register map

<div class="figure-row">
  <img src="/figures/csapp-integer-registers.png" alt="CSAPP Figure 3.2: sixteen x86-64 integer registers, their overlapping 64-, 32-, 16-, and 8-bit names, and System V calling-convention roles." />
  <div class="figure-guide">
    <p>Each row is <strong>one register</strong>.</p>
    <p>The nested boxes name overlapping portions of the same bits.</p>
    <p><code>rax → eax → ax → al</code></p>
    <p>The book uses AT&amp;T notation: <code>%rax</code> is the register we write as <code>rax</code>.</p>
    <p>The right column shows calling-convention roles.</p>
  </div>
</div>
<div class="figure-source">Bryant &amp; O’Hallaron, Computer Systems: A Programmer’s Perspective, 3rd ed., Figure 3.2 · supplied PDF page 208.</div>

<!-- Direct crop from CSAPP_2016.pdf, PDF page 208 (zero-based page index 207). Point at the bit positions and nested names; these are aliases, not independent storage. The roles on the right describe the System V convention used in this lecture. -->

---

# Read Intel syntax: destination first

```asm
mov eax, 7          ; eax = 7
mov ecx, eax        ; ecx = eax
add eax, 3          ; eax = eax + 3
imul eax, ecx       ; eax = low 32 bits of eax * ecx
xor edx, edx        ; edx = 0
```

- `mov` copies; it does not erase the source.
- An **immediate** is a constant encoded in the instruction.
- Integer register arithmetic has a fixed width.

<div class="note">AT&amp;T syntax uses different operand order and notation. Do not mix the two when reading disassembly.</div>

<!-- Trace the values: eax 7 → 10 → 70, ecx 7, edx 0. C++ signed overflow rules are a separate language-level issue. -->

---

# Memory: brackets mean an access

```asm
mov eax, DWORD PTR [rdi]          ; load 4 bytes
mov DWORD PTR [rdi + 4], eax      ; store 4 bytes
mov edx, DWORD PTR [rdi + rcx*4]  ; load a[rcx]
lea rax, [rdi + rcx*4 + 8]        ; compute an address
```

**Address = base + index × scale + displacement**

- The scale can be 1, 2, 4, or 8.
- `DWORD PTR` specifies a 32-bit memory operand.
- `lea` computes the expression without reading memory.

<div class="callout">`lea` can do integer arithmetic. Brackets alone do not make it a load.</div>

<!-- Addresses are virtual addresses in an ordinary user process. Translation and caches come later. -->

---

# A tiny C++ function, decoded

```cpp
unsigned affine(unsigned x, unsigned y) {
  return x * 5u + y;
}
```

One possible implementation, using the System V x86-64 ABI:

```asm
affine:
    lea eax, [rdi + rdi*4]  ; low 32 bits of 5*x
    add eax, esi            ; add y
    ret                     ; result is in eax
```

<div class="note">Illustrative assembly throughout; exact compiler output depends on compiler, options, target, and context.</div>

<!-- Unsigned arithmetic makes the modulo-2^32 result intentional. Explain that LEA does not update condition flags. -->

---

# Comparisons create flags; branches read them

```asm
cmp edi, esi       ; set flags as if computing edi - esi
jb  .less          ; branch if unsigned edi < esi
; fall-through path
```

| After `cmp a, b` | Meaning |
|---|---|
| `je` / `jne` | Equal / not equal |
| `jb` / `jae` | Unsigned below / above or equal |
| `jl` / `jge` | Signed less / greater or equal |

<div class="callout">Signed and unsigned comparisons interpret the same bits differently.</div>

<!-- Example: 0xffffffff is unsigned 4294967295 or signed -1. CMP does not store the subtraction result. Flags are also dependencies. -->

---

# A complete scalar loop

```cpp
std::uint64_t sum(const std::uint32_t* a, std::size_t n);
```

```asm
sum:
    xor eax, eax                 ; sum = 0
    xor ecx, ecx                 ; i = 0
    test rsi, rsi
    je .done                     ; n == 0
.loop:
    mov edx, DWORD PTR [rdi + rcx*4]
    add rax, rdx                 ; zero-extended element
    inc rcx
    cmp rcx, rsi
    jb .loop
.done:
    ret
```

<!-- Teaching implementation, not claimed optimized compiler output. RDI is a, RSI is n. Ask which instructions access data memory and which form loop-carried dependencies. RET also accesses the stack. -->

---

# Calls, stack, and the ABI

- System V integer/pointer arguments start in `rdi, rsi, rdx, rcx, r8, r9`.
- Integer results use `rax` (or `eax` for a 32-bit result).
- `call` puts a return address on the stack; `ret` retrieves it.
- The ABI specifies register preservation and stack alignment.
- Locals may live in registers, on the stack, or disappear entirely.

<div class="callout">Inlining can remove call overhead and expose more optimization opportunities.</div>

<div class="note">Windows x64 uses a different calling convention. The stack is memory, usually served through caches.</div>

<!-- Caller-saved versus callee-saved is a convention, not an automatic save of every register. Inlining can also enlarge code. -->

---
class: exercise
---

# Checkpoint: read the result

```asm
mov eax, 10
mov ecx, 3
lea edx, [rax + rcx*4]
cmp edx, 22
jne .different
```

1. What is in `edx`?
2. Is the branch taken?
3. Which instruction reads data memory?

<v-click>
<div class="callout">22 · No · None of these instructions</div>
</v-click>

<!-- Let learners answer before revealing. Instruction fetching still accesses the instruction hierarchy. -->

---
layout: section
---

# From instruction order<br>to overlapping work

Pipelining → superscalar execution → out-of-order execution

---

# A simple CPU datapath

<div class="flow"><b>Fetch</b> → <b>Decode</b> → <b>Execute</b> → <b>Memory</b> → <b>Write back</b></div>

- **Fetch:** obtain instruction bytes at the instruction pointer.
- **Decode:** determine the operation and operands.
- **Execute:** compute a result, address, or branch outcome.
- **Memory:** access data for a load or store.
- **Write back:** make the result available in this simple model.

<div class="note">A five-stage teaching CPU. Modern x86 cores have a more complex pipeline; writeback and retirement are distinct.</div>

<!-- No claim that all x86 instructions pass through exactly five one-cycle stages. -->

---

# Pipelining overlaps different instructions

| Instruction / cycle | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|
| A | F | D | E | M | W | | |
| B | | F | D | E | M | W | |
| C | | | F | D | E | M | W |

**Assume:** five one-cycle stages, independent instructions, no stalls.

- First result: after 5 cycles.
- Then: one result per cycle.
- N instructions: **N + 4 cycles**, versus 5N without overlap.

<div class="callout">Pipelining improves throughput; it does not make an individual operation instantaneous.</div>

<!-- Clock period, pipeline overhead and hazards are intentionally omitted from this model. -->

---

# Latency and throughput answer different questions

**Latency:** how long until a dependent operation can use the result?

**Reciprocal throughput:** average cycles per operation for independent work.

```asm
imul rax, rax, 3   ; next multiply needs this result
imul rax, rax, 3
imul rax, rax, 3
```

Toy multiplier: **latency 3 cycles**, **one new multiply per cycle**.

- Dependent chain: start at cycles 0, 3, 6.
- Independent multiplies: start at cycles 0, 1, 2.

<div class="note">These numbers define the exercise machine, not a universal x86 timing.</div>

<!-- The three results become available at 3,6,9 versus 3,4,5. Real timings depend on instruction form and CPU. -->

---

# Hazards: reasons a pipeline must wait

| Hazard | Example | Possible response |
|---|---|---|
| Data | `add` needs a previous load | Forward results; wait if not ready |
| Structural | Two operations need one unit | Schedule them in different cycles |
| Control | Next address depends on a branch | Predict and verify later |

**Forwarding / bypassing** sends a result directly to a consumer.

A cache miss or an unresolved dependency can still require waiting.

<!-- Forwarding avoids waiting for a register-file write/read, but cannot produce a value before it exists. -->

---

# Superscalar: more than one operation per cycle

<div class="flow"><b>Decode / dispatch</b> → <b>ALU</b><b>Multiply</b><b>Load / store</b></div>

- Multiple execution resources can work simultaneously.
- Different instructions compete for different resources.
- Front-end, scheduling, execution, and retirement have finite widths.
- Pipelining overlaps stages; superscalar execution adds width.

<div class="callout">A “wide” CPU needs enough independent work to use that width.</div>

<!-- This diagram represents choices of resources, not a serial chain after dispatch. Port mappings and widths are model-specific. -->

---

# Modern x86: instructions become micro-operations

<div class="flow"><b>Instruction bytes</b> → <b>Decode / µop cache</b> → <b>µops</b> → <b>Execution units</b></div>

- A machine instruction may translate to one or several **µops**.
- Some instruction pairs can be fused on supported cores.
- Some complex operations use microcode sequences.
- Many cores cache decoded work to reduce repeated decoding.

<div class="callout">Instruction count alone does not describe execution cost.</div>

<!-- A memory-source arithmetic instruction can involve a load and arithmetic. Fusion and counting rules vary by core and pipeline stage. -->

---

# Out-of-order execution: run what is ready

```asm
mov  rax, QWORD PTR [rdi]  ; A: load, possibly slow
add  rax, 1                ; B: waits for A
imul r8, r9, 3             ; C: independent of A and B
add  r10, r8               ; D: waits for C
```

<div class="flow"><b>A → B<br>load chain</b><b>C → D<br>arithmetic chain</b></div>

The scheduler may execute C and D while A is still waiting.

<div class="callout">Execution follows operand readiness, within a finite window of work.</div>

<!-- This is still one software thread. OoO does not parallelize through true data dependencies. -->

---

# True dependencies versus reused names

```asm
mov rax, QWORD PTR [rdi]   ; creates value A
add rbx, rax               ; must read A
mov rax, QWORD PTR [rsi]   ; creates unrelated value B
add rcx, rax               ; must read B
```

- **RAW — read after write:** consumer needs a producer's value.
- **WAR — write after read:** a later write must not destroy an earlier input.
- **WAW — write after write:** final state must reflect the later write.

<div class="callout">RAW is a real data dependency. WAR and WAW can be name conflicts.</div>

<!-- Full-register example. Partial-register behavior has extra details that are outside this introduction. -->

---

# Register renaming removes name conflicts

| Program register operation | Conceptual physical-register operation |
|---|---|
| `rax = load [rdi]` | `P17 = load [rdi]` |
| `rbx = rbx + rax` | `P18 = P_old_b + P17` |
| `rax = load [rsi]` | `P19 = load [rsi]` |
| `rcx = rcx + rax` | `P20 = P_old_c + P19` |

Each reader is linked to the correct version of its input.

The second load can start before the first addition completes.

<div class="callout">Renaming removes false dependencies; the P17 → P18 dependency remains.</div>

<!-- The mapping is assigned in program order. Physical registers are a hidden implementation resource, not extra registers available in source assembly. -->

---

# Execute out of order; retire in order

<div class="flow"><b>Rename / allocate</b> → <b>Schedule ready µops</b> → <b>Execute</b> → <b>Retire in order</b></div>

- A **reorder buffer (ROB)** tracks in-flight work in program order.
- Completed results may feed younger instructions immediately.
- Retirement commits the oldest completed instructions safely.
- A fault or wrong-path execution must not commit younger results.

<div class="callout">Retirement preserves precise architectural state while execution overlaps.</div>

<!-- Stores typically enter a store buffer and become globally visible later under memory-ordering rules. Retirement is not “all stores have reached RAM.” -->

---
class: book-figure
---

# A real CPU: the Pentium 4

<div class="figure-row">
  <img src="/figures/inside-machine-pentium4.png" alt="Inside the Machine Figure 7-5: Pentium 4 front end, trace cache, micro-op queues, reorder buffer, schedulers, execution units, and completion unit." />
  <div class="figure-guide">
    <p>Follow the instruction flow from <strong>top to bottom</strong>.</p>
    <p>The front end fetches and decodes instructions.</p>
    <p>Queues and schedulers feed several execution resources.</p>
    <p>The completion unit preserves program order.</p>
    <p class="note">A historical implementation: the trace cache and exact layout are specific to this design.</p>
  </div>
</div>
<div class="figure-source">Jon Stokes, Inside the Machine, p. 148, Figure 7-5: Basic architecture of the Pentium 4 · supplied PDF page 170.</div>

<!-- Direct crop from the supplied Inside the Machine PDF. Revisit fetch, decode, µops, scheduling, and retirement using the original diagram. Do not interpret this as the layout of every modern x86 CPU. -->

---
class: book-figure
---

# Inside the back end

<div class="figure-row">
  <img src="/figures/inside-machine-pentium4-backend.png" alt="Enlarged back end from the Pentium 4 diagram: reorder buffer, integer and memory queues, schedulers, execution ports, integer and floating-point units, load-store unit, and completion." />
  <div class="figure-guide">
    <p><strong>Queues:</strong> hold work in flight.</p>
    <p><strong>Schedulers:</strong> select work whose operands and resources are ready.</p>
    <p><strong>Execution units:</strong> perform arithmetic and memory operations.</p>
    <p><strong>Completion:</strong> retire valid results in order.</p>
  </div>
</div>
<div class="figure-source">Jon Stokes, Inside the Machine, p. 148, Figure 7-5 · enlarged crop of the back end and completion unit.</div>

<!-- Same original figure, enlarged for projection. Ask learners which parts could fill up while an old load waits. Use that question to introduce the finite-window slide next. -->

---

# The window cannot grow forever

Suppose an old load misses in the cache:

1. Younger independent work can execute.
2. Retirement may eventually wait behind the old load.
3. The ROB, scheduler, physical registers, or load/store queues fill.
4. The front end stops admitting more work until resources free up.

<div class="callout">Out-of-order execution hides some latency. It cannot hide arbitrary latency with finite independent work.</div>

<!-- Relate to lookup versus calculate from lecture 1. A long chain of dependent loads offers very little useful overlap. -->

---
class: exercise
---

# Checkpoint: how many independent chains?

Toy machine: multiply latency 3 cycles, one multiply can start per cycle.

```asm
imul rax, rax, 3
imul rbx, rbx, 3
imul rcx, rcx, 3
; repeat this sequence many times
```

What limits one chain? Can three chains keep the multiplier busy?

<v-click>
<div class="callout">One chain starts once every 3 cycles. Three chains can alternate every cycle, assuming no other bottleneck.</div>
</v-click>

<!-- A at 0,3,6; B at 1,4,7; C at 2,5,8. Ideal independent-chain count is approximately latency / reciprocal throughput. -->

---
layout: section
---

# Where should the CPU fetch next?

Branch prediction and speculative execution

---

# Waiting for every branch would starve the CPU

```asm
mov eax, DWORD PTR [rdi]
test eax, eax
je .zero
; nonzero path
```

The branch needs flags; the flags need the load's result.

The front end wants the next instruction address **before** that result arrives.

<div class="callout">Predict a path, start useful work early, then verify the prediction.</div>

<!-- Even a register-only comparison is resolved later than the front-end prediction. A slow load makes the problem more obvious. -->

---

# Prediction has several jobs

| Question | Mechanism, conceptually |
|---|---|
| Is a conditional branch taken? | Direction predictor |
| Where does a taken branch go? | Branch target prediction / BTB |
| Where does an indirect call or jump go? | Indirect target prediction |
| Where does a function return? | Return-address prediction stack |

Predictors learn from instruction addresses and execution history.

<div class="note">Modern predictors are more sophisticated than “guess the most frequent outcome.” Exact designs vary.</div>

<!-- Loops often have predictable behavior. A balanced branch can be predictable if its outcome sequence has structure. -->

---

# A two-bit predictor: a teaching model

| State | Predict | On taken | On not taken |
|---|---|---|---|
| Strongly not taken | N | Weakly N | Strongly N |
| Weakly not taken | N | Weakly T | Strongly N |
| Weakly taken | T | Strongly T | Weakly N |
| Strongly taken | T | Strongly T | Weakly T |

A single unusual result does not immediately reverse a strong prediction.

For a mostly taken loop branch, the exit can be the unusual outcome.

<div class="note">Real predictors combine richer histories and multiple structures; this is an intuition-building example.</div>

<!-- Start strongly taken and walk T,T,T,N,T. There is one miss at N; the next T is predicted correctly. -->

---

# A wrong prediction discards younger work

<div class="flow"><b>Predict taken</b> → <b>Execute speculatively</b> → <b>Resolve: not taken</b> → <b>Redirect / refill</b></div>

- Older valid work is preserved.
- Younger wrong-path results do not retire.
- Recover the correct state and fetch from the actual target.
- Wrong-path work consumes time, energy, and execution resources.

<div class="callout">Recovery cost depends on the CPU and when the branch resolves; there is no universal fixed penalty.</div>

<!-- Architectural state is restored, but cache and predictor effects may remain. This distinction is also relevant to speculative-execution side channels. -->

---

# Predictability depends on the sequence

| Outcomes over time | What a predictor may learn |
|---|---|
| T T T T T T … | Stable direction |
| T N T N T N … | A repeating pattern |
| T T T N T T T N … | A history-dependent pattern |
| Independent random 50/50 | Little useful predictive information |

Two datasets can execute the same number of taken branches with very different miss rates.

<div class="callout">A 50% taken rate does not imply a 50% misprediction rate.</div>

<!-- Predictor capacity, aliasing, warm-up, and surrounding branches also matter. Do not promise that every repeating pattern is learned perfectly. -->

---

# Branchless code trades control for data flow

```asm
; unsigned min(a, b), a in edi, b in esi
mov eax, edi
cmp edi, esi
cmova eax, esi       ; choose b if a > b
```

- `cmov` selects using flags without a conditional jump.
- It creates data dependencies on the candidate values and condition.
- Computing both expensive candidates may cost more than a good branch.
- A compiler may already turn an `if` into conditional moves or SIMD.

<div class="callout">Compare generated code and realistic inputs before choosing branchless code.</div>

<!-- Example uses register operands. A memory-source CMOV must not be used as a guard against an invalid memory access. -->

---
class: exercise
---

# Checkpoint: should we remove this branch?

```cpp
if (active[i])
  sum += expensive_mix(values[i]);
```

| Workload | Hypothesis to test |
|---|---|
| Almost always inactive | Branch can skip most of the computation |
| Random 50/50 activity | Mispredictions may make eager work attractive |
| Always active | Predictor may make the branch inexpensive |

<div class="note">Use the same semantics: eager evaluation must be valid even for inactive elements.</div>

<!-- Refer back to lecture 1's conditional/eager measurements. Ask what vectorization does to the comparison. Branchless is not automatically faster. -->

---

# Memory is another source of waiting

<div class="flow"><b>Registers</b><b>L1 data cache</b> → <b>L2</b> → <b>Last-level cache</b> → <b>DRAM</b></div>

- A load needs address generation, address translation, and data access.
- The **TLB** caches virtual-to-physical address translations.
- Caches transfer data in cache lines, not single C++ objects.
- Hardware prefetchers can bring predictable streams closer to the core.

<div class="note">Cache sizes, sharing, line size, latency, and bandwidth depend on the CPU. Registers are shown for context, not as a cache lookup level.</div>

<!-- A TLB miss can require a page-table walk, which itself accesses the cache hierarchy. Avoid presenting one latency table as universal. -->

---

# Pointer chasing versus independent loads

```cpp
// Each next address needs the previous load.
for (Node* p = head; p; p = p->next)
  sum += p->value;
```

```cpp
// Future addresses are computable independently.
for (std::size_t i = 0; i < n; ++i)
  sum += values[i];
```

**Memory-level parallelism:** multiple outstanding memory accesses.

<div class="callout">A sequential array exposes predictable addresses; a pointer chain can serialize cache misses.</div>

<!-- Array reductions still have arithmetic dependencies, which can be cheap or transformed. Cache locality and vectorization also differ; do not attribute all improvement solely to OoO. -->

---

# Stores complicate memory dependencies

```asm
mov QWORD PTR [rdi], rax   ; store
mov rbx, QWORD PTR [rsi]   ; can this load run early?
```

- Register names differ, but the addresses might refer to the same bytes.
- Store queues track pending stores; forwarding can supply a matching load.
- Memory-dependence prediction may allow a younger load to run early.
- If an ordering assumption was wrong, affected work must be replayed.

<div class="callout">CPU scheduling does not remove C++ aliasing rules or the need for synchronization between threads.</div>

<!-- Hardware preserves the ISA memory contract. Compiler reordering, hardware execution order, and cross-core visibility are distinct subjects. -->

---

# Three kinds of parallelism

| Kind | What runs together? | Example |
|---|---|---|
| Instruction-level | Independent operations in one thread | OoO and multiple execution units |
| Data-level | Multiple lanes of one vector operation | SIMD addition of several elements |
| Thread-level | Work in separate software threads | Multiple cores; SMT shares a core |

- SIMD does not eliminate dependency chains.
- Multiple accumulators can expose independent chains.
- Unrolling can help, but also enlarge code and increase register pressure.

<!-- Tie to the four-accumulator example. An optimizing compiler may already vectorize and interleave the plain loop, so source-level changes may duplicate or hinder its work. -->

---

# Estimate a lower bound, then find the bottleneck

For a steady-state loop, consider cycles per iteration from:

- **Dependencies:** critical recurrence latency.
- **Execution resources:** operations competing for units.
- **Front end / retirement:** work admitted and completed per cycle.
- **Memory:** available bandwidth and outstanding requests.

<div class="callout">Idealized lower bound ≈ maximum of these constraints, not their sum.</div>

Branch misses, cache misses, and resource interactions can make execution slower.

<!-- These limits overlap. A lower bound is not a complete performance prediction. Latency and bandwidth can interact with a finite execution window. -->

---

# A practical investigation loop

1. Measure an optimized build with representative inputs.
2. Read the hot loop's assembly: loads, branches, SIMD, dependencies.
3. Form one hypothesis: prediction, dependency, resource, or memory limit.
4. Change one relevant property and compare repeated measurements.

```sh
# Run from the lecture directory on a Linux x86-64 system.
g++ -O3 -std=c++20 -S -masm=intel examples.cpp -o /tmp/cpu-examples.s
# With a separately built benchmark executable:
perf stat -r 5 -e cycles,instructions,branches,branch-misses ./benchmark
```

<div class="note">Counters depend on CPU and OS support/permissions. IPC = retired instructions / cycles; it is evidence, not a speed ranking.</div>

<!-- examples.cpp accompanies this deck. These functions are inspection material, not a benchmark. A low IPC alone does not identify the bottleneck. -->

---
class: exercise
---

# Lab: connect the code to the machine

Use the accompanying `examples.cpp`.

1. Compile `affine`, `sum`, and `sum_four`; identify argument registers.
2. Find vector instructions and loop-carried dependencies in each sum.
3. Inspect `sum_active`: branch, conditional move, or vector mask?
4. Design a benchmark with constant, alternating, and random activity.
5. Predict which counters would support your explanation.

<div class="callout">Record CPU, compiler, flags, input size, input pattern, and whether the data is cache-resident.</div>

<!-- Suggested discussion: run both cache-resident and larger datasets; prevent dead-code elimination; generate input outside timing; keep results observable. Do not infer cycles from static instruction count. -->

---

# What to remember when reading C++

- Assembly exposes operations and dependencies, not a cycle schedule.
- Pipelines overlap stages; superscalar cores overlap operations.
- Out-of-order execution finds ready work; renaming removes name conflicts.
- Prediction keeps the front end busy; mistakes waste speculative work.
- True dependencies and finite resources limit overlap.
- The best optimization changes the bottleneck that measurement reveals.

<!-- Final verbal exercise: ask why fewer instructions, fewer branches, and more accumulators can each lose. -->

---

# References and further exploration

- [Intel Software Developer Manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html): architecture, instruction semantics, and programming environment.
- [Intel Optimization Reference Manual, volume 1](https://cdrdv2-public.intel.com/821612/248966-Optimization-Reference-Manual-V1-050.pdf): execution pipeline and performance analysis.
- [AMD Zen 5 Software Optimization Guide](https://docs.amd.com/v/u/en-US/58455_1.00): a concrete modern core.
- [Agner Fog: The Microarchitecture of Intel, AMD, and VIA CPUs](https://www.agner.org/optimize/microarchitecture.pdf): dependencies, pipelines, and comparisons.
- [uops.info](https://uops.info/): measured instruction latency, throughput, and port usage by CPU.

<div class="note">Use the manual and measurements for your target CPU. All numerical pipeline examples in this lecture are explicitly defined teaching models.</div>

<!-- These are primary vendor references and original measurement resources. No benchmark results are invented for this lecture. -->
