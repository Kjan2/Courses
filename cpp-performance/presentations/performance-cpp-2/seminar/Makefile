CMAKE ?= cmake
CMAKE_FLAGS ?=

.PHONY: all asm check
all:
	$(CMAKE) -S . -B build -DCMAKE_BUILD_TYPE=Release $(CMAKE_FLAGS)
	$(CMAKE) --build build --parallel 2

asm: all
	objdump -d -C -Mintel build/bench-scalar > build/scalar.asm
	objdump -d -C -Mintel build/bench-native > build/native.asm

check: all
	bash check.sh
