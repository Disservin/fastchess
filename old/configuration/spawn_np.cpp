#include <spawn.h>

int main() {
    posix_spawn_file_actions_t file_actions;
    posix_spawn_file_actions_init(&file_actions);

    return posix_spawn_file_actions_addchdir_np(&file_actions, "/some/directory");
}