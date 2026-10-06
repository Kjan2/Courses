#include "cases.hpp"

#include <benchmark/benchmark.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifndef BUILD_NAME
#define BUILD_NAME "custom"
#endif

static bool failed = false;

static bool check(benchmark::State& state, bool correct, const char* message) {
  if (!correct) {
    state.SkipWithError(message);
    failed = true;
  }
  return correct;
}

// This only controls the profiler. Google Benchmark owns all timing,
// iteration selection, repetitions, statistics, and output.
static void perf_control(const char* command) {
  const char* control_path = std::getenv("SEMINAR_PERF_CONTROL");
  if (!control_path) return;
  const char* ack_path = std::getenv("SEMINAR_PERF_ACK");
  if (!ack_path) throw std::runtime_error("SEMINAR_PERF_ACK is missing");
  std::ofstream control(control_path);
  std::ifstream acknowledgement(ack_path);
  if (!control || !acknowledgement) throw std::runtime_error("cannot open perf FIFOs");
  control << command << '\n' << std::flush;
  std::string reply;
  if (!std::getline(acknowledgement, reply) || reply != "ack")
    throw std::runtime_error("perf did not acknowledge the control command");
}

static std::vector<std::uint32_t> make_values(std::size_t n) {
  std::mt19937 random(42);
  std::vector<std::uint32_t> values(n);
  for (auto& value : values) value = random() | 1u;
  return values;
}

using Product = decltype(&product_serial);
using Active = decltype(&active_branch);
using Chase = decltype(&chase_one);

enum class Pattern { Random, Blocked, Alternating, Rare, Always };

static void dependency(benchmark::State& state, Product kernel) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const auto values = make_values(n);
  std::uint64_t expected = 1;
  for (auto value : values) expected *= value;
  // Setup and checks before the state loop are excluded from Google's timing.
  if (!check(state, kernel(values.data(), n) == expected, "incorrect product")) return;
  perf_control("enable");
  for (auto _ : state) {
    auto result = kernel(values.data(), n);
    benchmark::DoNotOptimize(result);
  }
  perf_control("disable");
  check(state, kernel(values.data(), n) == expected, "incorrect product after timing");
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

static void branch(benchmark::State& state, Active kernel, Pattern pattern) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const auto values = make_values(n);
  std::vector<std::uint8_t> active(n, 0);
  std::mt19937 random(43);
  if (pattern == Pattern::Alternating) {
    for (std::size_t i = 1; i < n; i += 2) active[i] = 1;
  } else if (pattern == Pattern::Always) {
    std::fill(active.begin(), active.end(), 1);
  } else {
    // Exactly floor(n/2) active for random/blocked/alternating, floor(n/100)
    // for rare. Shuffle avoids periodic low PRNG bits posing as random flags.
    std::fill_n(active.begin(), pattern == Pattern::Rare ? n / 100 : n / 2, 1);
    if (pattern != Pattern::Blocked) std::shuffle(active.begin(), active.end(), random);
  }
  const auto expected = active_branch(values.data(), active.data(), n);
  if (!check(state, active_eager(values.data(), active.data(), n) == expected,
             "branch variants disagree")) return;
  if (!check(state, kernel(values.data(), active.data(), n) == expected,
             "incorrect selected branch kernel")) return;
  perf_control("enable");
  for (auto _ : state) {
    auto result = kernel(values.data(), active.data(), n);
    benchmark::DoNotOptimize(result);
  }
  perf_control("disable");
  check(state, kernel(values.data(), active.data(), n) == expected,
        "incorrect branch result after timing");
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

static void memory(benchmark::State& state, Chase kernel) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const auto values = make_values(n);
  std::vector<Node> nodes(n);
  std::vector<std::uint32_t> order(n);
  std::iota(order.begin(), order.end(), 0u);
  std::mt19937 random(43);
  std::shuffle(order.begin(), order.end(), random);
  for (std::size_t i = 0; i < n; ++i) {
    nodes[order[i]].next = order[(i + 1) % n];
    nodes[order[i]].value = values[i];
  }
  const std::array<std::uint32_t, 4> heads{
    order[0], order[n / 4], order[2 * (n / 4)], order[3 * (n / 4)]};
  const auto expected = std::accumulate(values.begin(), values.end(), std::uint64_t{0});
  if (!check(state, chase_one(nodes.data(), heads.data(), n) == expected &&
                    chase_four(nodes.data(), heads.data(), n) == expected &&
                    packed_sum(values.data(), n) == expected,
             "memory variants disagree")) return;
  auto warm = kernel ? kernel(nodes.data(), heads.data(), n) : packed_sum(values.data(), n);
  benchmark::DoNotOptimize(warm);
  perf_control("enable");
  for (auto _ : state) {
    auto result = kernel ? kernel(nodes.data(), heads.data(), n) : packed_sum(values.data(), n);
    benchmark::DoNotOptimize(result);
  }
  perf_control("disable");
  check(state, (kernel ? kernel(nodes.data(), heads.data(), n) : packed_sum(values.data(), n))
                == expected, "incorrect memory result after timing");
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

static void simd(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const auto values = make_values(n);
  std::vector<std::uint32_t> output(n);
  const auto correct = [&] {
    for (std::size_t i = 0; i < n; ++i)
      if (output[i] != values[i] * 5u + 3u) return false;
    return true;
  };
  transform(output.data(), values.data(), n);
  if (!check(state, correct(), "incorrect transform")) return;
  benchmark::DoNotOptimize(output.data());
  perf_control("enable");
  for (auto _ : state) {
    transform(output.data(), values.data(), n);
    benchmark::ClobberMemory();
  }
  perf_control("disable");
  check(state, correct(), "incorrect transform after timing");
  state.SetItemsProcessed(state.iterations() * state.range(0));
  state.SetBytesProcessed(state.iterations() * state.range(0) * 8);
}

// Small sizes exercise correctness and tails; the larger arguments are the labs.
static void check_sizes(benchmark::internal::Benchmark* benchmark) {
  for (auto n : {4, 5, 7, 31, 4097}) benchmark->Arg(n);
}

int main(int argc, char** argv) try {
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
  benchmark::AddCustomContext("build", BUILD_NAME);
  for (auto [name, kernel] : {std::pair{"serial", &product_serial},
                             std::pair{"four", &product_four}}) {
    benchmark::RegisterBenchmark((std::string("dependency/") + name).c_str(), dependency, kernel)
      ->Apply(check_sizes)->Arg(4096)->Arg(65536);
  }
  for (auto [name, kernel] : {std::pair{"branch", &active_branch},
                             std::pair{"eager", &active_eager}}) {
    for (auto [pattern_name, pattern] : {std::pair{"random", Pattern::Random},
                                        std::pair{"blocked", Pattern::Blocked},
                                        std::pair{"alternating", Pattern::Alternating},
                                        std::pair{"rare", Pattern::Rare},
                                        std::pair{"always", Pattern::Always}}) {
      benchmark::RegisterBenchmark((std::string("branch/") + name + "/" + pattern_name).c_str(),
                                   branch, kernel, pattern)
        ->Apply(check_sizes)->Arg(4096)->Arg(1048576);
    }
  }
  for (auto [name, kernel] : {std::pair{"one", &chase_one}, std::pair{"four", &chase_four},
                             std::pair{"packed", static_cast<Chase>(nullptr)}}) {
    benchmark::RegisterBenchmark((std::string("memory/") + name).c_str(), memory, kernel)
      ->Apply(check_sizes)->Arg(256)->Arg(4096)->Arg(65536)->Arg(1048576);
  }
  benchmark::RegisterBenchmark("simd/transform", simd)
    ->Apply(check_sizes)->Arg(2048)->Arg(4096)->Arg(16777216);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return failed ? 1 : 0;
} catch (const std::exception& error) {
  std::cerr << "Error: " << error.what() << '\n';
  return 1;
}
