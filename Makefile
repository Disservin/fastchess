all: ## Build the project
	@echo "Building.."
	$(MAKE) -C app BINARY_PATH=../
	@echo "Done."

update-fmt: ## Fetch subtree fmt
	@echo "Updating fmt.."
	git fetch https://github.com/fmtlib/fmt.git master
	git subtree pull --prefix=app/third_party/fmt https://github.com/fmtlib/fmt.git master --squash
	@echo "Done."

.PHONY:

update-man: man ## Update man like page
	xxd -i man | sed 's/unsigned char/inline char/g' | sed 's/unsigned int/inline unsigned int/g' > temp.hpp
	printf '/* Generate with make update-man*/\n#pragma once\n' > ./app/src/cli/man.hpp
	echo 'namespace fastchess::man {' >> ./app/src/cli/man.hpp
	cat temp.hpp >> ./app/src/cli/man.hpp
	echo '}' >> ./app/src/cli/man.hpp
	rm temp.hpp
	clang-format -i ./app/src/cli/man.hpp

tests: ## Run tests
	@echo "Running tests.."
	$(MAKE) -C app tests BINARY_PATH=../
	@echo "Done."

format: ## Format code
	@echo "Formatting.."
	$(MAKE) -C app format
	@echo "Done."

clean: ## Clean up
	@echo "Cleaning up.."
	$(MAKE) -C app clean BINARY_PATH=../
	@echo "Done."

help: ## Print this help
	@egrep -h '\s##\s' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m  %-30s\033[0m %s\n", $$1, $$2}'