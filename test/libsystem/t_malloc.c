/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dlfcn.h>
#include <malloc/malloc.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"

static int
zeroed(const unsigned char *p, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (p[i] != 0) {
            return 0;
        }
    }
    return 1;
}

int
main(void)
{
    malloc_zone_t *zone = malloc_default_zone();

    unsigned char *p = malloc_type_malloc(100, 0x1234);
    CHECK(p != NULL);
    memset(p, 0xff, 100);
    p = malloc_type_realloc(p, 4000, 0x1234);
    CHECK(p != NULL && p[99] == 0xff);
    malloc_type_free(p, 0x1234);
    p = malloc_type_calloc(10, 10, 0x1234);
    CHECK(p != NULL && zeroed(p, 100));
    malloc_type_free(p, 0x1234);
    p = malloc_type_zone_malloc(zone, 64, 0x1234);
    CHECK(p != NULL);
    malloc_type_zone_free(zone, p, 0x1234);

    p = malloc_zone_malloc_with_options(NULL, 64, 256, MALLOC_ZONE_MALLOC_OPTION_CLEAR);
    CHECK(p != NULL && (uintptr_t)p % 64 == 0 && zeroed(p, 256));
    free(p);
    p = malloc_zone_malloc_with_options(zone, 8, 48, MALLOC_ZONE_MALLOC_OPTION_NONE);
    CHECK(p != NULL);
    free(p);
    p = malloc_type_zone_malloc_with_options(zone, 4096, 512, 0x1234, MALLOC_ZONE_MALLOC_OPTION_CLEAR);
    CHECK(p != NULL && (uintptr_t)p % 4096 == 0 && zeroed(p, 512));
    free(p);
    CHECK(dlsym(RTLD_DEFAULT, "malloc_type_zone_malloc_with_options_internal") != NULL);
    return checks_done("typed malloc");
}
