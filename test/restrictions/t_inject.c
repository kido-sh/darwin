/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

__attribute__((constructor))
static void
kido_inject_init(void)
{
    setenv("KIDO_INJECTED", "1", 1);
}
