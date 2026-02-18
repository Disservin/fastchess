# Rust Cargo wrapper Makefile
# This Makefile provides a compatibility layer between the old C++ build system
# and the new Rust cargo-based build system

BINARY_PATH ?= ..
TARGET_DIR := target
RELEASE_DIR := $(TARGET_DIR)/release
DEBUG_DIR := $(TARGET_DIR)/debug

# Binary name
BINARY_NAME := fastchess

.PHONY: all tests coverage clean format tidy scan

all: ## Build the release binary
	@echo "Building release binary with cargo..."
	cargo build --release
	@echo "Copying binary to $(BINARY_PATH)/"
	@cp $(RELEASE_DIR)/$(BINARY_NAME) $(BINARY_PATH)/
	@echo "Done."

tests: ## Build and run tests
	@echo "Running tests with cargo..."
	cargo test --release
	@echo "Done."

coverage: ## Run tests with coverage
	@echo "Running tests with coverage..."
	@which cargo-tarpaulin >/dev/null 2>&1 || (echo "Installing cargo-tarpaulin..." && cargo install cargo-tarpaulin)
	cargo tarpaulin --out Html --output-dir coverage
	@echo "Done. Coverage report in coverage/"

scan: ## Run static analysis (clippy)
	@echo "Running clippy (static analysis)..."
	cargo clippy --all-targets --all-features -- -D warnings
	@echo "Done."

format: ## Format code
	@echo "Formatting code with rustfmt..."
	cargo fmt
	@echo "Done."

tidy: ## Check code with clippy (alias for scan)
	@echo "Running clippy checks..."
	cargo clippy --all-targets --all-features -- -D warnings
	@echo "Done."

clean: ## Clean up build artifacts
	@echo "Cleaning up..."
	cargo clean
	@rm -f $(BINARY_PATH)/$(BINARY_NAME)
	@echo "Done."

help: ## Print this help
	@egrep -h '\s##\s' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m  %-30s\033[0m %s\n", $$1, $$2}'
