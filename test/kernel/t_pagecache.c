/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysctl.h>
#include <unistd.h>

#define BLOCK 4096

static void
fill(uint64_t *w, uint64_t off)
{
    for (size_t i = 0; i < BLOCK / sizeof(*w); i++) {
        w[i] = (off + i * sizeof(*w)) ^ 0x6b69646f6b69646fULL;
    }
}

int
main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "/var/root/t_pagecache.bin";
    uint64_t mem = 0;
    size_t len = sizeof(mem);
    if (sysctlbyname("hw.memsize", &mem, &len, NULL, 0) != 0 || mem == 0) {
        printf("FAIL hw.memsize: %s\n", strerror(errno));
        return 1;
    }
    uint64_t total = (mem + mem / 2) & ~(uint64_t)(BLOCK - 1);
    int fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0600);
    if (fd < 0) {
        printf("FAIL open %s: %s\n", path, strerror(errno));
        return 1;
    }
    uint64_t buf[BLOCK / sizeof(uint64_t)], want[BLOCK / sizeof(uint64_t)];
    for (uint64_t off = 0; off < total; off += BLOCK) {
        fill(buf, off);
        if (write(fd, buf, BLOCK) != BLOCK) {
            printf("FAIL write at %" PRIu64 ": %s\n", off, strerror(errno));
            return 1;
        }
    }
    if (lseek(fd, 0, SEEK_SET) != 0) {
        printf("FAIL lseek: %s\n", strerror(errno));
        return 1;
    }
    uint64_t bad = 0;
    for (uint64_t off = 0; off < total; off += BLOCK) {
        if (read(fd, buf, BLOCK) != BLOCK) {
            printf("FAIL read at %" PRIu64 ": %s\n", off, strerror(errno));
            return 1;
        }
        fill(want, off);
        bad += memcmp(buf, want, BLOCK) != 0;
    }
    close(fd);
    unlink(path);
    if (bad != 0) {
        printf("FAIL pagecache: %" PRIu64 " of %" PRIu64 " blocks read back wrong\n", bad, total / BLOCK);
        return 1;
    }
    printf("pagecache: wrote and read back %" PRIu64 " MiB, 1.5 times hw.memsize, in %d-byte calls\n",
        total >> 20, BLOCK);
    return 0;
}
