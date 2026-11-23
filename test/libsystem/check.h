/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>

static int checks_failed;
static int checks_run;

#define CHECK(cond) do { \
        checks_run++; \
        if (!(cond)) { \
            checks_failed++; \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

static int
checks_done(const char *name)
{
    printf("%s: %d/%d passed\n", name, checks_run - checks_failed, checks_run);
    return checks_failed != 0;
}
