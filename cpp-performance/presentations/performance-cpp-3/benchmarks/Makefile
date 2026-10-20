CXX = g++
CXXFLAGS = -O3 -g -std=c++20 -march=native -fno-omit-frame-pointer -Wall -Wextra -Wpedantic

.PHONY: all check clean
all: build/memory-bench

build/memory-bench: benchmark.cpp kernels.cpp kernels.hpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) benchmark.cpp kernels.cpp -o $@

check: build/memory-bench
	./build/memory-bench --check

clean:
	$(RM) build/memory-bench
