#pragma once

#include <cstddef>
#include <cstdint>

// One teaching node per 64-byte slot; check your CPU's actual cache-line size.
struct alignas(64) Node {
  std::uint32_t next;
  std::uint32_t value;
};
static_assert(sizeof(Node) == 64);

std::uint64_t product_serial(const std::uint32_t*, std::size_t);
std::uint64_t product_four(const std::uint32_t*, std::size_t);
std::uint64_t active_branch(const std::uint32_t*, const std::uint8_t*, std::size_t);
std::uint64_t active_eager(const std::uint32_t*, const std::uint8_t*, std::size_t);
std::uint64_t chase_one(const Node*, const std::uint32_t*, std::size_t);
std::uint64_t chase_four(const Node*, const std::uint32_t*, std::size_t);
std::uint64_t packed_sum(const std::uint32_t*, std::size_t);
void transform(std::uint32_t*, const std::uint32_t*, std::size_t);
