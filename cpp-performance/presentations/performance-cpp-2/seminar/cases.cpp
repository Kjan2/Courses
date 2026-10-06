#include "cases.hpp"

// Unsigned multiplication is associative modulo 2^64. Both versions are exact.
std::uint64_t product_serial(const std::uint32_t* a, std::size_t n) {
  std::uint64_t result = 1;
  for (std::size_t i = 0; i < n; ++i) result *= a[i];
  return result;
}

std::uint64_t product_four(const std::uint32_t* a, std::size_t n) {
  std::uint64_t x = 1, y = 1, z = 1, w = 1;
  std::size_t i = 0;
  for (; n - i >= 4; i += 4) {
    x *= a[i];
    y *= a[i + 1];
    z *= a[i + 2];
    w *= a[i + 3];
  }
  for (; i < n; ++i) x *= a[i];
  return x * y * z * w;
}

static std::uint32_t mix(std::uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

std::uint64_t active_branch(const std::uint32_t* a,
                            const std::uint8_t* active, std::size_t n) {
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < n; ++i)
    if (active[i]) sum += mix(a[i]);
  return sum;
}

std::uint64_t active_eager(const std::uint32_t* a,
                           const std::uint8_t* active, std::size_t n) {
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < n; ++i)
    sum += static_cast<std::uint64_t>(mix(a[i])) * active[i];
  return sum;
}

std::uint64_t chase_one(const Node* nodes, const std::uint32_t* heads,
                         std::size_t n) {
  std::uint64_t sum = 0;
  auto p = heads[0];
  for (std::size_t i = 0; i < n; ++i) {
    sum += nodes[p].value;
    p = nodes[p].next;
  }
  return sum;
}

std::uint64_t chase_four(const Node* nodes, const std::uint32_t* heads,
                          std::size_t n) {
  auto p = heads[0], q = heads[1], r = heads[2], s = heads[3];
  std::uint64_t x = 0, y = 0, z = 0, w = 0;
  for (std::size_t i = 0; i < n / 4; ++i) {
    x += nodes[p].value; p = nodes[p].next;
    y += nodes[q].value; q = nodes[q].next;
    z += nodes[r].value; r = nodes[r].next;
    w += nodes[s].value; s = nodes[s].next;
  }
  // The last partition contains the remainder when n is not divisible by four.
  for (std::size_t i = 0; i < n % 4; ++i) {
    w += nodes[s].value;
    s = nodes[s].next;
  }
  return x + y + z + w;
}

std::uint64_t packed_sum(const std::uint32_t* a, std::size_t n) {
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < n; ++i) sum += a[i];
  return sum;
}

void transform(std::uint32_t* output, const std::uint32_t* input, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i) output[i] = input[i] * 5u + 3u;
}
