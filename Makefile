.PHONY: install
install:
	bash cmd/install

.PHONY: build
build: install
	bash cmd/build

.PHONY: dev
dev:
	docker compose exec vst3-dev zsh

JOBS ?= $(shell nproc)

.PHONY: full
full:
	git submodule update --init --recursive
	cmake -G Ninja -S . -B build
	cmake --build build -j $(JOBS)
	ctest --test-dir build --output-on-failure
