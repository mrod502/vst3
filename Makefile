# Build trees live in $(BUILD_ROOT)/<config>: build/release on the host, /ct/build/release in
# the container, where compose points BUILD_ROOT at the build volume. BUILD_TYPE is the CMake
# spelling and the knob; the directory name is its lowercase form.
BUILD_TYPE ?= Release
CONFIG     ?= $(shell printf '%s' '$(BUILD_TYPE)' | tr '[:upper:]' '[:lower:]')
BUILD_ROOT ?= build
BUILD      := $(BUILD_ROOT)/$(CONFIG)

.PHONY: install
install:
	bash cmd/install $(CONFIG)

.PHONY: build
build: install
	bash cmd/build $(CONFIG)

.PHONY: dev
dev:
	docker compose exec vst3-dev zsh

JOBS ?= $(shell nproc)

.PHONY: full
full:
	git submodule update --init --recursive
	cmake -G Ninja -S . -B $(BUILD) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	cmake --build $(BUILD) -j $(JOBS)
	ctest --test-dir $(BUILD) --output-on-failure
