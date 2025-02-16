HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR := $(shell \
    if $(CXX) ./configuration/spawn.c -o test 2>/dev/null && [ -f test ]; then \
        echo 1; rm test; \
    else \
        echo 0; \
    fi)

ifeq ($(HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR),0)
HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR_NP := $(shell \
    if $(CXX) ./configuration/spawn_np.c -o test 2>/dev/null && [ -f test ]; then \
        echo 1; rm test; \
    else \
        echo 0; \
    fi)
endif