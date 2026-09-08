#include "cases.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <numeric>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

using Clock = std::chrono::steady_clock;

template<class First, class Second>
std::pair<double, double> median_pair(First first, Second second) {
  std::array<double, 11> first_samples{}, second_samples{};
  first();
  second();
  auto time = [](auto run) {
    const auto start = Clock::now();
    run();
    return std::chrono::duration<double, std::nano>(Clock::now() - start).count();
  };
  for (std::size_t i = 0; i < first_samples.size(); ++i) {
    if (i % 2 == 0) {
      first_samples[i] = time(first);
      second_samples[i] = time(second);
    } else {
      second_samples[i] = time(second);
      first_samples[i] = time(first);
    }
  }
  std::sort(first_samples.begin(), first_samples.end());
  std::sort(second_samples.begin(), second_samples.end());
  return {first_samples[first_samples.size() / 2],
          second_samples[second_samples.size() / 2]};
}

void parameters() {
  std::cout << "example,size,variant,median_ns\n";
  for (std::size_t size : {16u, 1024u, 1048576u}) {
    std::vector<std::uint32_t> input(size), output(size);
    std::iota(input.begin(), input.end(), 1u);
    const Params params{17, 23};
    const std::size_t repetitions = std::max<std::size_t>(1, 33554432 / size);
    auto ref = [&] {
      for (std::size_t i = 0; i < repetitions; ++i)
        transform_ref(output.data(), input.data(), size, params);
    };
    auto value = [&] {
      for (std::size_t i = 0; i < repetitions; ++i)
        transform_value(output.data(), input.data(), size, params);
    };
    const auto [ref_total, value_total] = median_pair(ref, value);
    for (std::size_t i = 0; i < size; ++i)
      if (output[i] != input[i] * params.multiplier + params.addend)
        std::exit(2);
    std::cout << "parameters," << size << ",const_ref,"
              << ref_total / repetitions << '\n';
    std::cout << "parameters," << size << ",value,"
              << value_total / repetitions << '\n';
  }
}

void prefetch() {
  std::cout << "example,size,variant,median_ns\n";
  for (std::size_t size : {4096u, 1048576u, 16777216u}) {
    std::vector<std::uint32_t> values(size);
    std::uint32_t state = 42;
    for (auto& value : values) {
      state = state * 1664525u + 1013904223u;
      value = state;
    }
    const auto expected = sum_plain(values.data(), values.size());
    const std::size_t repetitions = std::max<std::size_t>(1, 67108864 / size);
    std::uint64_t result = 0;
    auto plain = [&] {
      for (std::size_t i = 0; i < repetitions; ++i)
        result ^= sum_plain(values.data(), values.size());
    };
    auto manual = [&] {
      for (std::size_t i = 0; i < repetitions; ++i)
        result ^= sum_prefetch(values.data(), values.size());
    };
    const auto [plain_total, manual_total] = median_pair(plain, manual);
    if (sum_prefetch(values.data(), values.size()) != expected) std::exit(3);
    std::cout << "prefetch," << size << ",plain,"
              << plain_total / repetitions << '\n';
    std::cout << "prefetch," << size << ",manual_64,"
              << manual_total / repetitions << '\n';
    if (result == 0xdeadbeef) std::cerr << result;
  }
}

void atomics() {
  std::cout << "example,threads,variant,median_ns\n";
  constexpr std::size_t size = 1048576;
  std::vector<std::uint32_t> values(size);
  std::uint32_t state = 42;
  for (auto& value : values) {
    state = state * 1664525u + 1013904223u;
    value = state & 1u;
  }
  const auto expected = std::accumulate(values.begin(), values.end(), std::uint64_t{0});
  for (unsigned count : {1u, 2u, 4u, 8u}) {
    auto measure = [&](auto function) {
      std::atomic<std::uint64_t> total{0};
      std::vector<std::thread> threads;
      threads.reserve(count);
      for (unsigned t = 0; t < count; ++t)
        threads.emplace_back(function, values.data(), size * t / count,
                             size * (t + 1) / count, std::ref(total));
      for (auto& thread : threads) thread.join();
      if (total.load() != expected) std::exit(4);
    };
    const auto [each_ns, batched_ns] = median_pair(
        [&] { measure(atomic_each); }, [&] { measure(atomic_batched); });
    std::cout << "atomic," << count << ",each," << each_ns << '\n';
    std::cout << "atomic," << count << ",batched," << batched_ns << '\n';
  }
}

int main(int argc, char** argv) {
  if (argc != 2) return 1;
  const std::string_view example = argv[1];
  if (example == "parameters") parameters();
  else if (example == "prefetch") prefetch();
  else if (example == "atomic") atomics();
  else return 1;
}
