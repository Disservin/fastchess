PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man/man1
MANPAGE_SOURCE = man.md  # Path to the markdown file for the man page
MANPAGE_OUTPUT = fastchess.1

ifeq ($(shell uname), Linux)
	LOWDOWN_INSTALLED := $(shell command -v lowdown >/dev/null 2>&1 && echo yes || echo no)
endif

all: ## Build the project
	@echo "Building.."
	$(MAKE) -C app BINARY_PATH=../
	@echo "Done."

tests: ## Run tests
	@echo "Running tests.."
	$(MAKE) -C app tests BINARY_PATH=../
	@echo "Done."

coverage: ## Run tests with coverage
	@echo "Running tests with coverage.."
	$(MAKE) -C app coverage BINARY_PATH=../
	@echo "Done."

scan:
	@echo "Running scan-build.."
	$(MAKE) -C app scan=yes BINARY_PATH=../
	@echo "Done."

install: ## Install the binary and man page
	@echo "Installing fastchess binary to: $(BINDIR)"
	@install -d $(BINDIR)
	@install fastchess $(BINDIR)
	@if [ "$(shell uname)" = "Linux" ] && [ "$(LOWDOWN_INSTALLED)" = "no" ]; then \
		echo "\033[33mWarning: 'lowdown' is not installed. Man page will not be generated.\033[0m"; \
	else \
		make install-manpage --no-print-directory ; \
	fi
	@echo "Done."

manpage: ## Generate the man page from Markdown using lowdown
	@lowdown -s -Tman -o  $(MANPAGE_OUTPUT) $(MANPAGE_SOURCE) -mtitle="Fastchess"

install-manpage: manpage ## Install the man page to $(MANDIR)
	@install -d $(MANDIR)
	@install -m 644 $(MANPAGE_OUTPUT) $(MANDIR)
	@echo "Installed man page."

update-man: ## Update man like page
	lowdown -Tterm man.md > fastchess-tmp-man-page
	xxd -i fastchess-tmp-man-page | sed 's/fastchess_tmp_man_page/man/g' | sed 's/fastchess_tmp_man_page_len/man_len/g' | sed 's/unsigned char/inline unsigned char/g' | sed 's/unsigned int/inline unsigned int/g' > temp.hpp
	printf '/* Generate with make update-man*/\n#pragma once\n' > ./app/src/cli/man.hpp
	echo 'namespace fastchess::man {' >> ./app/src/cli/man.hpp
	cat temp.hpp >> ./app/src/cli/man.hpp
	echo '}' >> ./app/src/cli/man.hpp
	rm temp.hpp
	clang-format -i ./app/src/cli/man.hpp
	rm fastchess-tmp-man-page

update-fmt: ## Fetch subtree fmt
	@echo "Updating fmt.."
	git fetch https://github.com/fmtlib/fmt.git master
	git subtree pull --prefix=app/third_party/fmt https://github.com/fmtlib/fmt.git master --squash
	@echo "Done."

format: ## Format code
	@echo "Formatting.."
	$(MAKE) -C app format
	@echo "Done."

tidy: ## Check clang-tidy
	@echo "Checking includes.."
	$(MAKE) -C app tidy
	@echo "Done."

clean: ## Clean up
	@echo "Cleaning up.."
	$(MAKE) -C app clean BINARY_PATH=../
	@echo "Done."

help: ## Print this help
	@egrep -h '\s##\s' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m  %-30s\033[0m %s\n", $$1, $$2}'

.PHONY: all tests install manpage install-manpage update-man clean format update-fmt help coverage
