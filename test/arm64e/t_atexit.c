/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dlfcn.h>
#include <stdio.h>

#include "../libsystem/check.h"

static int ran;

int
main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    CHECK(argc == 2);
    void *h = argc == 2 ? dlopen(argv[1], RTLD_NOW) : NULL;
    CHECK(h != NULL);
    if (h != NULL) {
        void (*reg)(int *) = (void (*)(int *))dlsym(h, "t_atexit_register");
        CHECK(reg != NULL);
        if (reg != NULL) {
            reg(&ran);
        }
        CHECK(ran == 0);
        CHECK(dlclose(h) == 0);
    }
    CHECK(ran == 1);
#if __arm64e__
    return checks_done("atexit in a dlclosed dylib arm64e");
#else
    return checks_done("atexit in a dlclosed dylib arm64");
#endif
}
