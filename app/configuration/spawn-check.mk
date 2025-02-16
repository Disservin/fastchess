HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR := $(shell \
    if $(CXX) ./configuration/spawn.c -o test 2>/dev/null; then echo 1; else echo 0; fi; \
    rm test)

ifeq ($(HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR),0)
HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR_NP := $(shell \
    if $(CXX) ./configuration/spawn_np.c -o test 2>/dev/null; then echo 1; else echo 0; fi; \
    rm test)
endif