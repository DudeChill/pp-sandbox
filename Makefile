BUILD_DIR ?= build/release
JOBS ?= 2

.PHONY: all build run test bench deps clean

all: build

build:
	JOBS=$(JOBS) ./build.sh

run: build
	./$(BUILD_DIR)/pp $(ARGS)

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

bench: build
	./$(BUILD_DIR)/pp_bench

deps:
	./scripts/setup_deps.sh

clean:
	rm -rf build
