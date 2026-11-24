/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../libsystem/check.h"

#define HN_DECIMAL 0x01
#define HN_NOSPACE 0x02
#define HN_B 0x04
#define HN_AUTOSCALE 0x20

struct mntopt {
    const char *m_option;
    int m_inverse;
    int m_flag;
    int m_altloc;
};
typedef struct mntoptparse *mntoptparse_t;
typedef struct __wipefs_ctx *wipefs_ctx;
struct pidfh;

int humanize_number(char *, size_t, int64_t, const char *, int, int);
int expand_number(const char *, uint64_t *);
mntoptparse_t getmntopts(const char *, const struct mntopt *, int *, int *);
const char *getmntoptstr(mntoptparse_t, const char *);
long getmntoptnum(mntoptparse_t, const char *);
void freemntopts(mntoptparse_t);
extern int getmnt_silent;
struct pidfh *pidfile_open(const char *, mode_t, pid_t *);
int pidfile_write(struct pidfh *);
int pidfile_close(struct pidfh *);
int pidfile_remove(struct pidfh *);
int realhostname(char *, size_t, const struct in_addr *);
int realhostname_sa(char *, size_t, struct sockaddr *, int);
int reexec_to_match_kernel(void);
int reexec_to_match_lp64ness(bool);
int tzlink(const char *);
int wipefs_alloc(int, size_t, wipefs_ctx *);
int wipefs_include_blocks(wipefs_ctx, off_t, off_t);
int wipefs_except_blocks(wipefs_ctx, off_t, off_t);
int wipefs_wipe(wipefs_ctx);
void wipefs_free(wipefs_ctx *);

static int
all_bytes(const unsigned char *p, size_t from, size_t to, unsigned char v)
{
    for (size_t i = from; i < to; i++) {
        if (p[i] != v) {
            return 0;
        }
    }
    return 1;
}

static void
check_numbers(void)
{
    char buf[8];
    uint64_t n = 0;

    CHECK(humanize_number(buf, 5, 1572864, "", HN_AUTOSCALE, HN_B | HN_NOSPACE | HN_DECIMAL) > 0);
    CHECK(strcmp(buf, "1.5M") == 0);
    CHECK(expand_number("10k", &n) == 0 && n == 10240);
    CHECK(expand_number("3M", &n) == 0 && n == 3 << 20);
    CHECK(expand_number("2g", &n) == 0 && n == 2ULL << 30);
    CHECK(expand_number("7", &n) == 0 && n == 7);
    CHECK(expand_number("5b", &n) == 0 && n == 5);
    CHECK(expand_number("0x10", &n) == 0 && n == 16);
    CHECK(expand_number("15E", &n) == 0 && n == 15ULL << 60);
    errno = 0;
    CHECK(expand_number("16E", &n) == -1 && errno == ERANGE);
    errno = 0;
    CHECK(expand_number("12q", &n) == -1 && errno == EINVAL);
    errno = 0;
    CHECK(expand_number("k", &n) == -1 && errno == EINVAL);
}

static void
check_mntopts(void)
{
    static const struct mntopt table[] = {
        { "rdonly", 0, 0x1, 0 },
        { "dev", 1, 0x2, 0 },
        { "owners", 1, 0x4, 1 },
        { "size", 0, 0, 0 },
        { NULL, 0, 0, 0 },
    };
    int flags = 0x2, alt = 0;

    mntoptparse_t mp = getmntopts("rdonly,dev,noowners,size=0x40", table, &flags, &alt);
    CHECK(mp != NULL);
    if (mp != NULL) {
        CHECK(flags == 0x1 && alt == 0x4);
        CHECK(strcmp(getmntoptstr(mp, "size"), "0x40") == 0);
        CHECK(getmntoptnum(mp, "size") == 64);
        freemntopts(mp);
    }
    flags = 0;
    mp = getmntopts("nodev", table, &flags, &alt);
    CHECK(mp != NULL && flags == 0x2);
    freemntopts(mp);
    getmnt_silent = 1;
    mp = getmntopts("bogus,rdonly", table, &flags, &alt);
    CHECK(mp != NULL && (flags & 0x1) != 0);
    CHECK(mp != NULL && getmntoptstr(mp, "size") == NULL && getmntoptnum(mp, "size") == -1);
    freemntopts(mp);
    getmnt_silent = 0;
}

static const char *dir = "/var/root";

static void
check_pidfile(void)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/t_util.pid", dir);
    pid_t other = 0;
    char buf[32] = "";

    unlink(path);
    struct pidfh *pfh = pidfile_open(path, 0644, &other);
    CHECK(pfh != NULL);
    CHECK(pidfile_write(pfh) == 0);
    int fd = open(path, O_RDONLY);
    CHECK(fd >= 0 && read(fd, buf, sizeof(buf) - 1) > 0);
    close(fd);
    CHECK(atoi(buf) == getpid());
    pid_t child = fork();
    if (child == 0) {
        pid_t holder = 0;
        struct pidfh *again = pidfile_open(path, 0644, &holder);
        _exit(again == NULL && errno == EEXIST && holder == getppid() ? 0 : 1);
    }
    int st = 0;
    waitpid(child, &st, 0);
    CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 0);
    CHECK(pidfile_remove(pfh) == 0);
    CHECK(access(path, F_OK) != 0 && errno == ENOENT);
    pfh = pidfile_open(path, 0, NULL);
    CHECK(pfh != NULL && pidfile_close(pfh) == 0);
    CHECK(access(path, F_OK) == 0);
    unlink(path);
    errno = 0;
    CHECK(pidfile_write(NULL) == -1 && errno == EINVAL && pidfile_close(NULL) == -1 && pidfile_remove(NULL) == -1);
}

static void
check_hosts(void)
{
    char host[256] = "";
    struct in_addr lo = { htonl(INADDR_LOOPBACK) };
    struct sockaddr_in6 sin6;

    int r = realhostname(host, sizeof(host), &lo);
    printf("realhostname(127.0.0.1) = %d, %s\n", r, host);
    CHECK(r >= 0 && r <= 3 && host[0] != '\0');
    CHECK(r == 0 || strcmp(host, "127.0.0.1") == 0);
    memset(&sin6, 0, sizeof(sin6));
    sin6.sin6_len = sizeof(sin6);
    sin6.sin6_family = AF_INET6;
    inet_pton(AF_INET6, "::ffff:127.0.0.1", &sin6.sin6_addr);
    char mapped[256] = "";
    int rs = realhostname_sa(mapped, sizeof(mapped), (struct sockaddr *)&sin6, sizeof(sin6));
    CHECK(rs != 0 && strcmp(mapped, "::ffff:127.0.0.1") == 0);
}

static void
check_wipefs(void)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/t_util.img", dir);
    const size_t mib = 1024 * 1024, size = 8 * mib;
    unsigned char *data = malloc(size);
    wipefs_ctx w = NULL;

    memset(data, 0xa5, size);
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    CHECK(fd >= 0 && write(fd, data, size) == (ssize_t)size);
    CHECK(wipefs_alloc(fd, 512, &w) == 0 && w != NULL);
    CHECK(wipefs_include_blocks(w, 4 * mib / 512, 8) == 0);
    CHECK(wipefs_except_blocks(w, 0, 1) == 0);
    CHECK(wipefs_wipe(w) == 0);
    wipefs_free(&w);
    CHECK(w == NULL);
    CHECK(pread(fd, data, size, 0) == (ssize_t)size);
    close(fd);
    unlink(path);
    CHECK(all_bytes(data, 0, 512, 0));
    CHECK(all_bytes(data, 512, 2 * mib, 0));
    CHECK(all_bytes(data, 2 * mib, 4 * mib - 32768, 0xa5));
    CHECK(all_bytes(data, 4 * mib, 4 * mib + 4096, 0));
    CHECK(all_bytes(data, 4 * mib + 32768, 6 * mib, 0xa5));
    CHECK(all_bytes(data, 6 * mib, 8 * mib, 0));
    free(data);
}

static const unsigned char utc_tzif[] = {
    'T', 'Z', 'i', 'f', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 4,
    0, 0, 0, 0, 0, 0,
    'U', 'T', 'C', 0,
};

static void
check_tzlink(void)
{
    char before[1024] = "", after[1024] = "";
    const char *utc = "/usr/share/zoneinfo/UTC";

    ssize_t was = readlink("/etc/localtime", before, sizeof(before) - 1);
    CHECK(tzlink(NULL) == EINVAL);
    CHECK(tzlink("/usr/share/zoneinfo/../../../etc/passwd") != 0);
    if (access(utc, R_OK) != 0) {
        mkdir("/usr/share/zoneinfo", 0755);
        int fd = open(utc, O_WRONLY | O_CREAT | O_EXCL, 0644);
        CHECK(fd >= 0 && write(fd, utc_tzif, sizeof(utc_tzif)) == (ssize_t)sizeof(utc_tzif));
        close(fd);
    }
    CHECK(tzlink(utc) != 0);
    ssize_t now = readlink("/etc/localtime", after, sizeof(after) - 1);
    CHECK(now == was && strcmp(before, after) == 0);
}

int
main(int argc, char **argv)
{
    if (argc > 1) {
        dir = argv[1];
    }
    check_numbers();
    check_mntopts();
    check_pidfile();
    check_hosts();
    check_wipefs();
    check_tzlink();
    CHECK(reexec_to_match_kernel() == 0);
    CHECK(reexec_to_match_lp64ness(true) == 0);
    return checks_done("libutil");
}
