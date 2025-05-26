.PHONY: install
install:
	bash cmd/install

.PHONY: build
build: install
	bash cmd/build
