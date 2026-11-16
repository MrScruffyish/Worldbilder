BUILD_DIR ?= build
CMAKE ?= cmake
JOBS ?= $(shell nproc 2>/dev/null || echo 2)

.PHONY: all configure build run clean

all: build

configure:
	$(CMAKE) -S . -B $(BUILD_DIR)

build:
	$(CMAKE) -S . -B $(BUILD_DIR)
	$(CMAKE) --build $(BUILD_DIR) -j$(JOBS)

run: build
	./$(BUILD_DIR)/worldbilder $(ARGS)

clean:
	$(CMAKE) --build $(BUILD_DIR) --target clean
