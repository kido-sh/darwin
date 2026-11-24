/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

static int *ran;

static void
on_unload(void)
{
    if (ran != NULL) {
        *ran = 1;
    }
}

void
t_atexit_register(int *flag)
{
    ran = flag;
    atexit(on_unload);
}
