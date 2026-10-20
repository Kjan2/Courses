#include "kernels.hpp"
#include <algorithm>

std::uintptr_t chase(Node* p, std::size_t steps) {
  for (std::size_t i = 0; i < steps; ++i) p = p->next;
  return reinterpret_cast<std::uintptr_t>(p);
}

std::uintptr_t chase8(Node* const* starts, std::size_t rounds) {
  auto a = starts[0]; auto b = starts[1];
  auto c = starts[2]; auto d = starts[3];
  auto e = starts[4]; auto f = starts[5];
  auto g = starts[6]; auto h = starts[7];
  for (std::size_t i = 0; i < rounds; ++i) {
    a = a->next; b = b->next; c = c->next; d = d->next;
    e = e->next; f = f->next; g = g->next; h = h->next;
  }
  return reinterpret_cast<std::uintptr_t>(a) ^ reinterpret_cast<std::uintptr_t>(b)
       ^ reinterpret_cast<std::uintptr_t>(c) ^ reinterpret_cast<std::uintptr_t>(d)
       ^ reinterpret_cast<std::uintptr_t>(e) ^ reinterpret_cast<std::uintptr_t>(f)
       ^ reinterpret_cast<std::uintptr_t>(g) ^ reinterpret_cast<std::uintptr_t>(h);
}

std::uint64_t sum_aos(const Record* data, std::size_t n) {
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < n; ++i) sum += data[i].value;
  return sum;
}
std::uint64_t sum_soa(const std::uint64_t* data, std::size_t n) {
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < n; ++i) sum += data[i];
  return sum;
}

void transpose_naive(const std::uint32_t* src, std::uint32_t* dst, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = 0; j < n; ++j) dst[j*n + i] = src[i*n + j];
}
void transpose_tiled(const std::uint32_t* src, std::uint32_t* dst, std::size_t n) {
  constexpr std::size_t tile = 32;
  for (std::size_t ii = 0; ii < n; ii += tile)
    for (std::size_t jj = 0; jj < n; jj += tile)
      for (std::size_t i = ii; i < std::min(ii + tile, n); ++i)
        for (std::size_t j = jj; j < std::min(jj + tile, n); ++j)
          dst[j*n + i] = src[i*n + j];
}

void touch_pages(volatile std::byte* data, std::size_t bytes, std::size_t page) {
  // Volatile is intentional here: this experiment measures exactly one store/page.
  for (std::size_t i = 0; i < bytes; i += page) data[i] = std::byte{1};
}
