ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

all: fetch-subs ## Build the project
	@echo "Building.."
	$(MAKE) -C app BINARY_PATH=$(ROOT_DIR)
	@echo "Done."

fetch-subs: ## Fetch submodules
	@echo "Fetching submodules.."
	git submodule init
	git submodule update
	@echo "Done."

.PHONY: all fetch-subs

update-man: man ## Update man like page
	xxd -i man | sed 's/unsigned char/inline char/g' | sed 's/unsigned int/inline unsigned int/g' > temp.hpp
	printf '/* Generate with make update-man*/\n#pragma once\n' > ./app/src/cli/man.hpp
	echo 'namespace fast_chess::man {' >> ./app/src/cli/man.hpp
	cat temp.hpp >> ./app/src/cli/man.hpp
	echo '}' >> ./app/src/cli/man.hpp
	rm temp.hpp
	clang-format -i ./app/src/cli/man.hpp

tests: ## Run tests
	@echo "Running tests.."
	$(MAKE) -C app tests
	@echo "Done."

format: ## Format code
	@echo "Formatting.."
	$(MAKE) -C app format
	@echo "Done."

clean: ## Clean up
	@echo "Cleaning up.."
	$(MAKE) -C app clean
	@echo "Done."

help:
	@egrep -h '\s##\s' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m  %-30s\033[0m %s\n", $$1, $$2}'