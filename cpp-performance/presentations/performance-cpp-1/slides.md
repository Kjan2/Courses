---
theme: default
title: "performance1"
layout: cover
highlighter: shiki
fonts:
  sans: Arial
  mono: DejaVu Sans Mono
  provider: none
transition: fade
---

# Performance C++
# 1

<style>
:global(.slidev-layout.two-columns > .code-pair) {
  align-self: center;
}
:global(.slidev-layout) {
  column-gap: 22px;
  padding: 38px;
  background: radial-gradient(ellipse at top right, #153b59, transparent 65%), #07111c;
  color: #e7edf5;
}
:global(.slidev-layout h1),
:global(.slidev-layout th),
:global(.slidev-layout strong) {
  color: #71c6ff;
}
:global(.slidev-layout.two-columns > .code-pair),
:global(.slidev-layout table) {
  min-width: 0;
  overflow: hidden;
  border: 1px solid #2b4056;
  border-radius: 16px;
  background: #101b2a;
  box-shadow: 0 18px 45px #0005;
}
:global(.slidev-layout table) {
  border-collapse: separate;
  border-spacing: 0;
}
:global(.slidev-layout th),
:global(.slidev-layout td) {
  border-color: #2b4056;
}
:global(.slidev-layout :not(pre) > code) {
  background: #1b3045;
  color: #b8dfff;
}
:global(.code-pair .slidev-code) {
  margin: 0;
  padding: 16px !important;
  border-radius: 0;
  background: #101b2a !important;
}
:global(.code-pair pre),
:global(.code-pair pre code) {
  font-size: 12px !important;
  line-height: 1.45 !important;
}
:global(.code-pair .shiki span) {
  color: var(--shiki-dark, #d7e5f3) !important;
}
:global(.slidev-layout.benchmark) {
  display: flex;
  flex-direction: column;
  justify-content: center;
  align-items: center;
}
:global(.benchmark table) {
  width: 100%;
  max-width: 100%;
  margin: 0;
  font-size: 21.6px;
  table-layout: fixed;
}
:global(.benchmark th),
:global(.benchmark td) {
  overflow-wrap: anywhere;
}
:global(.benchmark th code) {
  white-space: normal;
}
:global(.benchmark .workload) {
  margin: 16px 0 0;
  color: #93a0af;
  font-size: 14px;
  text-align: center;
}
</style>

<!-- Заглавный слайд. -->


---
layout: two-cols
class: code-pair
---

~~~cpp
std::uint32_t mix(std::uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

std::uint64_t calculate(
    const std::uint32_t* values,
    std::size_t n) {
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < n; ++i)
    sum += mix(values[i]);
  return sum;
}
~~~

::right::

~~~cpp
std::uint64_t lookup(
    const std::uint32_t* values,
    std::size_t n,
    const std::uint32_t* table) {
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < n; ++i)
    sum += table[values[i]];
  return sum;
}
~~~

---
class: benchmark
---

| Compiler | Table size | `calculate` | `lookup` | Change |
|---|---:|---:|---:|---:|
| GCC 13 | 4 KiB | 0.1725 ms | 0.0805 ms | **2,1× faster** |
| GCC 13 | 256 KiB | 0.169 ms | 0.17 ms | ≈ same |
| GCC 13 | 64 MiB | 0.169 ms | 3.0805 ms | **17–20× slower** |
| Clang 18 | 4 KiB | 0.293 ms | 0.0805 ms | **3,7× faster** |
| Clang 18 | 256 KiB | 0.292 ms | 0.169 ms | **1,7× faster** |
| Clang 18 | 64 MiB | 0.3075 ms | 3.552 ms | **11–12× slower** |

<div class="workload">262 144 lookups</div>

<!-- Медианы двух запусков. -->

---
layout: two-cols
class: code-pair
---

~~~cpp
std::uint64_t conditional(
    const std::uint32_t* values,
    const std::uint8_t* active,
    std::size_t n) {
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < n; ++i) {
    if (active[i])
      sum += mix(values[i]);
  }
  return sum;
}
~~~

::right::

~~~cpp
std::uint64_t eager(
    const std::uint32_t* values,
    const std::uint8_t* active,
    std::size_t n) {
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < n; ++i) {
    sum += std::uint64_t{mix(values[i])}
           * active[i];
  }
  return sum;
}
~~~

---
class: benchmark
---

| Active | `conditional` | `eager` | Change |
|---:|---:|---:|---:|
| 0% | 0.0775 ms | 0.3085 ms | **4,0× slower** |
| 1% | 0.1235 ms | 0.3145 ms | **2,5× slower** |
| 50% random | 1.093 ms | 0.313 ms | **3,5× faster** |
| 100% | 0.347 ms | 0.309 ms | **1,1× faster** |

<div class="workload">262 144 elements · Clang 18</div>

<!-- Медианы двух запусков. -->

---
layout: two-cols
class: code-pair
---

~~~cpp
std::uint64_t sum_plain(
    const std::uint32_t* values,
    std::size_t n) {
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < n; ++i)
    sum += values[i];
  return sum;
}
~~~

::right::

~~~cpp
std::uint64_t sum_skip_zero(
    const std::uint32_t* values,
    std::size_t n) {
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < n; ++i) {
    if (values[i] != 0)
      sum += values[i];
  }
  return sum;
}
~~~

---
class: benchmark
---

| Compiler | Zeros | `sum_plain` | `sum_skip_zero` |
|---|---:|---:|---:|
| GCC 13 | 0% | 0.0405 ms | 0.0395 ms |
| GCC 13 | 50% | 0.039 ms | 0.039 ms |
| GCC 13 | 100% | 0.042 ms | 0.0415 ms |
| Clang 18 | 0% | 0.032 ms | 0.0325 ms |
| Clang 18 | 50% | 0.0345 ms | 0.0355 ms |
| Clang 18 | 100% | 0.0315 ms | 0.0325 ms |

<div class="workload">262 144 elements · GCC / Clang: identical machine code</div>

<!-- Медианы двух запусков; машинный код функций одинаков внутри каждого компилятора. -->

---
layout: two-cols
class: code-pair
---

~~~cpp
std::uint64_t sum_plain(
    const std::uint32_t* values,
    std::size_t n) {
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < n; ++i)
    sum += values[i];
  return sum;
}
~~~

::right::

~~~cpp
std::uint64_t sum_unrolled(
    const std::uint32_t* v,
    std::size_t n) {
  std::uint64_t sum = 0;
  std::size_t i = 0;
  for (; i + 8 <= n; i += 8) {
    sum += v[i];     sum += v[i + 1];
    sum += v[i + 2]; sum += v[i + 3];
    sum += v[i + 4]; sum += v[i + 5];
    sum += v[i + 6]; sum += v[i + 7];
  }
  for (; i < n; ++i) sum += v[i];
  return sum;
}
~~~

---
class: benchmark
---

| Compiler | `sum_plain` | `sum_unrolled` | Change |
|---|---:|---:|---:|
| GCC 13 | 0.0393 ms | 0.03025 ms | **21–25% faster** |
| Clang 18 | 0.03125 ms | 0.0375 ms | **16–24% slower** |

<div class="workload">262 144 elements</div>

<!-- Медианы двух запусков. -->

---
layout: two-cols
class: code-pair
---

~~~cpp
struct Counter {
  std::atomic<unsigned> value{0};
};

Counter counters[4];

for (unsigned i = 0;
     i < work_per_thread; ++i) {
  counters[t].value.fetch_add(
      1, std::memory_order_relaxed);
}
~~~

::right::

~~~cpp
struct alignas(64) Counter {
  std::atomic<unsigned> value{0};
};

Counter counters[4];

for (unsigned i = 0;
     i < work_per_thread; ++i) {
  counters[t].value.fetch_add(
      1, std::memory_order_relaxed);
}
~~~

---
class: benchmark
---

| Threads | `Counter` | `alignas(64) Counter` |
|---:|---:|---:|
| 1 | 7.15 ms | 7.15 ms |
| 2 | 10.15 ms | **3.75 ms** |
| 4 | 10.25 ms | **3.75 ms** |

<div class="workload">1 048 576 increments · GCC / Clang</div>

<!-- Медианы двух запусков GCC и Clang. -->

---
layout: two-cols
class: code-pair
---

~~~cpp
std::uint64_t separate(
    const std::uint32_t* values,
    std::size_t n) {
  std::uint64_t sum = 0;
  std::uint64_t hash = 0;

  for (std::size_t i = 0; i < n; ++i)
    sum += values[i];
  for (std::size_t i = 0; i < n; ++i)
    hash = (hash ^ values[i])
           * 1099511628211ULL;

  return sum + hash;
}
~~~

::right::

~~~cpp
std::uint64_t fused(
    const std::uint32_t* values,
    std::size_t n) {
  std::uint64_t sum = 0;
  std::uint64_t hash = 0;

  for (std::size_t i = 0; i < n; ++i) {
    sum += values[i];
    hash = (hash ^ values[i])
           * 1099511628211ULL;
  }

  return sum + hash;
}
~~~

---
class: benchmark
---

| Compiler | Elements | `separate` | `fused` | Change |
|---|---:|---:|---:|---:|
| GCC 13 | 4 096 | 4915 ns | 4230 ns | **13–16% faster** |
| GCC 13 | 262 144 | 0.3255 ms | 0.2905 ms | **8–14% faster** |
| GCC 13 | 16 777 216 | 24.7525 ms | 19.5575 ms | **19–23% faster** |
| Clang 18 | 262 144 | 0.332 ms | 0.293 ms | **11–13% faster** |
| Clang 18 | 16 777 216 | 24.2255 ms | 19.324 ms | **20–21% faster** |

<div class="workload">4 096 / 262 144 / 16 777 216 elements</div>

<!-- Медианы двух запусков. -->

---
layout: two-cols
class: code-pair
---

~~~cpp
std::uint64_t count_flags(
    const std::vector<std::uint8_t>& flags) {
  std::uint64_t count = 0;
  for (std::uint8_t flag : flags)
    count += flag;
  return count;
}
~~~

::right::

~~~cpp
std::uint64_t count_flags(
    const std::vector<bool>& flags) {
  std::uint64_t count = 0;
  for (bool flag : flags)
    count += flag;
  return count;
}
~~~

---
class: benchmark
---

| Compiler | Flags | `vector<int>` | `vector<bool>` | Change |
|---|---:|---:|---:|---:|
| GCC 13 | 65 536 | 9300 ns | 0.04875 ms | **5,2× slower** |
| GCC 13 | 16 777 216 | 2.6855 ms | 11.4405 ms | **4,2× slower** |
| Clang 18 | 65 536 | 9700 ns | 0.06345 ms | **6,5× slower** |
| Clang 18 | 16 777 216 | 3.4755 ms | 16.983 ms | **4,2–5,8× slower** |

<div class="workload">65 536 / 16 777 216 flags</div>

<!--
Медианы двух запусков.
-->

---
layout: two-cols
class: code-pair
---

~~~cpp
std::string make_name(std::size_t n) {
  std::string s(n, 'x');
  return s;
}
~~~

::right::

~~~cpp
std::string make_name(std::size_t n) {
  std::string s(n, 'x');
  return std::move(s);
}
~~~

---
class: benchmark
---

| Compiler | Characters | `return s` | `return std::move(s)` |
|---|---:|---:|---:|
| GCC 13 | 15 | 7 ns | 12 ns |
| GCC 13 | 1 000 | 26 ns | 26 ns |
| Clang 18 | 15 | 7 ns | 14 ns |
| Clang 18 | 1 000 | 25 ns | 27 ns |

<div class="workload">-O3 · run 2 · median of 15 rounds · construction + destruction</div>

---
layout: two-cols
class: code-pair
---

~~~cpp
std::string make(std::size_t n) {
  return std::string(n, 'x');
}
~~~

::right::

~~~cpp
std::string make(std::size_t n) {
  thread_local std::string buffer;
  buffer.assign(n, 'x');
  return buffer;
}
~~~

---
class: benchmark
---

| Compiler | Characters | Fresh string | Reused buffer |
|---|---:|---:|---:|
| GCC 13 | 1 000 | 0.029 µs | 0.044 µs |
| GCC 13 | 1 000 000 | 13.494 µs | 38.778 µs |
| Clang 18 | 1 000 | 0.027 µs | 0.036 µs |
| Clang 18 | 1 000 000 | 12.050 µs | 35.029 µs |

<div class="workload">-O3 · run 2 · median of 15 rounds · returned string destroyed each call</div>

---
layout: two-cols
class: code-pair
---

~~~cpp
void sort(std::uint32_t* a,
          std::size_t n) {
  for (std::size_t i = 1; i < n; ++i) {
    auto value = a[i];
    std::size_t j = i;
    while (j && value < a[j - 1]) {
      a[j] = a[j - 1];
      --j;
    }
    a[j] = value;
  }
}
~~~

::right::

~~~cpp
void sort(std::uint32_t* a,
          std::size_t n) {
  std::sort(a, a + n);
}
~~~

---
class: benchmark
---

| Compiler | Elements / array | Insertion sort | `std::sort` |
|---|---:|---:|---:|
| GCC 13 | 32 | 1.722 ms | 2.039 ms |
| GCC 13 | 128 | 14.010 ms | 11.741 ms |
| Clang 18 | 32 | 1.502 ms | 2.074 ms |
| Clang 18 | 128 | 13.290 ms | 12.119 ms |

<div class="workload">4 096 random arrays · copy + sort + median-element checksum · -O3 · run 2 · median of 15 rounds</div>

---
layout: two-cols
class: code-pair
---

~~~cpp
std::thread worker([&] {
  for (std::size_t i = 0; i < n; ++i)
    output[i] = mix(input[i]);
});
worker.join();

std::uint64_t sum = 0;
for (std::size_t i = 0; i < n; ++i)
  sum += output[i];
~~~

::right::

~~~cpp
std::vector<std::thread> threads;
for (int t = 0; t < workers; ++t) {
  auto first = n * t / workers;
  auto last = n * (t + 1) / workers;
  threads.emplace_back([=] {
    for (std::size_t i = first;
         i < last; ++i)
      output[i] = mix(input[i]);
  });
}
for (auto& t : threads) t.join();

std::uint64_t sum = 0;
for (std::size_t i = 0; i < n; ++i)
  sum += output[i];
~~~

---
class: benchmark
---

| Elements | 1 thread | 2 threads | 4 threads | 8 threads |
|---:|---:|---:|---:|---:|
| 4 096 | 0.116 ms | 0.178 ms | 0.292 ms | 0.583 ms |
| 16 777 216 | 16.563 ms | 13.120 ms | 12.086 ms | 12.375 ms |

<div class="workload">GCC 13 · -O3 · run 2 · median of 15 rounds · thread creation + transform + join + serial checksum</div>

---
layout: two-cols
class: code-pair
---

~~~cpp
std::uint64_t lookup(
    const std::vector<Item>& items,
    std::uint64_t key) {
  for (const auto& x : items) {
    if (x.key == key)
      return x.value;
  }
  return 0;
}
~~~

::right::

~~~cpp
std::uint64_t lookup(
    const std::unordered_map<
      std::uint64_t, std::uint64_t>& map,
    std::uint64_t key) {
  auto it = map.find(key);
  if (it != map.end())
    return it->second;
  return 0;
}
~~~

---
class: benchmark
---

| Compiler | Query pattern | Linear search | `unordered_map` |
|---|---|---:|---:|
| GCC 13 | First key repeated | 0.141 ms | 0.986 ms |
| GCC 13 | Random existing key | 2.704 ms | 1.066 ms |
| GCC 13 | Same missing key | 1.972 ms | 4.112 ms |
| Clang 18 | First key repeated | 0.150 ms | 0.994 ms |
| Clang 18 | Random existing key | 2.568 ms | 1.062 ms |
| Clang 18 | Same missing key | 1.774 ms | 4.105 ms |

<div class="workload">20 items · 262 144 queries · -O3 · focused run · median of 15 rounds · construction excluded</div>

---
layout: two-cols
class: code-pair
---

~~~cpp
std::uint64_t sum(
    const std::uint64_t* data,
    std::size_t n) {
  std::uint64_t result = 0;
  for (std::size_t i = 0; i < n; ++i)
    result += data[i];
  return result;
}
~~~

::right::

~~~cpp
std::uint64_t sum(
    const std::uint64_t* data,
    std::size_t n) {
  std::uint64_t a = 0, b = 0;
  std::uint64_t c = 0, d = 0;
  std::size_t i = 0;
  for (; i + 4 <= n; i += 4) {
    a += data[i];
    b += data[i + 1];
    c += data[i + 2];
    d += data[i + 3];
  }
  for (; i < n; ++i) a += data[i];
  return a + b + c + d;
}
~~~

---
class: benchmark
---

| Compiler | One accumulator | Four accumulators | Change |
|---|---:|---:|---:|
| GCC 13 | 552.373 µs | 515.682 µs | **6.6% faster** |
| Clang 18 | 531.500 µs | 595.540 µs | **12.0% slower** |

<div class="workload">1 048 576 uint64_t values · -O3 · run 2 · median of 15 rounds</div>

---
layout: two-cols
class: code-pair
---

~~~cpp
struct Params {
  std::uint32_t multiplier;
  std::uint32_t addend;
};

void transform(
    std::uint32_t* output,
    const std::uint32_t* input,
    std::size_t n,
    const Params& params) {
  for (std::size_t i = 0; i < n; ++i)
    output[i] = input[i]
      * params.multiplier
      + params.addend;
}
~~~

::right::

~~~cpp
struct Params {
  std::uint32_t multiplier;
  std::uint32_t addend;
};

void transform(
    std::uint32_t* output,
    const std::uint32_t* input,
    std::size_t n,
    Params params) {
  for (std::size_t i = 0; i < n; ++i)
    output[i] = input[i]
      * params.multiplier
      + params.addend;
}
~~~

---
class: benchmark
---

| Compiler | Elements | `const Params&` | `Params` | Change |
|---|---:|---:|---:|---:|
| GCC 13 | 16 | 5.64 ns | 5.43 ns | **4% faster** |
| GCC 13 | 1 024 | 214.9 ns | 214.3 ns | ≈ same |
| GCC 13 | 1 048 576 | 569.4 µs | 572.8 µs | ≈ same |
| Clang 18 | 16 | 5.11 ns | 4.23 ns | **17% faster** |
| Clang 18 | 1 024 | 183.9 ns | 180.8 ns | ≈ same |
| Clang 18 | 1 048 576 | 642.0 µs | 631.8 µs | ≈ same |

<div class="workload">33 554 432 transformed values per sample · -O3 · separate translation units · median of 11 alternating rounds</div>

<!-- Обе версии векторизуются. Передача по ссылке добавляет возможное перекрытие с output, но разница заметна только на очень коротких вызовах. -->

---
layout: two-cols
class: code-pair
---

~~~cpp
std::uint64_t sum_plain(
    const std::uint32_t* values,
    std::size_t n) {
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < n; ++i)
    sum += values[i];
  return sum;
}
~~~

::right::

~~~cpp
std::uint64_t sum_prefetch(
    const std::uint32_t* values,
    std::size_t n) {
  std::uint64_t sum = 0;
  std::size_t i = 0;
  for (; i + 64 < n; ++i) {
    __builtin_prefetch(values + i + 64);
    sum += values[i];
  }
  for (; i < n; ++i)
    sum += values[i];
  return sum;
}
~~~

---
class: benchmark
---

| Compiler | Elements | Plain | Manual prefetch | Change |
|---|---:|---:|---:|---:|
| GCC 13 | 4 096 | 0.560 µs | 1.702 µs | **3.0× slower** |
| GCC 13 | 1 048 576 | 0.204 ms | 0.464 ms | **2.3× slower** |
| GCC 13 | 16 777 216 | 4.805 ms | 7.874 ms | **1.6× slower** |
| Clang 18 | 4 096 | 0.557 µs | 1.095 µs | **2.0× slower** |
| Clang 18 | 1 048 576 | 0.200 ms | 0.308 ms | **1.5× slower** |
| Clang 18 | 16 777 216 | 5.028 ms | 5.797 ms | **1.15× slower** |

<div class="workload">sequential uint32_t sum · prefetch distance 256 bytes · -O3 · median of 11 alternating rounds</div>

<!-- Обычный цикл векторизуется, основной цикл с __builtin_prefetch — нет. Линейный доступ также уже удобен аппаратному предвыборщику. -->

---
layout: two-cols
class: code-pair
---

~~~cpp
void add_each(
    const std::uint32_t* values,
    std::size_t first,
    std::size_t last,
    std::atomic<std::uint64_t>& total) {
  for (std::size_t i = first;
       i < last; ++i) {
    total.fetch_add(
      values[i],
      std::memory_order_relaxed);
  }
}
~~~

::right::

~~~cpp
void add_batched(
    const std::uint32_t* values,
    std::size_t first,
    std::size_t last,
    std::atomic<std::uint64_t>& total) {
  std::uint64_t local = 0;
  for (std::size_t i = first;
       i < last; ++i) {
    local += values[i];
  }
  total.fetch_add(
    local,
    std::memory_order_relaxed);
}
~~~

---
class: benchmark
---

| Compiler | Threads | Atomic each | Batched | Change |
|---|---:|---:|---:|---:|
| GCC 13 | 1 | 5.23 ms | 0.64 ms | **8.2× faster** |
| GCC 13 | 2 | 9.94 ms | 0.55 ms | **18× faster** |
| GCC 13 | 4 | 10.76 ms | 0.46 ms | **23× faster** |
| GCC 13 | 8 | 10.56 ms | 0.77 ms | **14× faster** |
| Clang 18 | 1 | 5.37 ms | 0.69 ms | **7.8× faster** |
| Clang 18 | 2 | 10.91 ms | 0.57 ms | **19× faster** |
| Clang 18 | 4 | 11.26 ms | 0.61 ms | **19× faster** |
| Clang 18 | 8 | 11.14 ms | 0.74 ms | **15× faster** |

<div class="workload">1 048 576 values · thread creation included · -O3 · median of 11 alternating rounds</div>

<!-- Эквивалентность требует, чтобы промежуточное значение счётчика никто не наблюдал. На машине четыре физических ядра, поэтому восемь потоков увеличивают накладные расходы. -->
