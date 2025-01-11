#include <spawn.h>

int main() { return posix_spawn_file_actions_addchdir(0, 0); }