#pragma once
#include <cstddef>
#include <cstdint>

// One node per 64-byte line on the measured x86 machine.
struct alignas(64) Node {
  Node* next;
  std::byte padding[64 - sizeof(Node*)];
};
struct alignas(64) Record {
  std::uint64_t value;
  std::uint64_t cold[7];
};
static_assert(sizeof(Node) == 64 && sizeof(Record) == 64);

std::uintptr_t chase(Node* start, std::size_t steps);
std::uintptr_t chase8(Node* const* starts, std::size_t rounds);
std::uint64_t sum_aos(const Record* data, std::size_t n);
std::uint64_t sum_soa(const std::uint64_t* data, std::size_t n);
void transpose_naive(const std::uint32_t* src, std::uint32_t* dst, std::size_t n);
void transpose_tiled(const std::uint32_t* src, std::uint32_t* dst, std::size_t n);
void touch_pages(volatile std::byte* data, std::size_t bytes, std::size_t page);
