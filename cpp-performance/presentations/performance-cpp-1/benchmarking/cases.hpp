#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

struct Params {
  std::uint32_t multiplier;
  std::uint32_t addend;
};

void transform_ref(std::uint32_t* output, const std::uint32_t* input,
                   std::size_t size, const Params& params);
void transform_value(std::uint32_t* output, const std::uint32_t* input,
                     std::size_t size, Params params);

std::uint64_t sum_plain(const std::uint32_t* values, std::size_t size);
std::uint64_t sum_prefetch(const std::uint32_t* values, std::size_t size);

void atomic_each(const std::uint32_t* values, std::size_t first,
                 std::size_t last, std::atomic<std::uint64_t>& total);
void atomic_batched(const std::uint32_t* values, std::size_t first,
                    std::size_t last, std::atomic<std::uint64_t>& total);
