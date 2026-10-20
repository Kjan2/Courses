---
theme: default
title: "Performance C++ 3 — Memory: from the OS to the silicon"
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
# 3

Memory: from the OS to the silicon

<div class="subtitle">Addresses · pages · caches · sharing · measurement</div>

<!--
Three teaching sessions, each 75–100 minutes, plus an optional lab. Use the explicit session summaries as stopping points. Prerequisites: C++ arrays, pointers, object lifetime, ordinary threads, and the CPU model from lecture 2. No prior OS memory-management knowledge is assumed. The appendix preserves advanced implementation and operational material. All numerical models are labeled; measured results retain their original data and environment.
-->

---

# One buffer, several performance questions

```cpp
constexpr std::size_t n = 1u << 20;  // 1,048,576 elements
auto data = std::make_unique_for_overwrite<std::uint64_t[]>(n);
for (std::size_t i = 0; i < n; ++i) data[i] = i;
```

Why can initialization cost more than a later pass over the same buffer?

Why can random access cost more than scanning it in order?

What changes when several workers read or write it?

<div class="callout">We will follow this buffer from the program's addresses to the bytes moved by the machine.</div>

<div class="source"><a href="https://eel.is/c++draft/unique.ptr.create">C++: make_unique_for_overwrite</a></div>

<!--
This is C++20; include memory, cstdint, and cstddef. make_unique_for_overwrite starts the element lifetimes without zero-initializing these integers; initialize each value before reading it. The allocator may reuse storage, so this C++ call alone does not guarantee a fresh OS mapping. Fresh-mapping experiments are identified separately.
-->

---

# What you will be able to explain

You already know C++ pointers, arrays, object lifetime, and basic threads.

By the end, you should be able to:

1. Explain what happens when the program first writes a newly obtained page.
2. Trace an address lookup and distinguish three different kinds of misses.
3. Predict which bytes a loop makes the machine fetch or share.
4. Choose a layout or scheduling change and test its performance explanation.

<!--
Use these as learning outcomes, not prerequisites. Lecture 2 supplies instruction dependencies and out-of-order execution. Expand memory-specific vocabulary here as it becomes necessary.
-->

---

# Three sessions and an optional appendix

| Session | Main question | Worked example |
|---|---|---|
| **1 · OS memory** | What storage does the program actually obtain? | One buffer's first write |
| **2 · CPU memory** | How does the CPU find and fetch its bytes? | Cache accesses and address lookups |
| **3 · C++ design** | How do layout and multiple workers change the cost? | Shared writes and data placement |

The **appendix** contains real CPU details, Linux controls, and further experiments.

<div class="note">Pause at each session summary. The complete deck is a course unit, not a single sitting.</div>

<!--
Allow 75–100 minutes per session including the short exercises, then a separate 45–60 minute lab. Appendix slides can be selected for an advanced audience or assigned as follow-up. The README and teaching guide list the exact slide boundaries.
-->

---

# Units we will use

| Unit | Meaning | Example |
|---|---|---|
| Byte, B | 8 bits | One `uint64_t` uses 8 B |
| KiB / MiB / GiB | 2¹⁰ / 2²⁰ / 2³⁰ bytes | Our buffer occupies **8 MiB** |
| ns / ms | 10⁻⁹ / 10⁻³ seconds | 1 ms = 1,000,000 ns |
| GB/s | 10⁹ bytes per second | A rate of moving bytes |

**Footprint** means the amount of storage occupied or touched by the data under discussion.

<div class="note">Always say whether a number is per element, per page, or per whole pass.</div>

<!--
Distinguish decimal GB and binary GiB. Sizes in the main examples use binary units unless explicitly labeled otherwise. Footprint needs a boundary: allocation, hot fields, cache lines, or pages.
-->

---
class: section
---

# Session 1 · From addresses to usable storage

The operating system (OS) manages memory for running programs.

We will separate **where the program may access**, **where bytes live**, and **when work is performed**.

<!--
Start with the abstraction and an actual address, then introduce allocation and faults. Do not introduce cache or TLB vocabulary yet.
-->

---

# Two meanings of an address

A **process** is a running program with its own memory view.

- A **virtual address** is the byte address used by that process's instructions.
- A **physical address** identifies a byte in physical RAM (random-access memory).
- An **address space** is the set of addresses available to a process.

<div class="flow"><b>Process A<br>virtual 0x10034</b> → <b>Address translation<br>look up the correspondence</b> → <b>Physical memory<br>0x2A034</b></div>

**Translation** converts a virtual address into a physical address using mappings established by the OS.

<div class="source"><a href="https://docs.kernel.org/admin-guide/mm/concepts.html">Linux memory concepts</a> · <a href="https://docs.kernel.org/mm/page_tables.html">page tables</a></div>

<!--
Use the addresses as an illustrative model, not observations of an actual allocation. Hardware ordinarily performs the translation; the OS establishes and changes the mapping rules. Main scope is Linux on a CPU with hardware address translation.
-->

---

# Why give each process its own addresses?

<div class="flow"><b>Process A<br>virtual 0x10034</b> → <b>Its mapping</b> → <b>Physical 0x2A034</b></div>
<div class="flow"><b>Process B<br>virtual 0x10034</b> → <b>Its mapping</b> → <b>Physical 0x71034</b></div>

- Each process can use the same numeric addresses for different data.
- The OS can deny reads, writes, or execution in selected regions: **protection**.
- It can deliberately map the same physical storage into both processes: **sharing**.

<div class="source"><a href="https://docs.kernel.org/admin-guide/mm/concepts.html">Linux memory concepts</a> · <a href="https://docs.kernel.org/mm/page_tables.html">page tables</a></div>

<!--
Address-space isolation is an abstraction with controlled sharing. Virtual memory provides these benefits even with no swap device. Do not imply C++ pointers alone grant permission or prove an object is alive.
-->

---

# Pages, frames, and offsets

A **page** is a fixed-size block of virtual memory. A **frame** is a block of physical memory that can hold a page.

Teaching example: **4 KiB = 4,096 B per page and frame**, our **base** (ordinary small) page size.

| Consecutive virtual pages in our buffer | Physical frames |
|---|---|
| Page 16: addresses 0x10000…0x10FFF | Frame 42: 0x2A000…0x2AFFF |
| Page 17: addresses 0x11000…0x11FFF | Frame 7: 0x07000…0x07FFF |

An **offset** is the byte's position within a page: 0…4,095 here.

<div class="callout">Consecutive virtual pages can use physical frames far apart. Translation preserves the offset.</div>

<div class="source"><a href="https://docs.kernel.org/admin-guide/mm/concepts.html">Linux memory concepts</a> · <a href="https://docs.kernel.org/mm/page_tables.html">page tables</a></div>

<!--
Draw two equally sized boxes and move the page label while keeping the byte position highlighted. Frame is used for physical storage and page for virtual subdivision in this explanation; real documentation sometimes uses page for both. Larger page sizes come later.
-->

---

# Translate one address by hand

Virtual address **0x10034**; 4 KiB pages; virtual page 16 maps to frame 42.

<div class="address"><span>Page base<br>0x10000</span><span>Offset<br>0x034 = 52 B</span></div>

1. Split: **0x10034 = 0x10000 + 0x034**.
2. Look up page 16 → frame 42, whose base is **0x2A000**.
3. Keep the offset: **0x2A000 + 0x034 = 0x2A034**.

<div class="flow"><b>Virtual page 16<br>byte 52</b> → <b>Replace page base<br>preserve byte position</b> → <b>Physical frame 42<br>byte 52</b></div>

<!--
Ask students to point to the part that changes and the part that stays. Hexadecimal 0x1000 is 4096, so the last three hex digits are the offset in this example. No real physical address is being exposed.
-->

---
class: exercise
---

# Try it: which part of the address changes?

4 KiB pages. Virtual page **1** maps to physical frame **7**.

1. Translate virtual address **0x1020**.
2. Translate the next byte, **0x1021**.
3. Do you need a different page mapping for that next byte?


<v-click>
<div class="callout">**0x7020**, then **0x7021**. Both keep the offset and use the same page-to-frame mapping.</div>
</v-click>

<!--
Allow 45–60 seconds before revealing. Ask for the split into base and offset, not just the final number. These are address arithmetic exercises, not permission to dereference a fabricated C++ pointer.
-->

---

# A region says what is allowed; an entry says where

A **mapping** associates a virtual region with access rules and a source of storage.

<div class="grid2">
<div class="panel"><h2>Virtual memory area (VMA)</h2>
Linux's description of one address range: for example, our 8 MiB buffer is writable.</div>
<div class="panel"><h2>Page-table entry (PTE)</h2>
An entry used to describe an individual translation: for example, page 16 → frame 42, writable.</div>
</div>

The **page table** is the OS-maintained structure the CPU consults for translations.

<div class="note">The allowed region can exist before all of its page-to-frame translations have been installed.</div>

<div class="source"><a href="https://docs.kernel.org/admin-guide/mm/concepts.html">Linux memory concepts</a> · <a href="https://docs.kernel.org/mm/page_tables.html">page tables</a></div>

<!--
First present the flat lookup-table model. Multilevel organization comes in session 2 and real x86 names in the appendix. There need not be one Linux VMA per allocator block: this example is an illustrative dedicated mapping.
-->

---

# Mapped, resident, and backed mean different things

| Term | Plain meaning | Our buffer |
|---|---|---|
| **Mapped** | The OS has established an address region and access rules | This range may be used for storage |
| **Resident** | The page's contents are currently held in physical RAM | A physical frame holds its bytes |
| **Backing** | The source/storage associated with the contents | Newly supplied zeros or a file, for example |

<div class="flow"><b>Region established<br>pages may be untouched</b> → <b>Pages accessed<br>OS supplies storage as needed</b> → <b>Contents resident<br>in physical frames</b></div>

**Mapped** does not imply that every page already has private resident storage.

<div class="source"><a href="https://docs.kernel.org/admin-guide/mm/concepts.html">Linux memory concepts</a> · <a href="https://docs.kernel.org/mm/page_tables.html">page tables</a></div>

<!--
Mapped is often used loosely; distinguish a VMA covering a range from a present hardware leaf entry. Also say that resident is about physical backing, not proximity to a CPU. Some resident pages are shared, and some accesses need no new allocation.
-->

---

# Where can a page get its contents?

<div class="grid2">
<div class="panel"><h2>Anonymous memory</h2>
Ordinary memory without an ordinary file supplying its initial contents.
<p>Newly supplied bytes appear as zeros. Our buffer normally uses this kind of storage.</p></div>
<div class="panel"><h2>File-backed memory</h2>
A file supplies the contents of the mapped pages.
<p>A program can use <code>mmap</code> to access file contents through memory addresses.</p></div>
</div>

**Private** means this mapping's changes are private; **shared** means mappings can access the same changing storage.

<div class="source"><a href="https://docs.kernel.org/admin-guide/mm/concepts.html">Linux memory concepts</a> · <a href="https://docs.kernel.org/mm/page_tables.html">page tables</a></div>

<!--
Anonymous and private are separate dimensions. Shared anonymous mappings also exist; the appendix has the full matrix. An allocator can recycle previously used anonymous storage; malloc does not promise zeros. The initial-zero guarantee concerns fresh anonymous OS pages.
-->

---

# Return to the buffer: allocation has several stages

```cpp
auto data = std::make_unique_for_overwrite<std::uint64_t[]>(n);
for (std::size_t i = 0; i < n; ++i) data[i] = i;
```

An **allocator** obtains and reuses storage for C++ objects.

<div class="flow"><b>C++ requests<br>8 MiB</b> → <b>Allocator reuses<br>or obtains storage</b> → <b>OS establishes<br>regions if needed</b> → <b>Our loop writes<br>the elements</b></div>

The request can be satisfied before all later writes have done their OS memory work.

<div class="note">For the next example, assume a fresh private anonymous mapping, rather than reused allocator storage.</div>

<div class="source"><a href="https://man7.org/linux/man-pages/man3/malloc.3.html">malloc</a> · <a href="https://www.man7.org/linux/man-pages/man2/mmap.2.html">mmap</a></div>

<!--
This is an explicit shift from a general C++ allocation to a controlled OS case. Construction/default initialization does not zero these integer elements. All reads in later examples follow the initialization loop. Do not promise one syscall per new.
-->

---

# First write: follow one page through a fault

A **page fault** is a CPU exception raised when an access cannot complete using the current mapping or permissions.

| Step | Virtual page 16 | What happens |
|---|---|---|
| Before `data[0] = 0` | Region allows a write; no private frame yet | CPU cannot finish the store |
| Fault enters the kernel | The OS checks that the access is legal | Allocate/clear a frame; install a writable entry |
| Retry the same store | Page 16 → frame 42, writable | The instruction writes its value |

<div class="callout">A page fault can be a normal part of a successful access. The program need not crash.</div>

<div class="source"><a href="https://docs.kernel.org/admin-guide/mm/concepts.html">Linux memory concepts</a> · <a href="https://docs.kernel.org/mm/page_tables.html">page tables</a></div>

<!--
Frame 42 is illustrative. Allocation can involve accounting, reclaim, or other work, which is deferred until after the basic case. The instruction is retried after resolution. A forbidden access cannot be repaired merely by assigning a frame.
-->

---

# Minor fault, major fault, or invalid access?

| Case | Meaning | Example |
|---|---|---|
| **Minor fault** | Resolve the fault without fetching the page from storage | Our fresh anonymous page gets a frame |
| **Major fault** | Resolution requires fetching page contents through I/O | A needed file or swapped page must be read |
| **Invalid access** | The OS cannot allow the requested access | Write to a read-only forbidden region |

**I/O** means input/output, such as a storage read. Invalid accesses commonly produce a signal such as `SIGSEGV`.

<div class="note">“Minor” describes the absence of a page fetch from storage. Allocation or copying can still be expensive.</div>

<div class="source"><a href="https://docs.kernel.org/admin-guide/mm/concepts.html">Linux memory concepts</a> · <a href="https://docs.kernel.org/mm/page_tables.html">page tables</a></div>

<!--
Keep these definitions on the slide, since the first checkpoint and benchmark will use them. Fault accounting and file readahead have subtleties; the table gives the basic Linux distinction. SIGBUS is another possible signal, e.g. for some invalid file-mapping accesses.
-->

---

# Why reading first may leave work for the first write

For ordinary fresh **private anonymous** memory, Linux can use a **zero page**: a shared, read-only frame full of zeros.

<div class="flow"><b>First read<br>must observe zero</b> → <b>May map the shared<br>read-only zero page</b> → <b>No private writable<br>frame required yet</b></div>
<div class="flow"><b>Later write<br>needs private bytes</b> → <b>Fault: allocate and clear<br>a private frame</b> → <b>Retry write<br>to private storage</b></div>

<div class="callout">To prepare writable pages, perform the writes required by the real workload before the critical phase.</div>

<div class="source"><a href="https://docs.kernel.org/admin-guide/mm/concepts.html">Linux memory concepts</a> · <a href="https://docs.kernel.org/mm/page_tables.html">page tables</a></div>

<!--
The zero-page path is permitted by ordinary Linux behavior, not a universal promise for every mapping/policy. Larger page optimizations and special mappings can change the path; they are appendix material. The conclusion is that read-only preparation is insufficient as a guarantee.
-->

---

# How to read the measured examples

- A **repetition** is another measurement of the same case.
- The **median** is the middle result after sorting the repetitions.
- “ns/page” means elapsed time divided by the number of pages touched.
- A **teaching model** uses stated assumptions; a **measurement** records a real run.
- **Warm-up** is an untimed run preparing the experiment's state.
- **Steady state** means repeated operation after initial setup and faults.

Recordings use **seven repetitions** in a Linux virtual machine on a reported AMD Ryzen 5 4600H. Each experiment states its size and layout.

<div class="note">The machine is a WSL2 guest (Windows Subsystem for Linux 2). Host activity and CPU frequency were uncontrolled. Full conditions and raw ranges are in the appendix and results files.</div>

<div class="source">Evidence: benchmarks/results/raw.csv and metadata.json; timings and OS fault counts.</div>

<!--
Introduce interpretation before the first result. GCC 13.3 and optimized C++20; the process was pinned to guest logical CPU 2. The full methodology slide is in the appendix. Warm traversals do not imply the entire data fits the smallest cache. Repeat relevant units and setup differences on each benchmark.
-->

---
class: exercise
---

# Predict: first write and second write

A fresh private anonymous mapping occupies **64 MiB** with **4 KiB pages**.

We disable larger-page allocation and write one byte in each page, twice.

1. How many pages do we touch?
2. Which pass should allocate private frames?
3. Should every store in the second pass cause another page fault?


<v-click>
<div class="callout">**64 MiB ÷ 4 KiB = 16,384 pages.** The first write pass supplies the private frames. With those pages still present and writable, the second pass can use them.</div>
</v-click>

<!--
Give students one minute. This is exactly the protocol of the recorded first_write/resident_write experiment. Assume no intervening reclaim or protection changes. Do not claim every possible kernel configuration faults once per page; the recorded setup explicitly disables THP.
-->

---

# Observe: the first write does more work

Fresh private anonymous **64 MiB** mappings; **4 KiB pages**, larger-page allocation disabled for each mapping.

| One byte written per page | Median ns / page | Minor faults / pass |
|---|---:|---:|
| First write | 1,704.18 | 16,384 |
| Second, resident write | 46.15 | 0 |

The first pass is **36.9× slower** in this environment.

<div class="note">First writes include allocation, zeroing, mapping work, and data access. The second pass still touches 16,384 lines and translations.</div>
<div class="source">Measured with steady_clock + getrusage; fresh mmap per repetition; mapping/unmapping excluded from timing. Counts are present in raw.csv.</div>

<!--
Volatile stores are used intentionally for exactly one store per page; volatile is not used as a general benchmark barrier. The second write is a single timed pass, not calibrated 40 ms; it has a different protocol because repeating the first write requires a fresh mapping. This is not full-buffer write bandwidth.
-->

---
class: exercise
---

# Check the explanation: did reading prepare the buffer?

The program maps 8 GiB, reads one byte per page, then writes to every page.

1. Must it already have 8 GiB of private resident storage after the reads?
2. Can the write pass still incur minor faults?
3. Explain which page-table entry or physical storage might change.


<v-click>
<div class="callout">**No; yes.** Reads may use a shared read-only zero page. A write can fault to obtain private writable storage and install a new entry.</div>
</v-click>

<!--
This checkpoint uses only concepts already visible in session 1. Physical NUMA placement is deliberately absent; its checkpoint follows the placement lesson in session 3.
-->

---

# Memory usage: whose bytes are being counted?

| Metric | Full name / meaning |
|---|---|
| **VSZ** | Virtual size: mapped/reserved address ranges |
| **RSS** | Resident set size: resident memory mapped by this process |
| **PSS** | Proportional set size: shared resident memory divided among its users |

Example: a process has **4 MiB private** plus **2 MiB shared equally with one other process**.

**RSS = 6 MiB; PSS = 4 + 2/2 = 5 MiB**.

<div class="note">RSS counts the shared storage in each process; adding processes' RSS can count the same physical storage repeatedly.</div>

<div class="source"><a href="https://man7.org/linux/man-pages/man5/proc_pid_smaps.5.html">Linux smaps accounting</a></div>

<!--
The example ignores other mappings and kernel bookkeeping. PSS is an apportioned resident count, not all memory charged to a container. VSZ can be much larger than physical usage. The appendix shows how to inspect Linux mappings.
-->

---

# A file can supply resident pages

The OS **page cache** keeps file contents in RAM so later accesses can reuse them without another storage read.

<div class="flow"><b>File on storage</b> → <b>Resident file pages<br>in OS page cache</b> → <b>Program reads<br>the contents</b></div>

- Buffered `read` usually copies from these pages into a program buffer.
- File `mmap` can map those contents into the process's address space.
- A **clean** page matches its backing file; a **dirty** page has changes awaiting writeback.

<div class="note">The CPU has additional, smaller caches. We introduce them in session 2.</div>

<div class="source"><a href="https://docs.kernel.org/mm/page_cache.html">Linux page cache</a> · <a href="https://www.man7.org/linux/man-pages/man2/mmap.2.html">mmap</a></div>

<!--
Define dirty here before reclaim. Normal buffered files are the scope; direct I/O and DAX use other paths. Visibility of changed bytes and durable storage are distinct. Avoid implying mmap makes every access free.
-->

---

# Copy-on-write: share first, copy when needed

`fork()` creates a child process from a parent. **Copy-on-write (COW)** lets their private pages share physical storage until a write requires separation.

| State | Parent's page | Child's page |
|---|---|---|
| After fork | Frame A, value **7**, writes protected | Same frame A, value **7**, writes protected |
| Parent writes **9** | Fault → copy A into B → write **9** in B | Keeps frame A and value **7** |

<div class="flow"><b>One shared frame<br>A: 7</b> → <b>Write fault<br>copy contents</b> → <b>Separate frames<br>parent B: 9 · child A: 7</b></div>

Changing one byte can require copying a whole page.

<div class="source"><a href="https://man7.org/linux/man-pages/man2/fork.2.html">fork</a></div>

<!--
This example assumes the old page remains needed by the child. If no other mapping needs the old page, a copy can sometimes be avoided. fork still has task/page-table costs. Private file writes use a related private-copy mechanism.
-->

---

# What happens when physical memory runs short?

The **working set** is the data actively needed during the phase being studied.

The OS can **reclaim** a frame: recover it for another use.

| Contents | How can the OS preserve them? |
|---|---|
| Clean file page | Discard; reread from the file later |
| Dirty file page | Write changes back before reclaiming it |
| Eligible anonymous page | Move contents to **swap**, a place for temporarily evicted data |

Repeatedly evicting and reloading needed pages is **thrashing**. It makes a too-large working set very slow.

<div class="source"><a href="https://docs.kernel.org/admin-guide/mm/concepts.html">Linux memory concepts</a> · <a href="https://docs.kernel.org/mm/page_tables.html">page tables</a></div>

<!--
Swap may use storage or compressed RAM. If enough memory cannot be recovered, allocation can fail or the OS can terminate a process; detailed out-of-memory and cgroup policies are in the appendix. Working set is phase-dependent; an 8 GiB allocation can have only 100 MiB currently active.
-->

---

# Session 1 · Explain the buffer now

<div class="flow"><b>C++ requests<br>storage</b> → <b>OS region<br>allows access</b> → <b>First write may fault<br>and supply a frame</b> → <b>Later writes reuse<br>the resident page</b></div>

You should now be able to:

- Translate a page number plus offset into a frame plus the same offset.
- Explain mapped versus resident storage.
- Explain a minor fault, a storage-fetching fault, and a forbidden access.
- Predict why read-only preparation and write preparation can differ.

<div class="callout">Next question: once the bytes are resident, how does the CPU fetch them efficiently?</div>

<!--
Stop here for the first session. Ask learners to narrate the first-write example without looking at the slide. Record unresolved terms before continuing.
-->

---
class: section
---

# Session 2 · The CPU fetches blocks and caches lookups

Our buffer already contains initialized, resident data.

We will first follow **the data bytes**, then **the address translation**.

<!--
Start with a recap of session 1. Keep data-cache entries and translation-cache entries visually distinct throughout this session.
-->

---

# What does a CPU cache keep?

A **cache** keeps recently used information in a smaller, faster store.

A CPU **data cache** keeps copies of memory bytes. It transfers a block called a **cache line** at a time.

Example: **64-byte lines**, eight 8-byte elements, starting at a line boundary.

<div class="cache-line"><span class="hot">data[0]</span><span>data[1]</span><span>data[2]</span><span>data[3]</span><span>data[4]</span><span>data[5]</span><span>data[6]</span><span>data[7]</span></div>

Reading `data[0]` can bring all eight elements into the cache together.

<div class="note">64 B is our target example. A cache line and an OS page are different-sized blocks with different jobs.</div>

<div class="source"><a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html">Intel architecture manuals</a> · <a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel64-and-ia32-architectures-optimization.html">optimization manuals</a></div>

<!--
Cache keeps information near the core; it does not change virtual mapping or C++ object lifetime. Cache-line size is a target property. Draw 64 cache lines inside a 4 KiB page if learners confuse the units.
-->

---
class: exercise
---

# Trace three reads: hit or miss?

Assume 64-byte lines, 8-byte elements, and `data[0]` at a line boundary. The data cache starts empty; ignore automatic fetching of future lines.

| Read | Which line is needed? | Already present? |
|---|---|---|
| `data[0]` | Elements 0…7 | ? |
| `data[1]` | Elements 0…7 | ? |
| `data[8]` | Elements 8…15 | ? |

A **hit** finds the needed information in this cache. A **miss** must obtain it from elsewhere.


<v-click>
<div class="callout">**Miss, hit, miss.** The first read fetched neighboring bytes. The second uses them; the third needs another line.</div>
</v-click>

<!--
Ask learners to identify line numbers before predicting hits. This is a simplified demand-access trace; real prefetchers can fetch the next line early.
-->

---

# Several places can supply the line

| Level | Meaning | Usual character |
|---|---|---|
| **L1D / L1I** | Level-1 data / instruction caches | Small and close to a core |
| **L2** | Level-2 cache | Larger, usually slower than L1 |
| **LLC** | Last-level cache; often L3 | Larger again; often shared |
| **DRAM** | Dynamic random-access memory | Main-memory capacity |

Caches commonly use **SRAM**, static random-access memory: fast but less dense than DRAM. DRAM needs periodic refresh of its stored charge.

<div class="note">Exact levels, sharing, sizes, and timings depend on the CPU. A hit in L3 can still be much slower than a hit in L1.</div>

<div class="source"><a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html">Intel architecture manuals</a> · <a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel64-and-ia32-architectures-optimization.html">optimization manuals</a></div>

<!--
Registers, introduced in lecture 2, hold operands directly used by instructions. They are not another address-lookup cache level. Real cache topologies and DRAM organization are appendix topics.
-->

---

# Locality: use the bytes before they leave

**Eviction** removes a line to make room for another line.

- **Spatial locality:** use nearby bytes, such as `data[1]` after `data[0]`.
- **Temporal locality:** reuse the same bytes before they are evicted.
- **Hot** data is accessed frequently in the current phase; **cold** data is accessed rarely.

One 8-byte hot field inside a 64-byte record:

<div class="cache-line"><span class="hot">hot</span><span>cold</span><span>cold</span><span>cold</span><span>cold</span><span>cold</span><span>cold</span><span>cold</span></div>

**8 / 64 = 12.5%** of each fetched line is useful to this pass.

<!--
Ask what happens if the next operation uses all eight fields: the useful fraction depends on the operation mix. Useful bytes are not the same as transferred line bytes at a chosen cache boundary.
-->

---
class: exercise
---

# Predict: move the hot field into its own array

We scan **1,048,576 records**, reading one 8-byte field from each.

<div class="grid2"><div class="panel"><h2>Array of structures (AoS)</h2>Each record holds one hot field and seven cold fields.<p>64 MiB of records.</p></div>
<div class="panel"><h2>Structure of arrays (SoA)</h2>Each field has its own dense array.<p>8 MiB for the field used by this scan.</p></div></div>

Which layout should move fewer unused bytes? Which will need fewer pages for this pass?


<v-click>
<div class="callout">The dense hot-field array uses more of each fetched line and occupies fewer pages. Predict a benefit for this scan; whole-record operations can favor a different layout.</div>
</v-click>

<!--
Define both names before showing the code. Let students calculate the footprint ratio. It does not by itself predict the exact speedup, because caches, request overlap, and compiler vectorization also matter.
-->

---

# Change: scan a dense hot-field array

**Alignment** is an address-boundary requirement. Here `alignas(64)` starts each record at a 64-byte boundary.

<div class="grid2">
<div>

```cpp
struct alignas(64) Record {
  std::uint64_t value;
  std::uint64_t cold[7];
};
for (std::size_t i = 0; i < n; ++i)
  sum += records[i].value;
```

</div><div>

```cpp
// Same values, densely stored.
std::vector<std::uint64_t> values;

for (std::size_t i = 0; i < n; ++i)
  sum += values[i];
```

</div>
</div>

Workload: **1,048,576 records**; both kernels compute the same unsigned sum.

<div class="note">This is a one-field scan. Whole-record operations can favor an array of structures.</div>

<!--
The harness initializes both arrays once, outside timing. A full 64-byte record is intentionally wasteful for a one-field scan. This is a controlled access-pattern example, not a claim that every AoS layout is bad.
-->

---

# Less hot data, faster scan

| Layout | Hot allocation | Median ns / record | Full pass |
|---|---:|---:|---:|
| AoS, 64-byte records | 64 MiB | 4.374 | 4.587 ms |
| Dense values | 8 MiB | 0.503 | 0.527 ms |

**8.70× faster** for this pass.

- Fewer fetched bytes and a smaller working set are plausible contributors.
- SIMD code generation and which cache level holds the data can change too.
- Check assembly and counters before assigning the entire gain to bandwidth.

<div class="source">Local medians, seven repetitions. Useful payload is 8 MiB in both cases; no measured DRAM traffic claim.</div>

<!--
Rounded pass times are ns/record times 1,048,576. Dividing 8 MiB useful payload by time gives effective payload bandwidth, not physical memory bandwidth. The two allocations can have different residency levels and vectorization.
-->

---

# A small cache: sets, ways, and tags

This teaching cache holds **eight lines**, organized into **four sets** of **two slots** each. Each slot is called a **way**.

| Set: group a line may enter | Way 0 | Way 1 |
|---|---|---|
| 0 | One line | One line |
| 1 | One line | One line |
| 2 | One line | One line |
| 3 | One line | One line |

For this model: **set = physical line number % 4**.

A **tag** identifies which particular memory line occupies a slot. An **index** selects the set; the tag check selects a matching line within it.

<!--
Use numbered physical cache lines as the input so translation is not another unknown. The line number is the physical byte address divided by the line size. Two ways means any of the two slots in the selected set may hold the line, not two copies of each line.
-->

---

# Watch one set fill, hit, and evict

Access physical line numbers **0, 4, 0, 8, 4**. Each selects **set 0**.

| Access | Hit / miss | Set 0 after access | Explanation |
|---|---|---|---|
| 0 | Miss | **0**, empty | Fetch line 0 |
| 4 | Miss | 0, **4** | Fetch line 4 into the other way |
| 0 | Hit | **0**, 4 | Reuse line 0 |
| 8 | Miss | 0, **8** | Evict line 4 to make room |
| 4 | Miss | **4**, 8 | Re-fetch line 4; evict line 0 |

Here we choose **least recently used (LRU)** replacement: evict the line used longest ago.

<div class="callout">One set is full while the other sets are empty. Reusing the evicted line needs another fetch.</div>

<!--
Replacement is explicitly LRU for this toy trace. Real CPUs can use other policies. Ask learners to point out the difference between total free slots and slots eligible for a particular line.
-->

---
class: exercise
---

# Try it: which line gets evicted?

Same four-set, two-way teaching cache; LRU replacement; initially empty.

Access **1, 5, 1, 9**.

1. Which set receives these lines?
2. Which access is a hit?
3. Which line is evicted by the final access?


<v-click>
<div class="callout">All select **set 1**. The second access to **1** hits. Fetching **9** evicts **5**, the least recently used line in that set.</div>
</v-click>

<!--
Allow a minute to write the two slots after each access. This checks the model before introducing real cache-size arithmetic.
-->

---

# Now calculate a realistic cache geometry

The number of ways is the cache’s **associativity**. Teaching cache: **32 KiB, 64-byte lines, 8 ways**.

**Sets = 32,768 / (64 × 8) = 64**

<div class="address"><span>Tag</span><span>Set index<br>6 bits</span><span>Line offset<br>6 bits</span></div>

In a simple modulo-indexed cache, addresses 4 KiB apart target the same set.

Nine competing hot lines can overflow an eight-way set even when total hot bytes are tiny.

<div class="source"><a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel64-and-ia32-architectures-optimization.html">Intel optimization manuals: cache organization</a></div>

<!--
Cache indexing can involve physical bits or hashes, particularly in larger caches. This exact bit split is an explicit teaching cache, not a claim about all L1 designs.
-->

---

# Three reasons for a data-cache miss

| Name | Plain explanation | Example / candidate change |
|---|---|---|
| **Compulsory** | This line has not been fetched yet | First scan; use more of each line |
| **Capacity** | Too much distinct data was used before reuse | Smaller tiles or a smaller hot representation |
| **Conflict** | Too many needed lines compete for the same sets | Change a stride or layout |

A **stride** is the byte distance between consecutive accesses.

<div class="note">Another core can also invalidate a line. We will demonstrate that mechanism in session 3.</div>

<!--
Distinguish the classic three-C taxonomy from multicore coherence. The note is an explicit preview, not a required new term. A miss can overlap other work, so miss count times a fixed penalty is not a general timing formula.
-->

---

# Transpose: see the access order before the loops

A transpose writes `dst[j][i] = src[i][j]`. In a **row-major** array, a row's elements occupy consecutive addresses.

| Read source coordinates | Write destination element indices, for a 4 × 4 array |
|---|---|
| (0,0), (0,1), (0,2), (0,3) | **0, 4, 8, 12** |
| (1,0), (1,1), (1,2), (1,3) | **1, 5, 9, 13** |

The source reads neighboring elements; the destination jumps between rows.

For the measured 2048-wide `uint32_t` array, that destination stride is **2048 × 4 B = 8 KiB**.

<div class="callout">Can we finish more work on nearby destination bytes before moving to distant rows?</div>

<!--
Use the linear index j*n+i to derive the table. The tiny matrix visualizes order, not cache performance: all of it might fit in a single line on real hardware. The measured large array has very different footprints.
-->

---

# A tile changes the order of visits

A **tile** is a small rectangular part processed before moving to the next part.

<div class="tile-picture">
<span class="tile-a">0,0</span><span class="tile-a">0,1</span><span class="tile-b">0,2</span><span class="tile-b">0,3</span>
<span class="tile-a">1,0</span><span class="tile-a">1,1</span><span class="tile-b">1,2</span><span class="tile-b">1,3</span>
<span class="tile-c">2,0</span><span class="tile-c">2,1</span><span class="tile-d">2,2</span><span class="tile-d">2,3</span>
<span class="tile-c">3,0</span><span class="tile-c">3,1</span><span class="tile-d">3,2</span><span class="tile-d">3,3</span>
</div>

With 2 × 2 tiles, first visit **(0,0), (0,1), (1,0), (1,1)**.

Next visit the top-right, bottom-left, then bottom-right tile. Nearby destination bytes are revisited sooner.

<div class="note">This picture uses tiny tiles to show the order. The measured code uses 32 × 32 tiles.</div>

<!--
Point to the paired source and destination tiles; a source tile transposes into a destination tile. Do not claim the entire destination becomes contiguous. Blocking shortens reuse distance and can also help translation. This is a teaching schedule, not a tuned SIMD transpose.
-->

---
class: compact
---

# Keep a small tile active

```cpp
constexpr std::size_t B = 32;
for (std::size_t ii = 0; ii < n; ii += B)
  for (std::size_t jj = 0; jj < n; jj += B)
    for (std::size_t i = ii; i < std::min(ii+B, n); ++i)
      for (std::size_t j = jj; j < std::min(jj+B, n); ++j)
        dst[j*n + i] = src[i*n + j];
```

| Implementation | Median ns / element | Full transpose |
|---|---:|---:|
| Naive | 14.849 | 62.28 ms |
| 32 × 32 tiled | 7.125 | 29.89 ms |

<div class="note">Measured improvement: <strong>2.08×</strong>. Tile size is an experimental parameter; this is not a tuned transpose library.</div>

<!--
Blocking shortens reuse distance for destination lines and translations. The fixed matrix dimension can create conflicts. Register/SIMD transposes, recursive blocking, or padding can do better; test actual dimensions and tails.
-->

---

# How long must a line survive until reuse?

**Reuse distance** counts the distinct data accessed between uses.

Teaching model: every line can use any slot, with LRU replacement.

| Access order | Data between uses of A | Two-line cache | Three-line cache |
|---|---|---|---|
| A, B, A | One distinct line | A survives | A survives |
| A, B, C, A | Two distinct lines | A was evicted | A survives |

Tiling shortens reuse distance. The **working set** includes all arrays and temporary data needed during that tile.

Three 32 × 32 tiles of `double` occupy **3 × 32 × 32 × 8 B = 24 KiB**.

<!--
This model is fully associative to isolate capacity; real set conflicts add constraints. The three-tile arithmetic is useful for matrix multiplication, not the two-array transpose. Explain why leaving headroom in a 32 KiB cache is sensible.
-->

---

# Latency and bandwidth answer different questions

<div class="grid2">
<div class="panel"><h2>Latency</h2>How long until one requested result is ready?<p>One access waiting for its result exposes it.</p></div>
<div class="panel"><h2>Bandwidth</h2>How many bytes can be delivered per second?<p>Many independent requests can expose it.</p></div>
</div>

**Throughput** measures completed results per second; bandwidth measures bytes per second.

An application can be slow while using little memory bandwidth:
each request waits for the previous request to produce its address.

<div class="callout">Low bandwidth does not prove that memory is unimportant.</div>

<!--
Effective load-to-use time also includes cache/TLB behavior and queueing. A streaming test's throughput does not predict a graph traversal's latency.
-->

---

# Why a pointer chain waits for each load

```cpp
struct alignas(64) Node { Node* next; /* padding to 64 B */ };

for (std::size_t i = 0; i < steps; ++i)
  p = p->next;                 // next address depends on this load
return reinterpret_cast<std::uintptr_t>(p);
```

- A **stream** is one access sequence. Here it follows a random cycle, with one 64 B node per line.
- Initialization and shuffling happen outside timing.
- Repeated traversals measure steady-state effective load-to-use cost.
- The footprint sweep changes data-cache and translation pressure together.

<div class="source">Actual implementation: benchmarks/kernels.cpp · fixed RNG seed; all raw repetitions retained.</div>

<!--
A packed array of pointers would share cache lines and change spatial locality. This benchmark deliberately uses 64-byte nodes. It is not an isolated measurement of one cache's published hit latency.
-->

---

# Draw request overlap

Teaching model: **100 ns per load**; ignore issue overhead; four independent requests can overlap.

<div class="request-timeline">
<span></span><b>0…100 ns</b><b>100…200</b><b>200…300</b><b>300…400</b>
<b>One chain</b><span class="request">A → B</span><span class="request">B → C</span><span class="request">C → D</span><span class="request">D → E</span>
<b>Four chains</b><span class="request parallel">A, B, C, D</span><span></span><span></span><span></span>
</div>

**Memory-level parallelism (MLP)** means multiple memory requests in progress.

<div class="callout">Four results / 100 ns = <strong>25 ns per result</strong>. Each request still takes <strong>100 ns</strong>.</div>

<!--
The timeline is an explicit ideal teaching model, not a measurement. Real hardware has finite queues, issue rate, page walkers, and bandwidth. Lecture 2 explains out-of-order execution: it can issue ready independent loads but cannot invent a dependent address.
-->

---
class: exercise
---

# Predict: will eight streams reduce request latency?

Compare one dependent stream with eight independent streams over the same data footprint.

1. Which version exposes more requests to overlap?
2. Does a lower total time divided by load count prove each individual memory request became faster?
3. Should we expect exactly an 8× speedup on every machine?


<v-click>
<div class="callout">Eight streams expose more overlap. The normalized time measures **throughput per result**; it does not directly measure one request’s latency. Hardware limits and other costs can prevent an exact 8× gain.</div>
</v-click>

<!--
Ask students to refer to the previous timeline. Then reveal the actual recording, keeping the model and observation distinct.
-->

---

# Observe: eight independent streams

```cpp
for (std::size_t i = 0; i < rounds; ++i) {
  a = a->next; b = b->next; c = c->next; d = d->next;
  e = e->next; f = f->next; g = g->next; h = h->next;
}
```

At a **64 MiB** footprint, median time per dereference:

| One dependent stream | Eight streams | Throughput improvement |
|---:|---:|---:|
| **131.41 ns** | **19.57 ns** | **6.71×** |

<div class="note">Same random cycle, eight starting positions spaced around it; equal total dereferences. 19.57 ns is amortized throughput, not individual DRAM latency.</div>

<!--
Each repetition completes a full cycle per single-stream call; eight streams partition a cycle for the large sizes. Requests within each stream remain dependent. This is evidence consistent with overlap, not a count of outstanding hardware misses.
-->

---

# Effective load cost grows with footprint

<img class="chart" src="/figures/pointer-chase.svg" alt="Measured dependent pointer-chain nanoseconds per dereference versus allocation footprint, with median, minimum and maximum across seven repetitions." />

<div class="source">Horizontal axis: logarithmic (equal steps multiply the footprint). Lines: medians; bands: minimum–maximum of seven repetitions. Warm traversals; x86 WSL2 guest. Source: benchmarks/results/raw.csv.</div>

<!--
Read the log footprint axis. At 2 MiB there is a large outlier; show the retained range rather than hiding it. Knees are consistent with hierarchy effects but are not proof of exact cache boundaries; translation, host activity, and frequency also change the result.
-->

---
class: exercise
---

# Practice: count useful bytes and fetched lines

A cold pass reads one 8-byte field from each of **one million 64-byte records**. Assume one 64-byte line per record at the boundary being modeled.

1. How many useful bytes does the pass read?
2. How many bytes of lines does that model fetch?
3. What representation change could reduce the second number?


<v-click>
<div class="callout">**8 MB** useful; about **64 MB** of fetched lines. A dense hot-field array can approach **8 MB** of line traffic for the pass.</div>
</v-click>

<!--
Use decimal MB because the question uses one million, not 2^20. This is modeled line traffic at a chosen boundary, not a measurement of DRAM bandwidth. Tie back to the AoS/SoA result.
-->

---

# The CPU also needs the address correspondence

We have followed the **data**. Now return to virtual address **0x10034**.

The OS page table says **page 16 → frame 42**. Looking up that answer can itself require memory accesses.

A **translation lookaside buffer (TLB)** is a CPU cache of recent page translations and their access attributes.

| Cache | Key | Cached information |
|---|---|---|
| Data cache | A memory line's address | The data bytes |
| TLB | A virtual page in an address space | Physical frame and access attributes |

<div class="callout">Caching a translation and caching the translated data are different jobs.</div>

<div class="source"><a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html">Intel architecture manuals</a> · <a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel64-and-ia32-architectures-optimization.html">optimization manuals</a></div>

<!--
This is the first use of the TLB acronym in the main teaching path. A process/context identifier can also be part of the lookup; the appendix covers those tags. Do not imply one universal TLB structure.
-->

---

# Trace a TLB hit with our address

Virtual address **0x10034 = page 16 + offset 0x34**.

| Recent translations in this teaching TLB | Frame | Access |
|---|---|---|
| Page 16 | **42** | Read/write |
| Page 17 | 7 | Read/write |

<div class="flow"><b>Look up<br>virtual page 16</b> → <b>Hit: frame 42<br>base 0x2A000</b> → <b>Add offset 0x34<br>physical 0x2A034</b></div>

Then the CPU still needs the requested **data bytes**, which may hit or miss in its data caches.

<!--
Connect directly to the hand translation in session 1. The TLB makes the same correspondence available quickly. The diagram is conceptual; real L1 lookup can overlap translation rather than waiting for every step serially.
-->

---

# A TLB miss can start a hardware page walk

A **page walk** looks through the page-table structure to find a translation.

Toy example: split a six-bit page number into two three-bit indices.

**Page 16 = binary 010 000** → first index **2**, second index **0**.

<div class="flow"><b>Root table<br>entry [2]</b> → <b>Next table<br>entry [0]</b> → <b>Frame 42<br>with read/write access</b></div>

On x86, **hardware** performs the walk. A valid present entry can supply the translation without entering the OS.

<div class="note">Table reads can hit CPU caches. The appendix maps this two-level toy model to real x86-64 tables.</div>

<div class="source"><a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html">Intel architecture manuals</a> · <a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel64-and-ia32-architectures-optimization.html">optimization manuals</a></div>

<!--
Six page-number bits and two levels are explicitly a teaching model. The offset stays separate. In a real walk the root is selected by the active address-space context. If no usable entry exists, a page fault can enter the kernel.
-->

---

# Three misses, three different mechanisms

| Event | Missing information | Usually resolved by |
|---|---|---|
| **Data-cache miss** | Requested cache line | Cache hierarchy / main memory |
| **TLB miss** | Cached address translation | Hardware page-table walk on x86 |
| **Page fault** | Usable mapping or permission | Kernel exception handler |

A TLB miss can find a present page and complete **without entering the OS**.

A data-cache miss can happen with the translation already cached in the TLB.

<div class="source"><a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html">Intel SDM, volume 3: paging</a></div>

<!--
Some ISAs use software-managed TLBs. This lecture's hardware-walk statements are scoped to x86 and similar hardware-managed systems. Faults and page walks can generate additional cache traffic.
-->

---
class: exercise
---

# Try it: which mechanism is needed?

Assume x86 hardware-managed translation.

| Situation | What needs to happen? |
|---|---|
| Translation is in the TLB; data line is absent from L1 | ? |
| Translation is absent from all TLB levels; page-table entry is present | ? |
| A legal first write finds no private writable backing | ? |


<v-click>
<div class="callout">**Fetch the data line** through the cache hierarchy; **walk the page table**; **enter the kernel for fault handling**, respectively. These events can also occur together.</div>
</v-click>

<!--
Ask who does the work: cache/memory hardware, page-walk hardware, or OS. This follows all definitions, unlike the original early comparison.
-->

---

# TLB reach: how much data can entries cover?

**TLB reach** is the amount of memory covered by its cached translations.

Teaching TLB: **64 entries for 4 KiB pages**.

**64 × 4 KiB = 256 KiB** of nominal coverage.

Our 8 MiB buffer spans **2,048 base pages** if page-aligned. A random access pattern can need far more translations than this small TLB holds.

<div class="callout">Dense objects do more useful work per translated page. One tiny object on each page can waste translation capacity.</div>

<!--
Nominal reach is not a universal performance boundary. Real CPUs have several translation structures, conflicts, and page-size-specific entries. Do not label the 64-entry teaching TLB as the measured CPU specification.
-->

---

# Huge pages: fewer translations for the same bytes

A **huge page** is a supported mapping larger than the base page size.

For a suitably aligned **2 MiB** region on a supporting x86 system:

| Base pages | One huge page |
|---|---|
| 512 × 4 KiB mappings | One 2 MiB mapping |
| Potentially 512 base-page translations | One large-page translation |

This can reduce translation work. It also changes allocation granularity and can waste storage for sparse use.

<div class="note">Alignment alone does not create huge mappings. Linux mechanisms and how to verify them are in the appendix.</div>

<div class="source"><a href="https://docs.kernel.org/admin-guide/mm/concepts.html">Linux memory concepts</a> · <a href="https://docs.kernel.org/mm/page_tables.html">page tables</a></div>

<!--
The operating system needs suitable physical backing and a large-page entry. Actual TLB capacity differs by supported page size. Keep Linux THP names, compaction, and NUMA tradeoffs until their prerequisites are taught.
-->

---

# Session 2 · Follow the initialized buffer

| Layer | What we learned to track |
|---|---|
| OS pages | Our aligned 8 MiB buffer spans 2,048 × 4 KiB pages |
| Translation cache | Recent page-to-frame correspondences in the TLB |
| Data caches | With 64 B lines, the buffer spans 131,072 lines |
| Request dependencies | Which next addresses can be known before earlier loads finish? |

Explain why **resident data** can still be slow: the needed translation or line can be absent, and dependent loads can serialize waiting.

<div class="callout">Next: what changes when multiple cores read or modify these lines?</div>

<!--
Stop for session 2. Ask for a complete ordinary-load trace using the same virtual address, plus a TLB-miss variation. A separate cache line per small object wastes both line and translation capacity.
-->

---
class: section
---

# Session 3 · Design data and communication

We now know where bytes live and how one core fetches them.

Next: **which core may modify them**, **where pages are allocated**, and **which C++ changes remove work**.

<!--
Recap pages, cache lines, and independent requests. Multiple workers add sharing and placement, not a new kind of C++ address.
-->

---

# Several cores can cache the same line

**Cache coherence** is the hardware protocol that keeps cached copies of a memory location consistent with writes.

**Line ownership** means permission to modify a cache line. It differs from C++ ownership of an object's lifetime.

<div class="grid2">
<div class="panel"><h2>Core 0</h2>Cached line: value 7<br>May read shared copy.</div>
<div class="panel"><h2>Core 1</h2>Cached line: value 7<br>May read shared copy.</div>
</div>

Before core 0 writes, the protocol must arrange suitable write permission and prevent core 1 from using a stale copy.

<div class="source"><a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html">Intel architecture manuals</a> · <a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel64-and-ia32-architectures-optimization.html">optimization manuals</a></div>

<!--
This is a simplified coherent ordinary-memory model. ISA ordering across different locations and C++ synchronization are separate topics. Avoid the MESI state alphabet until the behavior is understood.
-->

---

# Watch a write change the cached copies

| Step | Core 0 | Core 1 |
|---|---|---|
| Both read | Shared copy: **7** | Shared copy: **7** |
| Core 0 obtains write permission | Can modify the line | Old copy becomes **invalid**: unusable |
| Core 0 writes **9** | Holds changed line: **9** | Must obtain a current copy before reading |

A changed line is **dirty** relative to the next memory level. **Write-back** transfers its changes later, such as on eviction.

An ordinary cold store may fetch the line while requesting write permission: **read-for-ownership (RFO)**.

<div class="note">Ownership and data can come from another cache. A write need not immediately reach DRAM.</div>

<div class="source"><a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html">Intel architecture manuals</a> · <a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel64-and-ia32-architectures-optimization.html">optimization manuals</a></div>

<!--
Read-only sharing has no repeated write ownership transfer. The simplified table leaves protocol transient states to the appendix. Coherence does not by itself make a racing C++ program correct.
-->

---

# An atomic update is still an update to shared memory

A **data race** occurs when unsynchronized threads access the same memory location, at least one writes, and the accesses are not suitably atomic. It is undefined behavior in C++.

An **atomic** operation performs its update indivisibly:

```cpp
std::atomic<std::uint64_t> count{0};
count.fetch_add(1, std::memory_order_relaxed);
```

`relaxed` preserves the atomic counter operation but supplies no ordering for unrelated data.

<div class="callout">Correct atomic increments still need the cache line's write permission. Contention can make them costly.</div>

<div class="source"><a href="https://eel.is/c++draft/intro.races">C++ data races</a> · <a href="https://eel.is/c++draft/atomics.order">atomic ordering</a></div>

<!--
Include atomic and cstdint. This is a definition sufficient for the counter example; publication ordering is developed in the appendix. Atomic does not always mean lock-free. Plain per-worker data can be correct when ownership and joining establish synchronization.
-->

---

# False sharing: independent variables, shared line

```cpp
struct Counters {
  std::atomic<std::uint64_t> a{0}; // worker 0 writes
  std::atomic<std::uint64_t> b{0}; // worker 1 writes
};
```

<div class="cache-line"><span class="hot">a</span><span class="hot">b</span><span>…</span><span>…</span><span>…</span><span>…</span><span>…</span><span>…</span></div>

The variables are logically separate, but modifications can bounce ownership of one physical line between cores.

<div class="note">Concurrent readers alone do not create write-induced false sharing.</div>
<div class="source"><a href="https://www.intel.com/content/www/us/en/content-details/671363/tutorial-identifying-false-sharing-c-sample-application-linux.html">Intel: identifying false sharing</a></div>

<!--
Exact placement is ABI-dependent; verify addresses and layout. Atomics remove the data race on each counter but do not remove line contention. Plain per-worker fields can also false-share when they are actually stored frequently.
-->

---

# Trace the ownership bouncing between writers

Assume counters `a` and `b` share one physical line; writes are actually performed frequently.

| Operation | Core with write permission afterward | Other core's cached copy |
|---|---|---|
| Worker 0 updates `a` | Core 0 | Invalid |
| Worker 1 updates `b` | Core 1 | Invalid |
| Worker 0 updates `a` again | Core 0 | Invalid |

<div class="flow"><b>Core 0<br>writes a</b> → <b>Line permission<br>and data transfer</b> → <b>Core 1<br>writes b</b></div>

**False sharing** means the hardware communicates because of line placement even though the variables are logically separate.

<!--
The exact protocol and physical data movement differ by CPU. Keep it conceptual rather than claiming every transfer goes through RAM. This is an unmeasured trace, not a benchmark. Compare with true sharing where both threads intentionally update the same counter.
-->

---
class: compact
---

# Separate writers, then reduce communication

`alignas(64)` requests a 64-byte boundary. On this example target, separate counter objects can occupy separate lines.

```cpp
struct alignas(64) Counter {  // example target's line size
  std::atomic<std::uint64_t> value{0};
};
Counter per_worker[worker_count];
```

- Independent counters can now occupy different lines.
- Better still: accumulate locally, publish in batches, reduce later.
- A frequently read aggregate can still contend with its writers.
- Padding trades memory footprint for reduced ownership interference.

<div class="note">Verify the target, ABI, actual store frequency, and access pattern. No false-sharing benchmark result is claimed here.</div>

<!--
worker_count is a compile-time count in the illustrative array declaration. Atomic counter batching must preserve the application's required freshness and overflow semantics. A non-atomic local result read only after joining the worker is often sufficient.
-->

---

# Partition by ownership and by locality

- Give each worker a contiguous range of output and scratch space.
- Keep frequent writes to the same line on one worker when possible.
- Give each worker a large contiguous **chunk** (range), rather than alternating adjacent elements between writers.
- Separate queue metadata that has different writers.
- Choose chunk sizes that amortize scheduling and boundary sharing.

<div class="callout">Ownership is a performance property as well as a correctness property.</div>

<div class="note">Load balance can favor dynamic scheduling. Compare its benefits with locality and communication costs.</div>

<!--
One boundary line per pair of large chunks is different from false sharing on every element. Producer/consumer queues still have true sharing; padding alone cannot remove required communication. Avoid prescribing a fixed chunk size for all workloads.
-->

---

# NUMA: the same address can be near or far

**Non-uniform memory access (NUMA)** means physical memory can have different access costs depending on which CPU uses it.

A **node** groups a set of CPUs and/or memory with a particular locality relationship.

<div class="grid2">
<div class="panel"><h2>Node 0</h2>Worker 0 + attached DRAM<br><strong>Local</strong> to worker 0.</div>
<div class="panel"><h2>Node 1</h2>Worker 1 + attached DRAM<br><strong>Remote</strong> to worker 0.</div>
</div>

Remote access crosses an **interconnect**, the communication fabric between nodes. It can have different latency and bandwidth.

<div class="note">The pointer remains a virtual address. Physical page placement determines which node supplies the memory.</div>

<div class="source"><a href="https://docs.kernel.org/admin-guide/mm/numa_memory_policy.html">Linux NUMA memory policy</a></div>

<!--
A NUMA node is not necessarily a socket, and some nodes have only memory. The simple two-node model is for teaching. The included VM reports one node, so there are no local/remote benchmark numbers.
-->

---

# Rebuild the buffer: who performs the first writes?

**First touch** means the access that causes actual physical allocation. Under a local allocation policy, new pages tend to be allocated near the CPU doing that work.

Fresh page-aligned buffer; two equal partitions; workers stay on their respective nodes:

| Initialization | First half's frames | Second half's frames |
|---|---|---|
| Worker 0 writes the entire buffer | Node 0 | Node 0 |
| Each worker writes its future half | Node 0 | Node 1 |

<div class="callout">Match initialization to later ownership. Writing existing resident pages again does not itself guarantee their relocation.</div>

<div class="note">This example assumes local allocation, available memory, and no automatic page migration. Real policy and pressure can change placement.</div>

<div class="source"><a href="https://docs.kernel.org/admin-guide/mm/numa_memory_policy.html">Linux NUMA placement</a></div>

<!--
A zero-page read is not the private allocation meant here. Pinning means fixing a worker to chosen CPUs. The table starts with fresh backing, not the previously initialized buffer magically changing nodes. Reused allocator pages can retain old placement.
-->

---
class: exercise
---

# Try it: where are the second worker’s pages?

Use the previous two-node model with a fixed local allocation policy and no page migration.

Worker 0 first-writes every page. Worker 1 then repeatedly processes the second half.

1. Where were the second half's frames allocated?
2. Does worker 1 merely rewriting the values necessarily move those frames?
3. What initialization change would test the placement explanation?


<v-click>
<div class="callout">**Node 0; no.** Repeat with fresh backing and let worker 1 initialize its own half while running on node 1. Check actual page placement before comparing timing.</div>
</v-click>

<!--
The checkpoint now follows the full definition and worked example. Do not infer NUMA effects from the included single-node recording. Binding tools and automatic balancing are appendix details.
-->

---

# Design hot data around actual accesses

- Split rarely read metadata away from frequently scanned fields.
- Use smaller integer widths when the valid range permits them.
- Group fields used together; avoid fetching unrelated payloads.
- Replace per-element pointers with compact indices when suitable.
- Measure size, alignment, hot-set footprint, and conversion cost.

```cpp
static_assert(sizeof(HotRecord) == expected_size);
```

<div class="callout">A representation change can reduce cache misses, translation work, allocation overhead, and serialization traffic together.</div>

<!--
expected_size and HotRecord are placeholders for an application's layout assertions. Packing may force unaligned accesses and break ABI expectations. Compact indices have capacity limits and lifetime/identity semantics to design explicitly.
-->

---

# Choose a layout for the operations you actually run

| Layout | Usually convenient for | Main tradeoff |
|---|---|---|
| Array of structures | Whole-object operations | Cold fields travel with hot fields |
| Structure of arrays | Field-wise scans and SIMD | Multiple arrays and coordinated updates |
| Array of structures of arrays (AoSoA): small SoA blocks | Bounded working sets and vector batches | Block size and tail handling |

Choose based on the hottest operations **and** update patterns.

Keep invariants, ownership, and API usability visible in the design.

<!--
Avoid claiming that SoA always vectorizes or that AoS never does. Compiler gather/shuffle support, field count, SIMD width, and branch behavior matter. Consider mixed scans, random lookup, and serialization.
-->

---

# Replace a chain of dependent lookups when useful

| Current representation | Candidate change | Why test it? |
|---|---|---|
| One allocation and pointer per record | Contiguous records | More useful records per line |
| Pointers linking records | Compact integer indices into arrays | Smaller links, if the range fits |
| Queries handled one at a time | Several independent queries in a batch | More requests available to overlap |

An **index** here is an array element number; it differs from a cache's set index.

<div class="note">Preserve required object identity, update behavior, iterator stability, and latency. The representation is chosen for an operation mix.</div>

<!--
Use the pointer-chain timing to motivate these candidates. A narrower index needs a capacity bound and an invalid-entry convention. Contiguous containers can move elements on growth; pointer-stability requirements are real. Advanced trees/hash tables are optional further reading.
-->

---
class: compact
---

# Loop transformations change reuse distance

**Loop fusion** combines passes so values can be reused immediately. **Aliasing** means that access paths can refer to overlapping storage.

```cpp
// Separate passes: temporary arrays may be evicted before reuse.
for (std::size_t i = 0; i < n; ++i) tmp[i] = f(input[i]);
for (std::size_t i = 0; i < n; ++i) out[i] = g(tmp[i]);

// When semantics permit, fuse and remove the intermediate array.
for (std::size_t i = 0; i < n; ++i) out[i] = g(f(input[i]));
```

- Interchange loops to favor contiguous access.
- Block work so reused data stays near the core.
- Fusion can reduce traffic but require more simultaneously live values in registers or hinder SIMD vectorization.

<div class="note">Check aliasing, side effects, floating-point rules, and loop-carried dependencies.</div>

<!--
Assume f and g are appropriate pure operations for the transformation. Removing a stored temporary can change floating-point rounding in some environments. Multiple consumers and parallel scheduling can favor retaining the temporary.
-->

---

# Allocators reuse blocks inside mapped pages

An allocator can retain OS memory and divide it into application-sized blocks.

- A **size class** groups similar allocation sizes; a 33 B request might use a 48 B block in an illustrative allocator.
- An **arena** is a pool of storage managed together.
- A **thread cache** keeps reusable blocks near one allocating thread.

<div class="flow"><b>OS supplies pages</b> → <b>Allocator divides<br>and retains blocks</b> → <b>C++ objects use<br>some blocks</b></div>

Freeing one object can return its block to the allocator without returning the whole page to the OS.

<div class="source"><a href="https://man7.org/linux/man-pages/man3/malloc.3.html">malloc</a></div>

<!--
The size-class numbers are explicitly illustrative, not glibc documentation. User-space arenas are not Linux slab caches. Kernel page/block allocation is optional appendix material.
-->

---

# Why live bytes and resident bytes can diverge

Illustrative page split into eight equal allocator blocks:

<div class="cache-line"><span class="hot">live</span><span>free</span><span>free</span><span>free</span><span>free</span><span>free</span><span>free</span><span>free</span></div>

The remaining object can keep the page in use even though most blocks are free.

- **Internal fragmentation:** unused space inside allocated blocks, such as size-class rounding.
- **External fragmentation:** free storage exists but its arrangement prevents the desired reuse or release.
- **Retained storage:** memory the allocator deliberately holds for future allocations.

<div class="note">This drawing shows allocator blocks within a page, not eight CPU cache-line fields.</div>

<!--
The colored live block is an allocator object, not a hot field. Fragmentation and deliberate retention are distinct reasons RSS may stay high after free. Track live objects, allocator-held storage, and OS residency separately.
-->

---

# Allocate a phase together, release it together

If all temporary objects live until the end of one phase, an arena can serve many small allocations from a larger buffer.

A **monotonic** resource advances through storage as allocations arrive. Individual deallocations do not recover that storage; the resource releases it together.

<div class="flow"><b>Reserve a pool</b> → <b>Build temporary<br>objects</b> → <b>Process them</b> → <b>Destroy users<br>then release pool</b></div>

**PMR** means C++ **polymorphic memory resource**: containers can obtain storage from a selected resource object.

<div class="note">The pool's lifetime must outlast every object using its storage. Retaining the pool also retains its footprint.</div>

<div class="source"><a href="https://eel.is/c++draft/mem.res.monotonic.buffer">C++ monotonic resources</a></div>

<!--
This establishes the lifetime diagram and the names before the API example. Polymorphic means the resource interface can select a concrete allocation strategy at runtime. A monotonic resource is not itself thread-safe.
-->

---
class: compact
---

# Make allocation follow object lifetime

```cpp
std::array<std::byte, 64 * 1024> storage;
std::pmr::monotonic_buffer_resource arena(storage.data(), storage.size());
{
  std::pmr::vector<Item> items{&arena};
  items.reserve(expected_count);
  // Build and process this phase's items.
}                          // destroy users before releasing storage
arena.release();
```

- One phase can amortize many small allocations.
- Monotonic resources reclaim storage together, not per object.
- The default upstream resource can allocate after the buffer is exhausted.

<div class="source"><a href="https://eel.is/c++draft/mem.res.monotonic.buffer">C++ draft: monotonic_buffer_resource</a></div>

<!--
Include array, cstddef, memory_resource, and vector. Item and expected_count are application-specific. An upstream null_memory_resource can make exhaustion explicit. A monotonic resource is not thread-safe. It can retain abandoned vector growth buffers until release. Resource lifetime must cover every container and object using its storage.
-->

---

# Containers can accidentally add memory work

- `vector::reserve` changes capacity; it does not construct elements or change size.
- `vector<T>(n)` constructs `n` elements; for integers it initializes values.
- Repeated growth moves/copies elements and can leave old arena blocks retained.
- `clear()` destroys elements; capacity usually remains available.
- Node containers add allocation and pointer-chasing costs, alongside their useful guarantees.

<div class="callout">Benchmark the same lifecycle and the same required initialization in both implementations.</div>

<div class="source"><a href="https://eel.is/c++draft/vector.capacity">C++ draft: vector capacity</a> · <a href="https://eel.is/c++draft/vector.cons">vector constructors</a></div>

<!--
Writing v[i] after reserve without resize is invalid, even if capacity is sufficient. reserve itself is not a guarantee of resident writable pages. Avoid duplicate zeroing if it is unnecessary, but never read uninitialized values.
-->

---

# Alignment: a required address boundary

A type's **alignment** restricts where its objects may start. An alignment of 8 requires an address divisible by 8.

| Illustrative address | Divisible by 8? |
|---|---|
| 0x1000 | Yes |
| 0x1003 | No |
| 0x1008 | Yes |

`alignas(64)` asks for a 64-byte boundary, which can help separate independent writers' objects on a 64-byte-line target.

<div class="callout">Padding changes footprint. Giving every small object a whole line can waste the locality we worked to improve.</div>

<!--
Required alignment is a correctness property, distinct from optional performance alignment. An x86 unaligned instruction does not make a misaligned C++ typed object legal. Over-aligned types also affect array stride.
-->

---

# Fast code still obeys the C++ object model

| Concern | Required discipline |
|---|---|
| Lifetime | Storage and a live object are different requirements |
| Alignment | Meet the type's alignment; an alignment promise such as assume_aligned must be true |
| Aliasing | Access through permitted types; use `memcpy` / `bit_cast` appropriately |
| Bounds | A plausible mapped address does not make out-of-bounds access legal |
| Concurrency | Establish synchronization; cache coherence does not remove data races |

<div class="note">GCC/Clang `__restrict__` is an extension with a real no-alias contract. A false promise can miscompile the program.</div>
<div class="source"><a href="https://eel.is/c++draft/basic.life">Lifetime</a> · <a href="https://eel.is/c++draft/basic.lval">Aliasing</a> · <a href="https://gcc.gnu.org/onlinedocs/gcc/Restricted-Pointers.html">GCC restrict</a></div>

<!--
A cast alone does not generally create arbitrary objects in storage. Modern implicit-lifetime rules have specific scopes. std::bit_cast requires same size and trivially copyable types; copying representation does not turn every bit pattern into a valid value. Unaligned x86 instructions do not legalize a misaligned C++ typed pointer.
-->

---
class: compact
---

# Software prefetch needs enough lead time

**Prefetch** means requesting data before the instruction that will need it. Hardware can recognize some regular streams; software can also provide hints.

```cpp
for (std::size_t i = 0; i < n; ++i) {
  if (distance < n - i)
    __builtin_prefetch(&items[i + distance], 0, 1);
  consume(items[i]);
}
```

- Hardware already handles many regular streams.
- Prefetching needs an address known before its demand use.
- Too early: eviction; too late: little help; too much: wasted traffic.
- A dependent linked list may not reveal the next useful address early enough.

<div class="source"><a href="https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html">GCC: __builtin_prefetch</a> · benchmark the distance and the target CPU.</div>

<!--
Assume i < n, a valid items range, and size_t distance. The guard avoids overflow and out-of-range pointer formation. A prefetch hint does not make evaluating an invalid address expression legal. It is neither synchronization nor a guarantee of cache residency.
-->

---
class: compact
---

# Roofline: estimate whether fewer FLOPs can help

A **FLOP** is one floating-point operation. **Arithmetic intensity** is useful arithmetic operations divided by transferred bytes at a stated memory boundary.

```cpp
z[i] = a * x[i] + y[i];  // double arrays: 2 FLOPs per element
```

Logical traffic: 8 B + 8 B read + 8 B write = **24 B / element**.

**Arithmetic intensity = 2 / 24 ≈ 0.083 FLOP/B**.

With **60 GB/s at that boundary**, the model limits throughput to **5 GFLOP/s**.

<div class="note">Cold write-allocate output can add ~8 B/element of RFO: 32 B total, ~3.75 GFLOP/s at 60 GB/s. Reuse and store policy change the model.</div>
<div class="source"><a href="https://www2.eecs.berkeley.edu/Pubs/TechRpts/2008/EECS-2008-134.pdf">Williams, Waterman, Patterson: Roofline (original report)</a></div>

<!--
All values here are a teaching model, not a measurement. FLOP/s ≤ min(compute peak, bandwidth × arithmetic intensity), with bytes and bandwidth measured at the same hierarchy boundary. a is reused in a register. Count an FMA as two FLOPs.
-->

---

# What objective and evidence are we comparing?

- **Throughput:** useful operations completed per second.
- **p99 latency:** a time threshold within which 99% of observed requests complete.
- **Profile:** measurements locating where time or events are spent.
- **PMU:** performance monitoring unit, hardware that counts supported CPU events.

A PMU can count selected cache or translation events. The OS can count faults and resident memory. Timing establishes whether the workload improved.

<div class="note">The included recordings contain timings and OS minor-fault counts. They contain no hardware-counter evidence.</div>

<!--
Distinguish software fault counts from hardware counters. Event meanings depend on CPU and kernel. PMU sampling and whole-process counting are appendix topics. Keep objectives explicit: a throughput win can still worsen a latency tail.
-->

---

# Match the optimization to the evidence

| Evidence to investigate | Candidate intervention |
|---|---|
| Dependent misses; low bandwidth | Batch queries, compact links, expose independent work |
| Saturated relevant bandwidth | Reduce bytes, reuse, compress, fuse |
| Many expensive translation walks | Dense layout, fewer pages, huge-page experiment |
| Faults / reclaim in request path | Prepare working set, bound footprint, inspect policy |
| Ownership transfers between cores | Partition, shard, batch, isolate writes |
| Poor NUMA placement | Align initialization, execution, and memory policy |

<div class="note">Verify that the candidate changes the suspected mechanism and improves the real workload.</div>

<!--
No row is an automatic diagnosis. Several limits can interact; an optimization can move the bottleneck. Throughput, p99 latency, and footprint may choose different tradeoffs.
-->

---

# A benchmark must preserve the question

- Use realistic sizes, distributions, lifetimes, and thread counts.
- Validate results; prevent dead-code elimination; inspect optimized code.
- Separate cold backing, cold CPU caches, and warm steady-state tests.
- Retain every repetition; randomize order for close comparisons.
- Record compiler, flags, CPU, kernel, page policy, affinity, and input.

<div class="callout">Report payload bytes/s, line traffic, and DRAM bytes/s with distinct definitions. They are not interchangeable.</div>

<!--
This harness uses fixed case order and short repetitions; it is educational evidence, not a universal ranking. Close comparisons need longer runs, order randomization, and host control. Cache eviction buffers and clflush have their own measurement effects; don't call an arbitrary run “cold.”
-->

---

# A productive optimization sequence

1. Define the objective: throughput, latency distribution, footprint, or cost.
2. Find the hot path and representative operating conditions.
3. Estimate useful bytes, touched lines/pages, dependencies, and ownership.
4. Collect evidence at the suspected bottleneck.
5. Change one representation, access pattern, or policy.
6. Recheck correctness and the end-to-end objective.

<div class="note">Prefer changes that remove work or bytes before adding target-specific hints.</div>

<!--
Keep the baseline and document tradeoffs. An improved microbenchmark can regress real workloads due to memory growth, setup cost, cold-start behavior, or a different operation mix.
-->

---
class: exercise
---

# Put the layers together: explain a slow pass

Our initialized buffer is resident. A random pointer-linked traversal gets slower as its footprint grows, while measured total bytes per second stay low.

1. Which kind of dependency can prevent requests from overlapping?
2. Could data-cache misses and translation work both contribute?
3. Which comparison would help test the overlap explanation?


<v-click>
<div class="callout">Each load can reveal the next address, serializing requests. Both data and translation misses can contribute. Compare several independent streams with equal total work, then seek supported counter evidence to separate causes.</div>
</v-click>

<!--
This is guided causal reasoning, not a proof that the included curve isolates one cache level. Ask for a competing explanation, such as host interference or changed compiler output, and a discriminating next experiment.
-->

---
class: exercise
---

# Case study: p99 regressed, bandwidth stayed low

A service's pointer-rich index doubled. CPU time increased, aggregate memory bandwidth stayed low, and request tails grew.

1. What would distinguish dependent cache misses from translation walks?
2. What would distinguish either from faults or reclaim?
3. Which representation or batching change would test your model?
4. Why might more request threads help initially and then hurt?

<v-click>
<div class="callout">Inspect the hot path, supported miss/walk events, faults, pressure, and scaling. Test compact layout or batched independent lookups with the same requests.</div>
</v-click>

<!--
No single symptom proves the cause. Larger index can stress both data and translations. Increased concurrency can hide latency until bandwidth, cache capacity, or queueing becomes limiting.
-->

---
class: exercise
---

# Five claims to challenge in a design review

1. “The TLB missed, so the kernel loaded a page from disk.”
2. “We use relaxed atomics, so the shared counter is cheap.”
3. “The allocation succeeded, so the memory is ready.”
4. “The data fits in L3, so memory cannot be the bottleneck.”
5. “Huge pages fixed it, so all allocations should use them.”

<v-click>
<div class="callout">Separate translation, backing, residency, cache traffic, dependencies, and ownership. Each claim needs a workload-specific measurement.</div>
</v-click>

<!--
Have students correct one claim each. Fitting in L3 still leaves LLC latency, bandwidth, conflicts, sharing, TLBs, and dependencies; it also ignores other users of the LLC.
-->

---

# Session 3 · Explain the buffer end to end

<div class="flow"><b>Legal C++<br>objects</b> → <b>Mapped pages<br>resident frames</b> → <b>Cached translations<br>and data lines</b> → <b>Overlapping requests<br>and clear writers</b></div>

For the same buffer, explain:

- **First write:** why the OS may need to supply a private frame.
- **Later scan:** why useful bytes per line and working set matter.
- **Random walk:** why the next address can force waiting.
- **Parallel phase:** why shared lines and physical page placement matter.

<div class="callout">Choose a change by naming the work or communication it removes, then measure the real objective.</div>

<!--
This is the final core summary. Ask learners to narrate an ordinary load, a faulting write, and a contended shared-line update. The appendix is optional depth; its topics should not be treated as prerequisites for these explanations.
-->

---
class: exercise
---

# Optional lab · one question per group

Use the included `benchmarks/README.md` and kernel checks.

1. Pointer chains: sweep footprint; compare one versus eight streams.
2. Hot fields: change record size while preserving the useful values.
3. Transpose: compare dimensions and tile sizes, including incomplete tiles.
4. First writes: compare fresh and resident passes and their fault counts.

Each group reports **prediction → result → explanation → competing explanation → next test**.

<div class="note">Choose one investigation for a 45–60 minute lab. Verified huge-page and multi-node experiments are advanced extensions.</div>

<!--
The previous six-task lab was too broad for one hour. A group chooses one family, validates its result, and reports the protocol and uncertainty. Teach API/tool setup separately if needed.
-->

---
class: section
---

# Appendix · Implementation details and further experiments

Choose these after the three core sessions.

1. Linux mappings, pressure, and page allocators.
2. DRAM organization, cache policies, and quantitative limits.
3. Real x86 page tables and advanced translation behavior.
4. C++ ordering, platform-specific optimizations, and profiling tools.

<div class="note">The teaching guide gives direct slide numbers. Vocabulary below builds on the core models.</div>

<!--
This is an explicit stopping and selection point, not another required session. Preserve the expert-level material while letting instructors decide which operational and architecture details suit their audience.
-->

---

# Read Drepper for mechanisms

Ulrich Drepper, **What Every Programmer Should Know About Memory** (2007).

- Focus: access patterns, working sets, translation, sharing, NUMA, measurement.
- Read §§2–5 for the machine model; §6 for optimization ideas; §7 for tools.
- Revisit its experiments with today's compiler and your actual CPU.
- Replace historical bus diagrams, cache assumptions, and fixed timings with target documentation.

<div class="callout">Keep the reasoning; remeasure the constants.</div>

<div class="source"><a href="https://www.akkadia.org/drepper/cpumemory.pdf">Drepper, original article</a> · this lecture uses original diagrams, code, and measurements.</div>

<!--
The article is foundational, not a current hardware specification. Do not generalize inclusive caches, northbridges, prefetch behavior, or the old figures to every modern processor.
-->

---

# The process map is a set of regions

| Typical region | Purpose |
|---|---|
| Executable and libraries | File-backed code and data; different permissions |
| Heap and anonymous mappings | Allocator storage, large buffers |
| Thread stacks | Automatic objects, frames, guard regions |
| File mappings / shared memory | Application-defined backing and sharing |

```sh
cat /proc/$$/maps       # inspect the current shell's address space
```

<div class="note">Address space layout randomization (ASLR), the program loader, allocators, and thread libraries determine the actual layout.</div>
<div class="source"><a href="https://man7.org/linux/man-pages/man5/proc_pid_maps.5.html">Linux: /proc/pid/maps</a></div>

<!--
This is a taxonomy, not an address-ordered diagram. Stack versus heap describes storage organization, not a separate kind of RAM. The shell expands $$ to its process ID.
-->

---

# What backs the bytes?

| Mapping | Read source | What writes do |
|---|---|---|
| Private anonymous | Initially zeros | Create/modify private memory |
| Private file | File-backed pages | Create private COW copies |
| Shared file | Shared file-backed pages | Dirty shared pages for writeback |
| Shared anonymous / shared-memory object | Shared memory object | Modify shared backing |

`MAP_PRIVATE` modifications do not update the mapped file.

`MAP_SHARED` visibility, synchronization, and durable storage are separate concerns.

<div class="source"><a href="https://www.man7.org/linux/man-pages/man2/mmap.2.html">mmap(2)</a> · <a href="https://man7.org/linux/man-pages/man2/msync.2.html">msync(2)</a></div>

<!--
A mapping does not make concurrent non-atomic C++ accesses safe. Durable updates require the appropriate persistence API/protocol, e.g. msync/fsync and filesystem guarantees. MAP_PRIVATE does not guarantee a snapshot against external file changes.
-->

---

# The page cache and CPU caches solve different problems

<div class="flow"><b>Storage<br>file contents</b> ↔ <b>OS page cache<br>resident RAM pages</b> ↔ <b>CPU caches<br>recent cache lines</b></div>

- Ordinary buffered `read` typically copies from page-cache memory.
- Ordinary file `mmap` can expose that backing through process mappings.
- A warm page cache avoids storage I/O; CPU cache misses can still dominate.
- Memory mapping changes the access interface and fault path; it does not remove all copying or all I/O costs.

<div class="source"><a href="https://docs.kernel.org/mm/page_cache.html">Linux: page cache</a> · <a href="https://www.man7.org/linux/man-pages/man2/mmap.2.html">mmap(2)</a></div>

<!--
Scope to normal buffered files. Direct I/O and DAX have different paths. A mapped write can still trigger COW or allocation. Compare read and mmap with realistic sizes, access order, concurrency, and cold/warm backing.
-->

---

# Pressure controls: promises and limits

- **Overcommit** policy controls which memory commitments Linux admits.
- A **control group (cgroup)** groups processes for accounting and resource limits.
- **OOM** means out of memory: exhaustion can trigger a process termination.
- **Pressure stall information (PSI)** records time tasks are stalled by resource pressure.
- **Direct reclaim** makes the allocating thread help recover pages before it can continue.

Within a workload's cgroup, `memory.high` can induce reclaim and throttling; `memory.max` imposes a hard limit.

<div class="note">A container can reach its own limit while the host still has free RAM. A successful allocation alone does not prove every later fault will succeed.</div>

<div class="source"><a href="https://cdn.kernel.org/doc/html/latest/mm/overcommit-accounting.html">Overcommit</a> · <a href="https://docs.kernel.org/admin-guide/cgroup-v2.html">cgroup v2</a> · <a href="https://docs.kernel.org/accounting/psi.html">PSI</a></div>

<!--
Throttling delays work to keep usage within policy. Inspect memory.current, memory.events, memory.stat, and memory.pressure in the actual workload cgroup. Overcommit modes and OOM selection are kernel-policy details, not C++ exception guarantees.
-->

---

# Kernel allocation: pages versus small kernel objects

Linux's **buddy allocator** manages free physical-page blocks in power-of-two sizes.

<div class="flow"><b>8 free pages</b> → <b>Split: 4 + 4</b> → <b>Split one half: 2 + 2</b> → <b>Supply requested<br>2-page block</b></div>

Freed compatible halves can merge again.

A **slab allocator** groups many small kernel objects into larger storage blocks, avoiding a whole page allocation per tiny object.

<div class="note">These are kernel mechanisms. A C++ allocator separately manages its user-space blocks inside mapped pages.</div>

<div class="source"><a href="https://docs.kernel.org/mm/physical_memory.html">Linux physical memory</a></div>

<!--
The diagram is a minimal split example, leaving zone/locality constraints to the kernel documentation. Slab details vary, e.g. SLUB. Do not conflate user allocator arenas with kernel object caches.
-->

---

# Why free() may leave RSS unchanged

- Freed blocks may remain in allocator caches or arenas for reuse.
- Live objects can pin mostly empty pages: **external fragmentation**.
- Size-class rounding and alignment waste space inside allocations.
- Returning pages requires reclaimable whole ranges and allocator support.

<div class="callout">Track live bytes, retained bytes, and resident pages over the same workload phases.</div>

An arena improves allocation cost but can retain a whole phase's peak footprint.

<div class="source"><a href="https://man7.org/linux/man-pages/man3/malloc_trim.3.html">malloc_trim(3), glibc-specific</a> · <a href="https://man7.org/linux/man-pages/man3/malloc.3.html">malloc(3)</a></div>

<!--
malloc_trim is an implementation-specific investigation tool, not a default per-request operation. Clearing a vector does not release its capacity. shrink_to_fit is a nonbinding request and moving storage has a cost.
-->

---

# Move page work out of a critical phase

1. Allocate and initialize the real working set before serving requests.
2. Touch writable pages from the workers that will use them.
3. Keep the steady-state footprint bounded; inspect faults during load.
4. **Prefaulting** resolves page work early. `mlock` can lock eligible pages resident; check limits and errors.

`MAP_POPULATE` / `MADV_POPULATE_WRITE` can move fault work earlier, with different guarantees and failure modes.

<div class="note">Prefaulting prepares backing and mappings. It does not keep all data in CPU caches.</div>
<div class="source"><a href="https://man7.org/linux/man-pages/man2/mlock.2.html">mlock(2)</a> · <a href="https://man7.org/linux/man-pages/man2/madvise.2.html">madvise(2)</a> · <a href="https://www.man7.org/linux/man-pages/man2/mmap.2.html">mmap(2)</a></div>

<!--
Locking pages consumes a constrained resource and does not prevent all scheduling delays, COW faults, or memory-management activity. Check RLIMIT_MEMLOCK. POPULATE_WRITE availability depends on Linux version; operations can partially succeed.
-->

---

# Observe both the process and its environment

```sh
/usr/bin/time -v ./app         # elapsed time, max RSS, faults
vmstat 1                      # system activity; first row is since boot
cat /proc/pressure/memory     # time stalled under memory pressure
cat /proc/PID/smaps_rollup    # resident/share accounting
```

Then inspect the actual workload's cgroup: `memory.current`, `memory.events`, `memory.stat`, and `memory.pressure`.

<div class="note">Correlate deltas over the slow interval. Process lifetime totals can hide a request-path problem.</div>
<div class="source"><a href="https://docs.kernel.org/accounting/psi.html">PSI</a> · <a href="https://docs.kernel.org/admin-guide/cgroup-v2.html">cgroup v2</a> · <a href="https://man7.org/linux/man-pages/man1/time.1.html">time</a></div>

<!--
max RSS is a peak, not the current value or unique ownership. vmstat's si/so can help investigate swap but file refaults need other evidence. memory.current includes charges outside anonymous process RSS. Replace PID and ./app.
-->

---

# Topology is part of the performance model

<div class="grid2">
<div class="panel"><h2>Core 0</h2>Registers → L1I / L1D → private L2</div>
<div class="panel"><h2>Core 1</h2>Registers → L1I / L1D → private L2</div>
</div>
<div class="flow"><b>Shared cache / coherence fabric<br>possibly split into chiplets or clusters</b> ↔ <b>Memory controller(s)<br>and DRAM channels</b></div>

- **Simultaneous multithreading (SMT)** runs multiple hardware threads on one core; those siblings often share caches and execution resources.
- A remote cache can supply a line without a DRAM read.
- Different cores, dies, or sockets can have different communication costs.

<div class="note">Illustrative organization. Modern memory controllers are commonly integrated into the processor.</div>

<!--
Original schematic, not a specification. Some cores share L2; some designs have system-level caches. Enumerate the target topology instead of inferring it from marketing core count.
-->

---

# DRAM vocabulary: channel, rank, bank, row

| Term | Working definition |
|---|---|
| **Memory controller** | Hardware scheduling requests to DRAM |
| **Channel** | An interface transferring data between controller and memory |
| **Rank** | A group of DRAM chips working together to supply data |
| **Bank** | A separately managed array inside DRAM |
| **Row / row buffer** | A row is opened into a buffer before column data is selected |
| **Burst** | A sequence transferring several data pieces per request |

<div class="flow"><b>Controller / channel</b> → <b>Rank: cooperating chips</b> → <b>Bank: open a row</b> → <b>Select data<br>and transfer burst</b></div>

<div class="source"><a href="https://www.akkadia.org/drepper/cpumemory.pdf">Drepper §2</a></div>

<!--
This is a vocabulary diagram, not a literal universal bus wiring specification. Banks provide scheduling parallelism, while rows have activation/precharge constraints. DDR generation-specific organization belongs in the platform specification.
-->

---

# DRAM rewards organized traffic

- Controllers distribute requests across channels, ranks, and banks.
- Access uses activated rows; row hits and row changes have different costs.
- Bursts transfer data efficiently; refresh and contention also consume time.
- More channels raise the bandwidth ceiling; they do not eliminate latency.

**GT/s** means billions of transfers per second. Teaching example: **2 × 64-bit aggregate channels × 5.6 GT/s = 89.6 GB/s**.

<div class="note">Theoretical payload peak, decimal GB/s. DDR5 subchannels partition channel width; do not count that width twice. Application throughput is lower.</div>
<div class="source"><a href="https://www.akkadia.org/drepper/cpumemory.pdf">Drepper §2: DRAM mechanisms</a> · current platform specifications determine the actual channel organization.</div>

<!--
Bandwidth is data rate times width, not clock frequency alone. Bank mapping, queues, timing constraints, and read/write turnarounds complicate predictions. A linear physical address sequence is not simply one row of one DRAM chip.
-->

---

# How much overlap does bandwidth require?

Teaching model: 64-byte lines, 100 ns response time, target **50 GB/s**.

**Outstanding bytes ≈ bandwidth × latency**

50 × 10⁹ B/s × 100 × 10⁻⁹ s = **5,000 B ≈ 79 lines**.

- One outstanding line supports only about **0.64 GB/s** in this model.
- Out-of-order execution, prefetchers, multiple streams, and cores can expose more requests.
- Queues, miss-tracking entries, dependencies, and translation limit overlap.

<div class="note">Steady-state approximation, not a CPU specification. Loaded latency can increase as queues fill.</div>

<!--
This is Little's-law reasoning for the chosen boundary. Miss status holding registers / line-fill buffers and page walkers are finite resources. Bandwidth is not obtained just by adding instructions.
-->

---

# Cache policies: what can be duplicated or delayed?

| Policy | Meaning |
|---|---|
| **Write-allocate** | An ordinary store miss can allocate a line in the cache |
| **Write-back** | Modified bytes can be sent to the next level later |
| **Inclusive** | A level includes lines held by specified inner caches |
| **Exclusive** | Levels avoid holding duplicate data lines under that policy |
| **Non-inclusive** | Inclusion of inner-cache lines is not required |

<div class="note">“Exclusive cache policy” concerns duplicate data between levels. “Exclusive line ownership” concerns who may write a line.</div>

Replacement is CPU-specific; real hardware need not implement the toy model's exact LRU rule.

<div class="source"><a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html">Intel architecture manuals</a> · <a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel64-and-ia32-architectures-optimization.html">optimization manuals</a></div>

<!--
Policy relationships are defined between particular levels. Non-inclusive does not mean exclusive. Directory metadata may still track copies even when data inclusion is absent. Adding capacities is not a general unique-data capacity formula.
-->

---

# A completed store instruction can still be buffered

- A **store buffer** holds pending writes, letting an instruction finish locally before its cache/coherence work completes.
- **Store-to-load forwarding** sends matching pending-store bytes directly to a later load.
- Size, overlap, and alignment affect forwarding on a given CPU.
- A full store buffer can stall the core; a fence can constrain progress.

```cpp
*p = value;
auto copy = *p;  // compiler may eliminate this load entirely
```

<div class="note">C++ semantics, compiler transformations, and hardware ordering are separate layers.</div>

<!--
If examining forwarding, force a real load with an appropriate benchmark design and inspect assembly. Do not add volatile to ordinary application code to “fix caching.” Retirement does not imply persistence to DRAM or storage.
-->

---

# Real x86-64: four table indices and an offset

One common mode uses 48 address bits, four table levels, and 4 KiB pages.

<div class="address"><span>Level 4<br>9 bits</span><span>Level 3<br>9 bits</span><span>Level 2<br>9 bits</span><span>Level 1<br>9 bits</span><span>Offset<br>12 bits</span></div>

Each 9-bit index chooses one of **512 entries**. Each table entry leads to the next table or, at an allowed level, a final mapping called a **leaf**.

**Canonical** addresses satisfy the active mode's upper-bit rule: here bits 63…48 repeat bit 47.

<div class="note">Other active modes can use five levels. A 64-bit pointer does not mean all 64 bits are freely usable virtual addresses.</div>

<div class="source"><a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html">Intel architecture manuals</a> · <a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel64-and-ia32-architectures-optimization.html">optimization manuals</a></div>

<!--
This follows the two-level toy page walk. CR3 selects the root/address-space context. Distinguish Linux generic page-table names from x86 hardware names. The final mapping can use a larger page and stop the walk earlier.
-->

---

# Translate the hardware table names

| Hardware name | Expansion | Role in the four-level model |
|---|---|---|
| **PML4** | Page-map level 4 | Root table |
| **PDPT** | Page-directory-pointer table | Next level |
| **PD** | Page directory | Next level; can contain a 2 MiB leaf |
| **PT** | Page table | Final table for 4 KiB leaves |
| **PTE** | Page-table entry | An individual table entry; often used for final-level entries |

Walk the indices left to right, then preserve the offset exactly as in the core example.

<div class="source"><a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html">Intel architecture manuals</a> · <a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel64-and-ia32-architectures-optimization.html">optimization manuals</a></div>

<!--
A name-expansion table is reference support after the mechanism has been taught, not a replacement for the core walk diagram. Supported 1 GiB leaves can terminate at the PDPT level.
-->

---

# Page tables consume memory too

Teaching example: **1 TiB densely mapped with 4 KiB pages** on x86-64.

- 2²⁸ base pages × 8-byte leaf PTE = **2 GiB of leaf entries**.
- Upper-level tables and kernel bookkeeping add overhead.
- With 2 MiB leaves: 2¹⁹ × 8 B = **4 MiB of leaf entries**.
- Sparse mappings allocate table structures only for populated parts, with granularity costs.

<div class="callout">Large and scattered address spaces can cost more than their resident payload suggests.</div>
<div class="source"><a href="https://docs.kernel.org/mm/page_tables.html">Linux page tables</a> · arithmetic assumes the explicitly stated page and entry sizes.</div>

<!--
The 2 GiB figure is leaf-entry storage, not all overhead. One 4 KiB page-table page holds 512 8-byte entries. Large-page mapping requires suitable backing. Virtual reservation alone does not allocate all leaf tables for a whole terabyte.
-->

---

# What a hardware page walk does

1. Start from the active address-space root.
2. Read entries down the required levels.
3. Check presence, permissions, and relevant attributes.
4. Cache a usable translation, or raise a fault.

**Four levels do not mean four DRAM round trips on every load.**

TLBs, paging-structure caches, and ordinary caches can satisfy repeated lookups.

<div class="source"><a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html">Intel SDM, volume 3: translation caching</a></div>

<!--
Page walks can overlap other execution and one another within hardware limits. Accessed and dirty bits can also be updated. Cached upper-level entries shorten effective work; large-page leaves end the walk early.
-->

---

# Translation structures and address-space tags

- An **instruction TLB (ITLB)** caches instruction-address translations.
- A **data TLB (DTLB)** caches data-address translations.
- Another shared translation-cache level can satisfy a first-level miss.
- **ASID** means address-space identifier; x86 **PCID** means process-context identifier.

These tags distinguish entries belonging to different address spaces, allowing useful translations to survive some process switches.

<div class="note">Entry counts, supported page sizes, associativity, and invalidation rules are CPU-specific. A DTLB miss need not start a walk if another translation-cache level hits.</div>

<div class="source"><a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html">Intel architecture manuals</a> · <a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel64-and-ia32-architectures-optimization.html">optimization manuals</a></div>

<!--
PCID is the x86 name. Tags do not excuse stale translations after mappings change. A hardware thread or address space change need not flush every cache line or translation.
-->

---

# How can L1 lookup overlap translation?

**VIPT:** virtually indexed, physically tagged.

For a simple 32 KiB, 8-way, 64-byte-line L1:

**6 offset bits + 6 set bits = 12 bits**, all within a 4 KiB page offset.

The set can be selected before translation finishes; the physical tag confirms the line.

<div class="note">This convenient geometry is one implementation model. Larger or differently organized caches need additional design techniques.</div>
<div class="source"><a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel64-and-ia32-architectures-optimization.html">CPU optimization documentation</a></div>

<!--
A traditional alias-safe VIPT geometry has cache size per way no larger than the minimum page size. Do not claim this inequality is a universal limit on real L1 size; hardware has alternatives.
-->

---

# Changing mappings can require remote work

- `munmap`, protection changes, COW, and migration can invalidate translations.
- Other CPUs running the address space may hold stale TLB entries.
- The kernel coordinates invalidation: a **TLB shootdown**.
- Frequent mapping/protection churn can harm many-thread latency.

<div class="callout">Reuse stable mappings when the application benefits; measure the retained-memory tradeoff.</div>

<div class="source"><a href="https://docs.kernel.org/core-api/cachetlb.html">Linux: cache and TLB flushing</a></div>

<!--
Shootdowns can use IPIs or architecture-specific mechanisms; batching and generation tracking reduce work. PCID tagging does not make stale entries safe. A thread migration may lose locality even without invalidating every entry.
-->

---
class: compact
---

# Huge pages are an experiment, not a default answer

**Compaction** moves movable physical pages to create larger contiguous free blocks.

| Mechanism | Engineering tradeoff |
|---|---|
| Transparent huge pages (THP), including multiple supported sizes | OS may allocate large pages, combine or split mappings, or fall back to base pages |
| Explicit HugeTLB: Linux managed huge-page mechanism | Managed pool / reservation; operational constraints |
| Base pages | Finer-grained allocation, reclaim, and placement |

Potential costs: compaction stalls, sparse-memory waste, changed COW behavior, coarser NUMA placement.

Check mapping evidence in `smaps`, including **AnonHugePages** and relevant flags; advice can fall back.

<div class="source"><a href="https://docs.kernel.org/admin-guide/mm/transhuge.html">THP</a> · <a href="https://docs.kernel.org/admin-guide/mm/hugetlbpage.html">HugeTLB</a> · <a href="https://man7.org/linux/man-pages/man5/proc_pid_smaps.5.html">smaps</a></div>

<!--
Modern THP includes sizes below PMD size. Smaller THP can reduce allocation/fault overhead without turning every group into a single PMD TLB entry. KernelPageSize/MMUPageSize alone are insufficient evidence for all THP mappings; use AnonHugePages, VmFlags, and kernel-specific accounting. MADV_HUGEPAGE is advice, not proof.
-->

---

# Virtual machines add another translation layer

A **guest** is a virtual machine; the **host** supplies its real resources. A **hypervisor** runs the guest.

<div class="flow"><b>Guest virtual<br>address</b> → <b>Guest physical<br>address</b> → <b>Host physical<br>address</b></div>

- **Nested paging** translates guest-physical into host-physical addresses.
- Hardware caches combined results and intermediate translations.
- Guest page size and host backing both affect the result.

<div class="callout">The included results are WSL2 guest measurements. They describe this environment, not bare-metal DRAM specifications.</div>
<div class="source"><a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html">Intel SDM: EPT and virtualization</a></div>

<!--
AMD uses nested paging / NPT terminology. A guest walk itself can need nested translations for table entries; caching makes a naive fixed multiplication misleading. This is optional depth after the core translation lesson.
-->

---

# Design a translation experiment carefully

1. Build a random dependent cycle with one node on each page.
2. Sweep page count while keeping touched line bytes as controlled as possible.
3. Vary the offset within each page to avoid one-set L1 conflicts.
4. Compare base and verified huge mappings with the same logical work.
5. Collect supported TLB walk events and timing; repeat.

<div class="callout">A fixed 4 KiB stride can stress both the TLB and a modulo-indexed L1 set. Timing alone cannot separate them.</div>

<div class="note">Proposed follow-up experiment. No isolated TLB or huge-page speedup is claimed by the included measurements.</div>

<!--
A one-line-per-page test still changes page-table/cache footprints and prefetch behavior. Large pages can alter allocation/NUMA placement. Record those confounders and inspect actual mappings.
-->

---
class: compact
---

# Streaming stores and bulk operations

- **Non-temporal** describes a hint that data is unlikely to be reused soon. For large writes with little near-term reuse, non-temporal stores may reduce cache pollution and ownership-read traffic.
- Benefits depend on alignment, write combining, full-line coverage, size, and reuse.
- Ordering requirements differ: use the instruction-set/API’s required ordering barrier (**fence**) before publishing results.
- Start with optimized `memcpy` / `memset`; inspect before replacing them.

<div class="callout">“Bypass the cache” is too simple a model. Measure traffic and end-to-end latency on the target.</div>
<div class="source"><a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html">Intel SDM: streaming stores and SFENCE</a> · <a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel64-and-ia32-architectures-optimization.html">optimization manuals</a></div>

<!--
Ordinary C++ release/acquire examples later use ordinary memory stores. Platform-specific non-temporal stores need platform-specific ordering, commonly SFENCE on x86 before publication. Non-temporal does not mean durable. Libraries can choose size-dependent implementations, including rep movsb or SIMD.
-->

---

# Publication: ordering makes other data safe to read

An atomic flag alone does not automatically publish unrelated non-atomic data.

- A **release** operation publishes earlier work through a synchronization operation.
- An **acquire** observing that release establishes the corresponding ordering for the reader.
- **Happens-before** is the C++ relation that makes those earlier writes ordered before the later reads.

<div class="flow"><b>Producer<br>write payload</b> → <b>Release: ready=true</b> → <b>Acquire sees true</b> → <b>Consumer<br>read payload</b></div>

<div class="note">This is a single-publication protocol. Reusing the payload requires another lifetime and synchronization design.</div>

<div class="source"><a href="https://eel.is/c++draft/atomics.order">C++ atomic ordering</a> · <a href="https://eel.is/c++draft/intro.races">data races</a></div>

<!--
The acquire must observe the relevant release or applicable release sequence. The definitions here intentionally precede the code example. Coherence of individual lines alone does not establish a C++ happens-before relation.
-->

---
class: compact
---

# Coherence does not publish a C++ object safely

```cpp
Payload payload;
std::atomic<bool> ready{false};

// Producer: initialize once.
payload = make_payload();
ready.store(true, std::memory_order_release);

// Consumer: use after observing publication.
if (ready.load(std::memory_order_acquire))
  use(payload);
```

Acquire observing this release establishes the required happens-before relation.

<div class="note">Assume one publication, no later payload mutation, and valid lifetimes. Reuse needs another protocol.</div>
<div class="source"><a href="https://eel.is/c++draft/atomics.order">C++ draft: release/acquire</a> · <a href="https://eel.is/c++draft/intro.races">happens-before and data races</a></div>

<!--
Code blocks represent two threads, not sequential execution. The acquire must observe the publishing release (or the applicable release sequence). A relaxed flag does not publish the non-atomic payload. Hardware that often appears to work cannot repair undefined C++ behavior.
-->

---
class: compact
---

# Ordering is a separate contract

Initially `x = y = 0`; both variables are `std::atomic<int>`.

<div class="grid2"><div>

```cpp
// Thread 0
x.store(1, std::memory_order_relaxed);
r0 = y.load(std::memory_order_relaxed);
```

</div><div>

```cpp
// Thread 1
y.store(1, std::memory_order_relaxed);
r1 = x.load(std::memory_order_relaxed);
```

</div></div>

**Sequentially consistent** operations participate in a single order consistent with each thread’s required ordering.

With the relaxed accesses above, **r0 == 0 and r1 == 0 is allowed.** Store buffering is one hardware explanation, including on x86.

<div class="note">Making all four accesses sequentially consistent forbids that outcome. Select ordering to satisfy the required protocol.</div>
<div class="source"><a href="https://eel.is/c++draft/atomics.order">C++ ordering</a> · <a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html">Intel SDM: memory ordering</a></div>

<!--
r0 and r1 are separate thread-local result variables, observed after joining. No other stores occur. The relaxed example is well-defined because x and y are atomic. Replacing them with plain ints would introduce data races, not create a valid hardware litmus test in C++.
-->

---

# Relaxed atomics can still be physically expensive

```cpp
shared.fetch_add(1, std::memory_order_relaxed);
```

- Atomic read-modify-write still needs indivisible modification.
- A contended cache line still moves or serializes between cores.
- Relaxed ordering does not turn a shared counter into a local register.
- `volatile` provides neither C++ synchronization nor a cache flush.

<div class="callout">Fewer shared modifications often matter more than choosing a weaker memory order.</div>
<div class="source"><a href="https://eel.is/c++draft/atomics.order">C++ atomic ordering</a> · <a href="https://eel.is/c++draft/intro.races">data races</a></div>

<!--
Do not imply every atomic is lock-free. Check is_always_lock_free/is_lock_free for the type and platform. x86 relaxed atomic RMW commonly still uses a locked instruction. Other ISAs have different instruction sequences and retry behavior.
-->

---
class: compact
---

# Investigate placement with explicit experiments

**CPU binding** restricts where work runs; **memory binding** restricts where allocation may obtain pages.

```sh
numactl --hardware
numastat -p PID
cat /proc/PID/numa_maps

# On a machine with the required nodes; replace ./app with your workload.
numactl --cpunodebind=0 --membind=0 ./app
numactl --cpunodebind=0 --membind=1 ./app
```

Compare local / remote access, per-node bandwidth, and worker scaling.

<div class="note">Interleaving can balance bandwidth for globally shared streams; it can increase latency for local data. Measure both.</div>
<div class="source"><a href="https://man7.org/linux/man-pages/man8/numactl.8.html">numactl(8)</a> · no NUMA speedup is claimed by the included single-node results.</div>

<!--
Replace PID with the application's PID. Node 1 must exist and be allowed by cpuset constraints. Strict binding can cause allocation failure; describe the experimental host's policy. Page-table placement can also affect large random workloads.
-->

---

# MESI names describe the ownership states we traced

The **MESI** name comes from four simplified cache-line states:

| State | Meaning |
|---|---|
| **Modified** | This cache has the exclusive dirty copy |
| **Exclusive** | This cache has the only clean copy |
| **Shared** | Clean copies may exist in several caches |
| **Invalid** | This cache has no usable copy |

A writer obtains suitable permission; other copies may become invalid. This is the behavior traced in the core lessons.

<div class="note">Actual protocols have more states and transitions. Coherence of lines does not establish the C++ ordering required between different locations.</div>
<div class="source"><a href="https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html">Intel architecture manuals</a></div>

<!--
Optional vocabulary after the core ownership traces. AMD MOESI adds Owned, and real protocols include transient states. Distinguish exclusive line ownership from exclusive data placement between cache levels.
-->

---

# Scaling stops at shared resources

- More workers can saturate channels before all cores are busy.
- SMT shares resources; two threads per core need not double throughput.
- Cache capacity per worker can shrink as concurrency rises.
- More threads can increase coherence, allocator, and page-management contention.

<div class="callout">Plot throughput, per-operation latency, and memory footprint against worker count. Find the useful operating point.</div>

<div class="note">A one-thread microbenchmark is insufficient evidence for a production thread count.</div>

<!--
Separate latency, bandwidth, CPU utilization, migrations, and queueing. Sometimes fewer workers improve tail latency and efficiency. Do not infer shared-resource saturation from a flat speedup curve without additional evidence.
-->

---

# What was measured here?

- AMD Ryzen 5 4600H reported to a WSL2 guest; Linux 6.18.54.1.
- Guest caches: **32 KiB L1D, 512 KiB L2 per core, 4 MiB L3**.
- GCC 13.3, C++20, `-O3 -march=native`; separate files, no link-time optimization (LTO).
- One thread pinned to logical CPU 2; **seven repetitions**.
- Steady-state tests: warm-up, ≥40 ms per repetition; first-write tests: fresh mappings.

<div class="note">Host load, clock frequency, and physical placement are uncontrolled. Raw ranges are retained. No hardware-counter evidence was collected.</div>
<div class="source">Local evidence: benchmarks/results/{raw.csv,metadata.json,summary.csv,README.md}. Record date and exact command are in metadata.json.</div>

<!--
Ordinary heap page backing was not verified; do not label it confirmed base pages or huge pages. First-write mappings explicitly use MADV_NOHUGEPAGE. Guest topology does not prove the host resource allocation.
-->

---
class: compact
---

# Get counter evidence for the hypothesis

```sh
perf list
perf stat -r 5 -e cycles,instructions,page-faults,minor-faults,major-faults \
  -- taskset -c 2 ./build/memory-bench --filter transpose --min-ms 300
# If supported by your PMU, investigate data-cache and DTLB walk events.
```

- Generic `cache-misses` does not always mean “all cache misses.”
- Distinguish DTLB misses, second-level hits, and completed page walks.
- Counts include setup/checks/calibration unless explicitly gated.
- Multiplexing, sampling skid (a sample can be attributed near, rather than exactly at, its triggering instruction), and unsupported events limit conclusions.

<div class="source"><a href="https://man7.org/linux/man-pages/man1/perf-stat.1.html">perf stat</a> · <a href="https://perfmon-events.intel.com/">Intel PMU events</a></div>

<!--
Run from benchmarks/. Use perf list for the actual CPU; dTLB-load-misses can have model-specific semantics and can be unavailable in VMs. Whole-process counts must not be divided by only the timed iterations. For attribution use gated counters, sampling, or a long dedicated steady-state kernel.
-->

---

# Separate correctness tools from performance evidence

| Question | Useful investigation |
|---|---|
| Out-of-bounds / use-after-free? | AddressSanitizer with representative tests |
| Undefined operations or misalignment? | UndefinedBehaviorSanitizer; inspect assumptions |
| Unsynchronized shared accesses? | ThreadSanitizer in a separate build |
| Who allocates and retains memory? | Allocation profiling, heap snapshots, allocator statistics |
| Why is this hot access slow? | Optimized build, assembly, timing, supported PMU events |

<div class="note">Sanitizers and heap tools alter layout, allocation, and timing. Their runtimes are not the performance baseline.</div>
<div class="source"><a href="https://clang.llvm.org/docs/AddressSanitizer.html">ASan</a> · <a href="https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html">UBSan</a> · <a href="https://clang.llvm.org/docs/ThreadSanitizer.html">TSan</a></div>

<!--
Instrumentation can expose correctness bugs and allocation sources, but changes cache/TLB footprints and thread scheduling. Each sanitizer has limitations and cannot prove all C++ code is correct. MemorySanitizer is another option for uninitialized reads when its toolchain/dependency requirements can be met.
-->

---
class: refs
---

# Reading: mechanisms and machine details

- [Drepper: What Every Programmer Should Know About Memory](https://www.akkadia.org/drepper/cpumemory.pdf): §§2–3 hardware and caches; §4 virtual memory; §5 NUMA; §6 optimization; §7 tools.
- [Intel Software Developer Manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html): volume 3 paging, translation caches, memory ordering; volume 2 instruction semantics.
- [Intel optimization manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel64-and-ia32-architectures-optimization.html): use the chapter for the actual target microarchitecture.
- [Roofline, original report](https://www2.eecs.berkeley.edu/Pubs/TechRpts/2008/EECS-2008-134.pdf): connect arithmetic intensity to sustainable bandwidth.
- [STREAM, original benchmark](https://www.cs.virginia.edu/stream/): sustainable streaming bandwidth and its reporting rules.

<!--
Reading pointers are primary sources. STREAM is a follow-up reference; our included AoS scan is not a STREAM result and must not be reported as one.
-->

---
class: refs
---

# Reading: Linux memory management

- [Page tables](https://docs.kernel.org/mm/page_tables.html), [memory concepts](https://docs.kernel.org/admin-guide/mm/concepts.html), [physical memory](https://docs.kernel.org/mm/physical_memory.html).
- [mmap](https://www.man7.org/linux/man-pages/man2/mmap.2.html), [madvise](https://man7.org/linux/man-pages/man2/madvise.2.html), [mlock](https://man7.org/linux/man-pages/man2/mlock.2.html), [smaps](https://man7.org/linux/man-pages/man5/proc_pid_smaps.5.html).
- [THP](https://docs.kernel.org/admin-guide/mm/transhuge.html), [HugeTLB](https://docs.kernel.org/admin-guide/mm/hugetlbpage.html), [NUMA memory policy](https://docs.kernel.org/admin-guide/mm/numa_memory_policy.html).
- [Overcommit](https://cdn.kernel.org/doc/html/latest/mm/overcommit-accounting.html), [cgroup v2](https://docs.kernel.org/admin-guide/cgroup-v2.html), [pressure stall information](https://docs.kernel.org/accounting/psi.html).

<div class="note">Interfaces and policy evolve. Match the documentation to the deployed kernel.</div>

<!--
These sources support the OS-specific parts; architectural mechanisms alone do not specify Linux's reclaim and placement policies.
-->

---
class: refs
---

# Reading: C++ contracts and measurement

- C++ draft: [object lifetime](https://eel.is/c++draft/basic.life), [permitted accesses](https://eel.is/c++draft/basic.lval), [data races](https://eel.is/c++draft/intro.races), [atomic ordering](https://eel.is/c++draft/atomics.order).
- C++ draft: [monotonic resources](https://eel.is/c++draft/mem.res.monotonic.buffer), [vector capacity](https://eel.is/c++draft/vector.capacity).
- GCC: [prefetch builtin](https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html), [restricted pointers](https://gcc.gnu.org/onlinedocs/gcc/Restricted-Pointers.html).
- [perf stat](https://man7.org/linux/man-pages/man1/perf-stat.1.html), [perf mem](https://man7.org/linux/man-pages/man1/perf-mem.1.html), [perf c2c](https://man7.org/linux/man-pages/man1/perf-c2c.1.html), [Intel event definitions](https://perfmon-events.intel.com/).

<div class="note">Runnable source, correctness checks, all raw timing repetitions, and chart generation accompany this deck.</div>

<!--
C++ draft links are live references, not a fixed published standard edition. PMU support differs by CPU/kernel and may be unavailable under virtualization.
-->
