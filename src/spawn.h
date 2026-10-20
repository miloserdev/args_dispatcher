#ifndef __SPAWN_H__
#define __SPAWN_H__

#include <spawn.h>

int spawn()
{
    pid_t pid;
    char *args[] = {
        "ls",
        "-holasi",
        "/",
        0
    };

    posix_spawnp(&pid, args[0], NULL, NULL, args, NULL);
}

#endif