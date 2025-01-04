CXX := g++

HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR := $(shell \
    echo \#include \<spawn.h\> > test.c && \
    echo 'int main(){return posix_spawn_file_actions_addchdir(0,0);}' >> test.c && \
    if $(CXX) test.c -o test 2>/dev/null; then echo 1; else echo 0; fi; \
    rm -f test.c test)

ifeq ($(HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR),0)
HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR_NP := $(shell \
    echo \#include \<spawn.h\> > test.c && \
    echo 'int main(){return posix_spawn_file_actions_addchdir_np(0,0);}' >> test.c && \
    if $(CXX) test.c -o test 2>/dev/null; then echo 1; else echo 0; fi; \
    rm -f test.c test)
endif