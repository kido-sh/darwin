/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dirent.h>
#include <fcntl.h>
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#include <mach-o/ldsyms.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/attr.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <util.h>
#include "check.h"

__attribute__((used, section("__DATA,__kido_marker")))
static const char marker[] = "kido section marker";

struct bulk_entry {
    uint32_t length;
    attribute_set_t returned;
    attrreference_t name;
};

static int
count_bulk(const char *dir, int *names_found)
{
    struct attrlist al = { .bitmapcount = ATTR_BIT_MAP_COUNT,
        .commonattr = ATTR_CMN_RETURNED_ATTRS | ATTR_CMN_NAME };
    char buf[4096];
    int fd = open(dir, O_RDONLY), total = 0, n;

    *names_found = 0;
    if (fd < 0)
        return -1;
    while ((n = getattrlistbulk(fd, &al, buf, sizeof(buf), 0)) > 0) {
        char *p = buf;

        for (int i = 0; i < n; i++) {
            struct bulk_entry *e = (struct bulk_entry *)p;
            const char *name = (const char *)&e->name + e->name.attr_dataoffset;

            if (strncmp(name, "bulk-", 5) == 0)
                (*names_found)++;
            p += e->length;
        }
        total += n;
    }
    close(fd);
    return n < 0 ? -1 : total;
}

int
main(void)
{
    char term[L_ctermid], name[64], buf[64], dir[] = "/tmp/kido-libc.XXXXXX", path[128];
    unsigned long size = 0;
    struct itimerval set = { .it_value = { 30, 0 } }, got;
    struct iovec iov[2];
    int master, fd, status, found;
    char *data;
    pid_t pid;
    ssize_t n;

    data = getsectdata("__DATA", "__kido_marker", &size);
    CHECK(data != NULL && size == sizeof(marker));
    CHECK(data + _dyld_get_image_vmaddr_slide(0) == marker);
    CHECK(getsectbyname("__DATA", "__kido_marker") != NULL);
    CHECK(getsectdata("__DATA", "__kido_nothing", &size) == NULL && size == 0);

    CHECK(ctermid_r(term) == term && strcmp(term, "/dev/tty") == 0);
    CHECK(ctermid_r(NULL) == NULL);

    pid = forkpty(&master, name, NULL, NULL);
    if (pid == 0) {
        printf("child:%d:%d\n", isatty(STDIN_FILENO), getsid(0) == getpid());
        fflush(stdout);
        _exit(0);
    }
    CHECK(pid > 0 && strncmp(name, "/dev/ttys", 9) == 0);
    memset(buf, 0, sizeof(buf));
    for (size_t got_bytes = 0; got_bytes < sizeof(buf) - 1 && strchr(buf, '\n') == NULL;) {
        n = read(master, buf + got_bytes, sizeof(buf) - 1 - got_bytes);
        if (n <= 0)
            break;
        got_bytes += (size_t)n;
    }
    CHECK(strncmp(buf, "child:1:1", 9) == 0);
    CHECK(waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    close(master);

    CHECK(setitimer(ITIMER_REAL, &set, NULL) == 0);
    CHECK(getitimer(ITIMER_REAL, &got) == 0);
    CHECK(got.it_value.tv_sec >= 29 && got.it_value.tv_sec <= 30);
    set.it_value.tv_sec = 0;
    setitimer(ITIMER_REAL, &set, NULL);
    CHECK(getitimer(ITIMER_REAL, &got) == 0 && got.it_value.tv_sec == 0 && got.it_value.tv_usec == 0);

    CHECK(mkdtemp(dir) != NULL);
    snprintf(path, sizeof(path), "%s/bulk-file", dir);
    fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    CHECK(fd >= 0);
    iov[0].iov_base = "hello, ";
    iov[0].iov_len = 7;
    iov[1].iov_base = "world";
    iov[1].iov_len = 5;
    CHECK(pwritev(fd, iov, 2, 10) == 12);
    CHECK(lseek(fd, 0, SEEK_CUR) == 0);
    memset(buf, 0, sizeof(buf));
    iov[0].iov_base = buf;
    iov[0].iov_len = 3;
    iov[1].iov_base = buf + 3;
    iov[1].iov_len = 9;
    CHECK(preadv(fd, iov, 2, 10) == 12 && strcmp(buf, "hello, world") == 0);
    CHECK(lseek(fd, 0, SEEK_CUR) == 0);
    close(fd);
    for (int i = 0; i < 3; i++) {
        snprintf(path, sizeof(path), "%s/bulk-%d", dir, i);
        close(open(path, O_WRONLY | O_CREAT, 0644));
    }
    CHECK(count_bulk(dir, &found) == 4 && found == 4);
    for (int i = 0; i < 3; i++) {
        snprintf(path, sizeof(path), "%s/bulk-%d", dir, i);
        unlink(path);
    }
    snprintf(path, sizeof(path), "%s/bulk-file", dir);
    unlink(path);
    rmdir(dir);

    return checks_done("libc");
}
