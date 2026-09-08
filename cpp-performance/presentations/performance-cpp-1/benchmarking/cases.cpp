#include "cases.hpp"

void transform_ref(std::uint32_t* output, const std::uint32_t* input,
                   std::size_t size, const Params& params) {
  for (std::size_t i = 0; i < size; ++i)
    output[i] = input[i] * params.multiplier + params.addend;
}

void transform_value(std::uint32_t* output, const std::uint32_t* input,
                     std::size_t size, Params params) {
  for (std::size_t i = 0; i < size; ++i)
    output[i] = input[i] * params.multiplier + params.addend;
}

std::uint64_t sum_plain(const std::uint32_t* values, std::size_t size) {
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < size; ++i) sum += values[i];
  return sum;
}

std::uint64_t sum_prefetch(const std::uint32_t* values, std::size_t size) {
  std::uint64_t sum = 0;
  std::size_t i = 0;
  for (; i + 64 < size; ++i) {
    __builtin_prefetch(values + i + 64);
    sum += values[i];
  }
  for (; i < size; ++i) sum += values[i];
  return sum;
}

void atomic_each(const std::uint32_t* values, std::size_t first,
                 std::size_t last, std::atomic<std::uint64_t>& total) {
  for (std::size_t i = first; i < last; ++i)
    total.fetch_add(values[i], std::memory_order_relaxed);
}

void atomic_batched(const std::uint32_t* values, std::size_t first,
                    std::size_t last, std::atomic<std::uint64_t>& total) {
  std::uint64_t local = 0;
  for (std::size_t i = first; i < last; ++i) local += values[i];
  total.fetch_add(local, std::memory_order_relaxed);
}
