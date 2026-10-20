#include "kernels.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>

using Clock = std::chrono::steady_clock;
constexpr std::size_t MiB = 1024 * 1024;
std::uintptr_t sink = 0;
int repetitions = 7;
double min_seconds = 0.04;
std::string filter;

void require(bool ok, const char* message) {
  if (!ok) throw std::runtime_error(message);
}
bool selected(const std::string& name) { return name.find(filter) != std::string::npos; }
long minor_faults() {
  rusage usage{};
  require(getrusage(RUSAGE_SELF, &usage) == 0, "getrusage failed");
  return usage.ru_minflt;
}
void row(const std::string& name, std::size_t footprint, int rep,
         std::size_t iterations, double seconds, double operations,
         std::size_t useful_bytes, long faults) {
  std::cout << name << ',' << footprint << ',' << rep << ',' << iterations << ','
            << seconds << ',' << seconds * 1e9 / operations << ','
            << useful_bytes << ',' << faults << '\n';
}
void measure(const std::string& name, std::size_t footprint,
             std::size_t operations, std::size_t useful_bytes,
             const std::function<std::uintptr_t()>& fn) {
  if (!selected(name)) return;
  sink ^= fn();  // warm data, code, and translations; not a cold-cache test
  for (int rep = 0; rep < repetitions; ++rep) {
    std::size_t iterations = 0;
    const long before_faults = minor_faults();
    const auto start = Clock::now();
    double elapsed;
    do {
      sink ^= fn();
      ++iterations;
      elapsed = std::chrono::duration<double>(Clock::now() - start).count();
    } while (elapsed < min_seconds);
    row(name, footprint, rep, iterations, elapsed,
        static_cast<double>(operations) * iterations, useful_bytes,
        minor_faults() - before_faults);
  }
}

void check() {
  // All nodes in exactly one cycle; eight streams partition the same cycle.
  std::vector<Node> nodes(64);
  for (std::size_t i = 0; i < nodes.size(); ++i) nodes[i].next = &nodes[(i + 1) % nodes.size()];
  require(chase(nodes.data(), 64) == reinterpret_cast<std::uintptr_t>(nodes.data()), "chase cycle");
  Node* starts[8];
  std::uintptr_t expected = 0;
  for (std::size_t k = 0; k < 8; ++k) {
    starts[k] = &nodes[k * 8];
    expected ^= reinterpret_cast<std::uintptr_t>(&nodes[(k*8 + 9) % 64]);
  }
  require(chase8(starts, 9) == expected, "chase8 endpoints");
  std::vector<Record> aos(1001);
  std::vector<std::uint64_t> soa(aos.size());
  for (std::size_t i = 0; i < soa.size(); ++i) aos[i].value = soa[i] = i;
  require(sum_aos(aos.data(), aos.size()) == 500500, "AoS sum");
  require(sum_soa(soa.data(), soa.size()) == 500500, "SoA sum");
  for (std::size_t n : {1, 31, 32, 33, 65}) {
    std::vector<std::uint32_t> a(n*n), b(n*n), c(n*n);
    std::iota(a.begin(), a.end(), 0u);
    transpose_naive(a.data(), b.data(), n);
    transpose_tiled(a.data(), c.data(), n);
    for (std::size_t i = 0; i < n; ++i)
      for (std::size_t j = 0; j < n; ++j)
        require(b[j*n+i] == a[i*n+j] && c[j*n+i] == a[i*n+j], "transpose result/tail");
  }
  std::vector<std::byte> pages(100, std::byte{0});
  touch_pages(pages.data(), pages.size(), 7);
  for (std::size_t i = 0; i < pages.size(); ++i)
    require(pages[i] == (i % 7 == 0 ? std::byte{1} : std::byte{0}), "page stores");
}

void chains() {
  if (!selected("chase1") && !selected("chase8")) return;
  std::mt19937_64 random(20261018);
  for (std::size_t bytes : {4096, 32768, 262144, 524288, 2*1024*1024,
                            8*1024*1024, 32*1024*1024, 64*1024*1024}) {
    const std::size_t n = bytes / sizeof(Node);
    auto nodes = std::make_unique<Node[]>(n);
    std::vector<std::size_t> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), random);
    for (std::size_t i = 0; i < n; ++i) nodes[order[i]].next = &nodes[order[(i+1)%n]];
    Node* starts[8];
    for (std::size_t k = 0; k < 8; ++k) starts[k] = &nodes[order[k*n/8]];
    const std::size_t steps = std::max<std::size_t>(n, 65536);
    measure("chase1", bytes, steps, steps * sizeof(Node*), [&] { return chase(starts[0], steps); });
    measure("chase8", bytes, steps, steps * sizeof(Node*), [&] { return chase8(starts, steps/8); });
  }
}

void layouts() {
  if (!selected("aos") && !selected("soa")) return;
  constexpr std::size_t n = MiB;
  std::vector<Record> aos(n);
  std::vector<std::uint64_t> soa(n);
  for (std::size_t i = 0; i < n; ++i) aos[i].value = soa[i] = i;
  measure("aos", n*sizeof(Record), n, n*sizeof(std::uint64_t), [&] { return sum_aos(aos.data(), n); });
  measure("soa", n*sizeof(std::uint64_t), n, n*sizeof(std::uint64_t), [&] { return sum_soa(soa.data(), n); });
}

void transposes() {
  if (!selected("transpose_naive") && !selected("transpose_tiled")) return;
  constexpr std::size_t n = 2048;
  std::vector<std::uint32_t> src(n*n), dst(n*n);
  std::iota(src.begin(), src.end(), 0u);
  const std::size_t traffic = 2*n*n*sizeof(std::uint32_t);
  measure("transpose_naive", traffic, n*n, traffic, [&] {
    transpose_naive(src.data(), dst.data(), n); return dst[n+1];
  });
  measure("transpose_tiled", traffic, n*n, traffic, [&] {
    transpose_tiled(src.data(), dst.data(), n); return dst[n+1];
  });
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = 0; j < n; ++j)
      require(dst[j*n+i] == src[i*n+j], "large transpose result");
}

void faults() {
  if (!selected("first_write") && !selected("resident_write")) return;
  constexpr std::size_t bytes = 64 * MiB;
  const long page_result = sysconf(_SC_PAGESIZE);
  require(page_result > 0, "page size");
  const auto page = static_cast<std::size_t>(page_result);
  for (int rep = 0; rep < repetitions; ++rep) {
    void* address = mmap(nullptr, bytes, PROT_READ|PROT_WRITE,
                         MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    require(address != MAP_FAILED, "mmap failed");
    require(madvise(address, bytes, MADV_NOHUGEPAGE) == 0, "MADV_NOHUGEPAGE failed");
    auto data = static_cast<volatile std::byte*>(address);
    for (const std::string name : {"first_write", "resident_write"}) {
      // Always execute the first pass even when only resident_write is selected.
      const long before_faults = minor_faults();
      const auto start = Clock::now();
      touch_pages(data, bytes, page);
      const double seconds = std::chrono::duration<double>(Clock::now() - start).count();
      const long fault_count = minor_faults() - before_faults;
      if (selected(name)) row(name, bytes, rep, 1, seconds, bytes/page, bytes/page, fault_count);
    }
    require(munmap(address, bytes) == 0, "munmap failed");
  }
}

int main(int argc, char** argv) {
  try {
    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg == "--check") { check(); std::cout << "All kernel checks passed\n"; return 0; }
      if (arg == "--filter" && i+1 < argc) filter = argv[++i];
      else if (arg == "--repetitions" && i+1 < argc) repetitions = std::stoi(argv[++i]);
      else if (arg == "--min-ms" && i+1 < argc) min_seconds = std::stod(argv[++i]) / 1000;
      else throw std::runtime_error("Usage: memory-bench [--check] [--filter substring] [--repetitions N] [--min-ms N]");
    }
    require(repetitions > 0 && min_seconds > 0, "positive repetitions/min-ms required");
    check();
    std::cout.precision(12);
    std::cout << "case,footprint_bytes,repetition,iterations,seconds,ns_per_op,useful_bytes_per_iteration,minor_faults\n";
    chains(); layouts(); transposes(); faults();
    std::cerr << "observable checksum: " << sink << '\n';
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n'; return 1;
  }
}
