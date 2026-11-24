/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/xattr.h>
#include <unistd.h>

#define CHUNK (64 * 1024)

static int failures;

#define CHECK(cond, ...)                                    \
    do {                                                    \
        if (!(cond)) {                                      \
            failures++;                                     \
            printf("FAIL line %d: ", __LINE__);             \
            printf(__VA_ARGS__);                            \
            printf("\n");                                   \
        }                                                   \
    } while (0)

struct entry {
    char path[1024];
    struct stat st;
};

static struct entry *files;
static size_t nfiles, capfiles;
static int dirs, dirs_with_dots, dtype_mismatch, dino_mismatch, quiet;

static uint64_t
fnv(uint64_t h, const unsigned char *p, size_t n)
{
    while (n--)
        h = (h ^ *p++) * 0x100000001b3ULL;
    return h;
}

static double
now(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

static double
cpu(void)
{
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return ru.ru_utime.tv_sec + ru.ru_utime.tv_usec / 1e6 + ru.ru_stime.tv_sec + ru.ru_stime.tv_usec / 1e6;
}

static int
hash_read(const char *path, uint64_t *out, off_t *size)
{
    unsigned char buf[CHUNK];
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -errno;
    uint64_t h = 0xcbf29ce484222325ULL;
    off_t total = 0;
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        h = fnv(h, buf, (size_t)n);
        total += n;
    }
    int err = n < 0 ? -errno : 0;
    close(fd);
    *out = h;
    *size = total;
    return err;
}

static int
hash_mmap(const char *path, off_t size, uint64_t *out, int *dirty_tail)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -errno;
    long page = sysconf(_SC_PAGESIZE);
    size_t span = ((size_t)size + page - 1) / page * page;
    unsigned char *p = mmap(NULL, span, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    if (p == MAP_FAILED)
        return -errno;
    *out = fnv(0xcbf29ce484222325ULL, p, (size_t)size);
    *dirty_tail = 0;
    for (size_t i = (size_t)size; i < span; i++)
        *dirty_tail |= p[i];
    munmap(p, span);
    return 0;
}

static int
byname(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static const char *
kind(mode_t mode)
{
    switch (mode & S_IFMT) {
    case S_IFREG: return "file";
    case S_IFDIR: return "dir";
    case S_IFLNK: return "link";
    case S_IFIFO: return "fifo";
    case S_IFSOCK: return "sock";
    case S_IFCHR: return "chr";
    case S_IFBLK: return "blk";
    default: return "?";
    }
}

static void
describe(const char *root, const char *rel, const struct stat *st)
{
    char path[2048];
    snprintf(path, sizeof(path), "%s/%s", root, rel);
    printf("%s %s %04o nlink=%u size=%lld mtime=%ld.%09ld ctime=%ld.%09ld", rel, kind(st->st_mode),
           st->st_mode & 07777, st->st_nlink, (long long)st->st_size, (long)st->st_mtimespec.tv_sec,
           st->st_mtimespec.tv_nsec, (long)st->st_ctimespec.tv_sec, st->st_ctimespec.tv_nsec);

    if (S_ISLNK(st->st_mode)) {
        char target[1024];
        ssize_t n = readlink(path, target, sizeof(target) - 1);
        target[n < 0 ? 0 : n] = 0;
        printf(" -> %s", n < 0 ? strerror(errno) : target);
    } else if (S_ISREG(st->st_mode)) {
        uint64_t h = 0, m = 0;
        off_t got = 0;
        int dirty = 0;
        int err = hash_read(path, &h, &got);
        if (err) {
            printf(" read=%s", strerror(-err));
        } else {
            printf(" fnv=%016llx", (unsigned long long)h);
            CHECK(got == st->st_size, "%s: read %lld bytes, size %lld", rel, (long long)got, (long long)st->st_size);
            if (st->st_size > 0) {
                err = hash_mmap(path, st->st_size, &m, &dirty);
                CHECK(!err, "%s: mmap: %s", rel, strerror(-err));
                CHECK(err || m == h, "%s: mmap content differs from read", rel);
                CHECK(err || !dirty, "%s: bytes past EOF in the last page are not zero", rel);
                int fd = open(path, O_RDONLY);
                unsigned char last = 0, again = 0, extra;
                CHECK(pread(fd, &last, 1, st->st_size - 1) == 1, "%s: pread of the last byte", rel);
                CHECK(pread(fd, &extra, 1, st->st_size) == 0, "%s: pread at EOF returns data", rel);
                lseek(fd, st->st_size - 1, SEEK_SET);
                CHECK(read(fd, &again, 1) == 1 && again == last, "%s: last byte differs by offset", rel);
                close(fd);
            }
        }
    }
    printf("\n");
}

static void
walk(const char *root, const char *rel)
{
    char path[2048];
    snprintf(path, sizeof(path), "%s%s%s", root, *rel ? "/" : "", rel);
    DIR *d = opendir(path);
    CHECK(d != NULL, "opendir %s: %s", path, strerror(errno));
    if (!d)
        return;

    char **names = NULL;
    size_t n = 0, cap = 0;
    int dots = 0;
    struct dirent *de;
    dirs++;
    while ((de = readdir(d)) != NULL) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) {
            dots++;
            continue;
        }
        char child[2048];
        struct stat st;
        snprintf(child, sizeof(child), "%s/%s", path, de->d_name);
        if (lstat(child, &st) == 0) {
            if (de->d_type != DT_UNKNOWN && DTTOIF(de->d_type) != (st.st_mode & S_IFMT))
                dtype_mismatch++;
            if (de->d_ino != st.st_ino)
                dino_mismatch++;
        }
        if (n == cap)
            names = realloc(names, (cap = cap ? cap * 2 : 64) * sizeof(*names));
        names[n++] = strdup(de->d_name);
    }
    closedir(d);
    if (dots == 2)
        dirs_with_dots++;
    qsort(names, n, sizeof(*names), byname);

    for (size_t i = 0; i < n; i++) {
        char child[1024];
        struct stat st;
        snprintf(child, sizeof(child), "%s%s%s", rel, *rel ? "/" : "", names[i]);
        char full[2048];
        snprintf(full, sizeof(full), "%s/%s", root, child);
        if (lstat(full, &st) != 0) {
            if (!quiet)
                printf("%s lstat=%s\n", child, strerror(errno));
        } else {
            if (!quiet)
                describe(root, child, &st);
            if (S_ISREG(st.st_mode)) {
                if (nfiles == capfiles)
                    files = realloc(files, (capfiles = capfiles ? capfiles * 2 : 256) * sizeof(*files));
                snprintf(files[nfiles].path, sizeof(files[nfiles].path), "%s", full);
                files[nfiles++].st = st;
            }
            if (S_ISDIR(st.st_mode))
                walk(root, child);
        }
        free(names[i]);
    }
    free(names);
}

static void
links(void)
{
    for (size_t i = 0; i < nfiles; i++) {
        if (files[i].st.st_nlink < 2)
            continue;
        size_t first = i;
        for (size_t j = 0; j < i; j++) {
            if (files[j].st.st_ino == files[i].st.st_ino) {
                first = j;
                break;
            }
        }
        printf("link-group %s = %s\n", strrchr(files[i].path, '/') + 1, strrchr(files[first].path, '/') + 1);
    }
}

static int
tree(const char *root)
{
    walk(root, "");
    links();
    printf("dirs=%d dots=%d dtype-mismatch=%d\n", dirs, dirs_with_dots, dtype_mismatch);
    fprintf(stderr, "entries whose d_ino differs from st_ino: %d\n", dino_mismatch);
    return failures ? 1 : 0;
}

static void
expect_errno(int rc, int want, const char *what)
{
    CHECK(rc < 0 && errno == want, "%s: got %d errno %d (%s), want errno %d (%s)", what, rc, rc < 0 ? errno : 0,
          rc < 0 ? strerror(errno) : "success", want, strerror(want));
}

static int
errors(const char *mnt, const char *spec)
{
    char p[1024], q[1024];
    struct statfs sf;

    CHECK(statfs(mnt, &sf) == 0, "statfs: %s", strerror(errno));
    CHECK(!strcmp(sf.f_fstypename, "virtiofs"), "f_fstypename %s", sf.f_fstypename);
    CHECK(!strcmp(sf.f_mntfromname, spec), "f_mntfromname %s", sf.f_mntfromname);
    CHECK((sf.f_flags & (MNT_RDONLY | MNT_NOSUID | MNT_NODEV)) == (MNT_RDONLY | MNT_NOSUID | MNT_NODEV),
          "mount flags 0x%x", sf.f_flags);
    CHECK(sf.f_blocks > 0 && sf.f_bsize > 0, "statfs blocks %llu bsize %u", (unsigned long long)sf.f_blocks,
          sf.f_bsize);

    snprintf(p, sizeof(p), "%s/small/4097", mnt);
    expect_errno(open(p, O_WRONLY), EROFS, "open for writing");
    expect_errno(open(p, O_RDONLY | O_TRUNC), EROFS, "open with O_TRUNC");
    expect_errno(truncate(p, 0), EROFS, "truncate");
    expect_errno(chmod(p, 0600), EROFS, "chmod");
    expect_errno(unlink(p), EROFS, "unlink");
    snprintf(q, sizeof(q), "%s/small/renamed", mnt);
    expect_errno(rename(p, q), EROFS, "rename");
    expect_errno(link(p, q), EROFS, "link");
    expect_errno(symlink("x", q), EROFS, "symlink");
    expect_errno(open(q, O_CREAT | O_WRONLY, 0644), EROFS, "create");
    expect_errno(mkdir(q, 0755), EROFS, "mkdir");
    snprintf(q, sizeof(q), "%s/small", mnt);
    expect_errno(rmdir(q), EROFS, "rmdir");
    expect_errno(setxattr(p, "user.test", "v", 1, 0, 0), EROFS, "setxattr");

    char value[16];
    expect_errno((int)getxattr(p, "user.test", value, sizeof(value), 0, 0), ENOATTR, "getxattr");
    CHECK(listxattr(p, NULL, 0, 0) == 0, "listxattr returned %zd", listxattr(p, NULL, 0, 0));

    snprintf(q, sizeof(q), "%s/missing", mnt);
    expect_errno(open(q, O_RDONLY), ENOENT, "open of a missing file");
    snprintf(q, sizeof(q), "%s/small/4097/x", mnt);
    expect_errno(open(q, O_RDONLY), ENOTDIR, "path through a file");
    char target[64];
    expect_errno((int)readlink(p, target, sizeof(target)), EINVAL, "readlink of a file");

    char name[300];
    memset(name, 'n', 255);
    name[255] = 0;
    snprintf(q, sizeof(q), "%s/names/%s", mnt, name);
    struct stat st;
    CHECK(lstat(q, &st) == 0, "lstat of a 255-byte name: %s", strerror(errno));
    memset(name, 'n', 256);
    name[256] = 0;
    snprintf(q, sizeof(q), "%s/names/%s", mnt, name);
    expect_errno(lstat(q, &st), ENAMETOOLONG, "lstat of a 256-byte name");

    snprintf(q, sizeof(q), "%s/deep/1/2/3/../../2/3/4/5", mnt);
    CHECK(lstat(q, &st) == 0 && S_ISDIR(st.st_mode), "lookup through ..: %s", strerror(errno));
    snprintf(q, sizeof(q), "%s/deep/1/2/3/4/5", mnt);
    char cwd[1024];
    CHECK(chdir(q) == 0 && getcwd(cwd, sizeof(cwd)) && !strcmp(cwd, q), "getcwd in %s gave %s", q, cwd);
    CHECK(chdir("../../..") == 0 && getcwd(cwd, sizeof(cwd)), "chdir ..: %s", strerror(errno));
    snprintf(q, sizeof(q), "%s/deep/1/2", mnt);
    CHECK(!strcmp(cwd, q), "after chdir ../../.. getcwd gave %s, want %s", cwd, q);
    chdir("/");

    printf("errors: %d failures\n", failures);
    return failures ? 1 : 0;
}

struct worker {
    int id, rounds;
    uint64_t *want;
};

static void *
stress_one(void *arg)
{
    struct worker *w = arg;
    for (int r = 0; r < w->rounds; r++) {
        for (size_t k = 0; k < nfiles; k++) {
            size_t i = (k * 7 + (size_t)w->id * 13 + (size_t)r) % nfiles;
            uint64_t h = 0;
            off_t got = 0;
            int dirty = 0;
            int err = (r + w->id) % 2 && files[i].st.st_size > 0 ?
                          hash_mmap(files[i].path, files[i].st.st_size, &h, &dirty) :
                          hash_read(files[i].path, &h, &got);
            CHECK(!err && h == w->want[i], "thread %d: %s differs (%s)", w->id, files[i].path, strerror(-err));
        }
    }
    return NULL;
}

static int
stress(const char *root, int threads, int rounds)
{
    quiet = 1;
    walk(root, "");
    if (nfiles == 0)
        return 1;
    uint64_t *want = calloc(nfiles, sizeof(*want));
    for (size_t i = 0; i < nfiles; i++) {
        off_t got;
        hash_read(files[i].path, &want[i], &got);
    }
    pthread_t tid[64];
    struct worker w[64];
    if (threads > 64)
        threads = 64;
    double t0 = now(), c0 = cpu();
    for (int i = 0; i < threads; i++) {
        w[i] = (struct worker){ i, rounds, want };
        pthread_create(&tid[i], NULL, stress_one, &w[i]);
    }
    for (int i = 0; i < threads; i++)
        pthread_join(tid[i], NULL);
    double dt = now() - t0;
    printf("stress: %d threads x %d rounds over %zu files in %.2f s at %.2f CPUs busy, %d failures\n", threads,
           rounds, nfiles, dt, (cpu() - c0) / dt, failures);
    return failures ? 1 : 0;
}

struct racer {
    int fd, id;
    off_t size;
    const unsigned char *want;
    double until;
    long reads, bad;
};

static void *
race_one(void *arg)
{
    struct racer *r = arg;
    unsigned char *buf = malloc(1 << 20);
    unsigned seed = (unsigned)r->id * 2654435761u;
    while (now() < r->until) {
        seed = seed * 1103515245u + 12345u;
        off_t off = (off_t)(seed % (unsigned)(r->size / 16384)) * 16384;
        size_t len = 16384u << (seed >> 28) % 7;
        if (off + (off_t)len > r->size)
            len = (size_t)(r->size - off);
        ssize_t n = pread(r->fd, buf, len, off);
        r->reads++;
        if (n != (ssize_t)len || memcmp(buf, r->want + off, len))
            r->bad++;
    }
    free(buf);
    return NULL;
}

static int
pread_race(const char *path, int threads, double seconds)
{
    int fd = open(path, O_RDONLY);
    struct stat st;
    if (fd < 0 || fstat(fd, &st) != 0)
        return 1;
    unsigned char *want = malloc((size_t)st.st_size);
    if (pread(fd, want, (size_t)st.st_size, 0) != st.st_size)
        return 1;
    fcntl(fd, F_NOCACHE, 1);
    pthread_t tid[64];
    struct racer r[64];
    if (threads > 64)
        threads = 64;
    long reads = 0, bad = 0;
    for (int i = 0; i < threads; i++) {
        r[i] = (struct racer){ fd, i, st.st_size, want, now() + seconds, 0, 0 };
        pthread_create(&tid[i], NULL, race_one, &r[i]);
    }
    for (int i = 0; i < threads; i++) {
        pthread_join(tid[i], NULL);
        reads += r[i].reads;
        bad += r[i].bad;
    }
    printf("pread-race %s: %d threads, one descriptor, %ld reads, %ld wrong\n", path, threads, reads, bad);
    return bad ? 1 : 0;
}

static int
inodes(const char *dir, int limit)
{
    DIR *d = opendir(dir);
    struct dirent *de;
    char path[2048];
    struct stat st;
    while (d && limit-- > 0 && (de = readdir(d)) != NULL) {
        snprintf(path, sizeof(path), "%s/%s", dir, de->d_name);
        lstat(path, &st);
        printf("%-12s d_ino=%llu st_ino=%llu\n", de->d_name, (unsigned long long)de->d_ino,
               (unsigned long long)st.st_ino);
    }
    return d ? 0 : 1;
}

static int
small_readdir(const char *dir, int bufsize)
{
    char *buf = malloc((size_t)bufsize);
    int fd = open(dir, O_RDONLY | O_DIRECTORY);
    off_t base = 0;
    long listed = 0, want = 0;
    int got;

    if (!buf || fd < 0)
        return 1;
    while ((got = syscall(SYS_getdirentries64, fd, buf, bufsize, &base)) > 0) {
        for (int at = 0; at < got; at += ((struct dirent *)(buf + at))->d_reclen)
            listed++;
    }
    int err = got < 0 ? errno : 0;
    close(fd);
    DIR *d = opendir(dir);
    while (d && readdir(d))
        want++;
    if (d)
        closedir(d);
    printf("%d-byte buffers: %ld of %ld entries, %s\n", bufsize, listed, want, err ? strerror(err) : "end");
    return err == EINVAL || (!err && listed == want) ? 0 : 1;
}

static int
as_user(uid_t uid, gid_t gid, char **paths)
{
    struct stat st;
    if (setgid(gid) != 0 || setuid(uid) != 0)
        return 1;
    for (char **p = paths; *p; p++) {
        if (lstat(*p, &st) != 0)
            printf("%s lstat=%s\n", *p, strerror(errno));
        else
            printf("%s uid=%u gid=%u mode=%04o\n", *p, st.st_uid, st.st_gid, st.st_mode & 07777);
    }
    return 0;
}

static int
lookups(int times, char **paths)
{
    struct stat st;
    for (char **p = paths; *p; p++) {
        for (int i = 0; i < times; i++) {
            if (lstat(*p, &st) != 0)
                return 1;
        }
    }
    return 0;
}

static void
read_pass(const char *what, const char *path, size_t bufsize, int nocache)
{
    static unsigned char *buf;
    if (!buf)
        buf = malloc(16 << 20);
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return;
    if (nocache)
        fcntl(fd, F_NOCACHE, 1);
    double t0 = now(), c0 = cpu();
    off_t total = 0;
    ssize_t n;
    while ((n = read(fd, buf, bufsize)) > 0)
        total += n;
    double dt = now() - t0, dc = cpu() - c0;
    close(fd);
    printf("  %s: %.0f MB/s, %.2f CPUs busy\n", what, total / dt / 1e6, dc / dt);
}

static int
bench(const char *path)
{
    printf("read %s\n", path);
    read_pass("first pass, 1 MiB reads", path, 1 << 20, 0);
    read_pass("cached pass, 1 MiB reads", path, 1 << 20, 0);
    read_pass("cached pass, 16 KiB reads", path, 16 << 10, 0);
    read_pass("uncached pass, 1 MiB reads", path, 1 << 20, 1);
    read_pass("uncached pass, 16 MiB reads", path, 16 << 20, 1);
    return 0;
}

static double
lstat_round(const char *dir, char **names, size_t n)
{
    struct stat st;
    char path[2048];
    double t0 = now();
    for (size_t i = 0; i < n; i++) {
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
        lstat(path, &st);
    }
    return (now() - t0) / (double)n * 1e6;
}

static int
bench_meta(const char *dir, int rounds)
{
    DIR *d = opendir(dir);
    if (!d)
        return 1;
    char **names = NULL;
    size_t n = 0, cap = 0;
    struct dirent *de;
    double t0 = now();
    while ((de = readdir(d)) != NULL) {
        if (n == cap)
            names = realloc(names, (cap = cap ? cap * 2 : 1024) * sizeof(*names));
        names[n++] = strdup(de->d_name);
    }
    closedir(d);
    double listed = now() - t0;

    double cold = lstat_round(dir, names, n), warm = 0;
    t0 = now();
    double c0 = cpu();
    for (int r = 1; r < rounds; r++)
        warm += lstat_round(dir, names, n) / (rounds - 1);
    double busy = (cpu() - c0) / (now() - t0);
    t0 = now();
    char path[2048];
    for (size_t i = 0; i < n; i++) {
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
        int fd = open(path, O_RDONLY);
        if (fd >= 0)
            close(fd);
    }
    double opened = (now() - t0) / (double)n * 1e6;
    printf("%s: readdir of %zu entries %.2f ms, lstat %.1f us each first, then %.1f us at %.2f CPUs busy, "
           "open+close %.1f us each\n", dir, n, listed * 1e3, cold, warm, busy, opened);
    return 0;
}

int
main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc == 3 && !strcmp(argv[1], "tree"))
        return tree(argv[2]);
    if (argc == 4 && !strcmp(argv[1], "errors"))
        return errors(argv[2], argv[3]);
    if (argc == 5 && !strcmp(argv[1], "stress"))
        return stress(argv[2], atoi(argv[3]), atoi(argv[4]));
    if (argc == 3 && !strcmp(argv[1], "bench"))
        return bench(argv[2]);
    if (argc == 4 && !strcmp(argv[1], "bench-meta"))
        return bench_meta(argv[2], atoi(argv[3]));
    if (argc == 5 && !strcmp(argv[1], "pread-race"))
        return pread_race(argv[2], atoi(argv[3]), atof(argv[4]));
    if (argc == 4 && !strcmp(argv[1], "inodes"))
        return inodes(argv[2], atoi(argv[3]));
    if (argc == 4 && !strcmp(argv[1], "small-readdir"))
        return small_readdir(argv[2], atoi(argv[3]));
    if (argc >= 5 && !strcmp(argv[1], "as"))
        return as_user((uid_t)atoi(argv[2]), (gid_t)atoi(argv[3]), argv + 4);
    if (argc >= 4 && !strcmp(argv[1], "lookups"))
        return lookups(atoi(argv[2]), argv + 3);
    fprintf(stderr, "usage: t_virtiofs tree DIR | errors MNT SPEC | stress DIR THREADS ROUNDS | bench FILE |"
                    " bench-meta DIR ROUNDS\n");
    return 2;
}
