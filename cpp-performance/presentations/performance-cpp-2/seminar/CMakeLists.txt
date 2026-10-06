cmake_minimum_required(VERSION 3.16)
project(cpu_seminar LANGUAGES CXX)

# Prefer an installed library; otherwise fetch a pinned release into build/.
find_package(benchmark 1.9 QUIET CONFIG)
if(NOT benchmark_FOUND)
  include(FetchContent)
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.4
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(googlebenchmark)
endif()

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  message(FATAL_ERROR "This seminar uses GCC optimization flags; configure with g++.")
endif()

foreach(variant scalar native)
  add_executable(bench-${variant} benchmark.cpp cases.cpp)
  target_compile_features(bench-${variant} PRIVATE cxx_std_20)
  target_compile_definitions(bench-${variant} PRIVATE BUILD_NAME="${variant}")
  target_compile_options(bench-${variant} PRIVATE
    -O3 -g -march=native -fno-omit-frame-pointer -Wall -Wextra -Wpedantic)
  target_link_libraries(bench-${variant} PRIVATE benchmark::benchmark)
  # Separate translation units retain inspectable calls and hot kernel symbols.
  set_target_properties(bench-${variant} PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)
endforeach()

# Restrict teaching flags to kernels, so the benchmark driver stays optimized.
set_property(SOURCE cases.cpp APPEND PROPERTY COMPILE_OPTIONS
  "$<$<STREQUAL:$<TARGET_PROPERTY:NAME>,bench-scalar>:-fno-tree-vectorize;-fno-tree-slp-vectorize;-fno-if-conversion;-fno-if-conversion2>"
  "$<$<STREQUAL:$<TARGET_PROPERTY:NAME>,bench-native>:-fopt-info-vec-optimized=${CMAKE_BINARY_DIR}/vectorization.txt>")
