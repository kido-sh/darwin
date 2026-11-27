/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--wait") == 0) {
        pause();
        return 0;
    }
    printf("injected=%d env=%d\n",
        getenv("KIDO_INJECTED") != NULL ? 1 : 0,
        getenv("DYLD_INSERT_LIBRARIES") != NULL ? 1 : 0);
    return 0;
}
