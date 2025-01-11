#include <spawn.h>

int main() { return posix_spawn_file_actions_addchdir_np(0, 0); }