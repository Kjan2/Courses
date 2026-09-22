#include <cstddef>
#include <cstdint>

unsigned affine(unsigned x, unsigned y) {
  return x * 5u + y;
}

std::uint64_t sum(const std::uint32_t* values, std::size_t n) {
  std::uint64_t result = 0;
  for (std::size_t i = 0; i < n; ++i)
    result += values[i];
  return result;
}

std::uint64_t sum_four(const std::uint32_t* values, std::size_t n) {
  std::uint64_t a = 0, b = 0, c = 0, d = 0;
  std::size_t i = 0;
  for (; n - i >= 4; i += 4) {
    a += values[i];
    b += values[i + 1];
    c += values[i + 2];
    d += values[i + 3];
  }
  for (; i < n; ++i)
    a += values[i];
  return a + b + c + d;
}

std::uint32_t mix(std::uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

std::uint64_t sum_active(const std::uint32_t* values,
                         const std::uint8_t* active, std::size_t n) {
  std::uint64_t result = 0;
  for (std::size_t i = 0; i < n; ++i)
    if (active[i])
      result += mix(values[i]);
  return result;
}
