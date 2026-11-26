/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: flush_volume PATH\n");
        return 2;
    }
    int rc = sync_volume_np(argv[1], SYNC_VOLUME_FULLSYNC | SYNC_VOLUME_WAIT);
    if (rc != 0) {
        fprintf(stderr, "flush_volume: sync_volume_np %s: %s\n", argv[1], strerror(rc));
        return 1;
    }
    return 0;
}
