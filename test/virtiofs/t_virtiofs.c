/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <sys/xattr.h>
#include <time.h>
#include <unistd.h>

#define CHUNK (64 * 1024)
#define XATTR_PROVENANCE "com.apple.provenance"

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
    return clock_gettime_nsec_np(CLOCK_UPTIME_RAW) / 1e9;
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
    CHECK(listxattr(p, NULL, 0, 0) >= 0, "listxattr: %s", strerror(errno));

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
    char wide[3 * 256 + 1] = "";
    for (int i = 0; i < 255; i++)
        strcat(wide, "\xe4\xb8\xad");
    snprintf(q, sizeof(q), "%s/long/%s", mnt, wide);
    CHECK(lstat(q, &st) == 0, "lstat of a 255-character, 765-byte name: %s", strerror(errno));
    strcat(wide, "\xe4\xb8\xad");
    snprintf(q, sizeof(q), "%s/long/%s", mnt, wide);
    expect_errno(lstat(q, &st), ENAMETOOLONG, "lstat of a 256-character name");

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
    char *buf = malloc(65536);
    int fd = open(dir, O_RDONLY | O_DIRECTORY);
    off_t base = 0;
    long listed = 0, want = 0;
    int got, next = 0;

    if (!buf || fd < 0)
        return 1;
    while ((got = syscall(SYS_getdirentries64, fd, buf, bufsize, &base)) > 0) {
        for (int at = 0; at < got; at += ((struct dirent *)(buf + at))->d_reclen)
            listed++;
    }
    int err = got < 0 ? errno : 0;
    if (err == EINVAL && syscall(SYS_getdirentries64, fd, buf, 65536, &base) > 0)
        next = ((struct dirent *)buf)->d_reclen;
    close(fd);
    DIR *d = opendir(dir);
    while (d && readdir(d))
        want++;
    if (d)
        closedir(d);
    printf("%d-byte buffers: %ld of %ld entries, %s", bufsize, listed, want, err ? strerror(err) : "end");
    if (err == EINVAL)
        printf(" before a %d-byte entry", next);
    printf("\n");
    return (err == EINVAL && next > bufsize) || (!err && listed == want) ? 0 : 1;
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
read_close(const char *path, int rounds)
{
    static char buf[128 << 10];
    long failures = 0;
    off_t at = 0;

    for (int r = 0; r < rounds; r++) {
        int fd = open(path, O_RDONLY);
        for (int i = 0; fd >= 0 && i < 4; i++, at += (off_t)sizeof(buf)) {
            if (pread(fd, buf, sizeof(buf), at) != (ssize_t)sizeof(buf))
                failures++;
        }
        if (fd < 0)
            failures++;
        else
            close(fd);
    }
    printf("read-close: %d rounds of 512 KiB, %ld failures\n", rounds, failures);
    return failures != 0;
}

static int
exec_fail(const char *path)
{
    char *argv[] = { (char *)path, NULL };
    execv(path, argv);
    printf("exec: %s\n", strerror(errno));
    return errno != ENOEXEC;
}

struct advised_append {
    int fd;
    atomic_int stop;
    long appends;
    int failed;
};

static void *
append_forever(void *arg)
{
    struct advised_append *a = arg;
    char chunk[3000];
    memset(chunk, 'y', sizeof(chunk));
    while (!atomic_load(&a->stop) && !a->failed) {
        a->failed = write(a->fd, chunk, sizeof(chunk)) != (ssize_t)sizeof(chunk);
        a->appends++;
    }
    return NULL;
}

static int
advise_append(const char *path, int seconds)
{
    static char buf[131072];
    memset(buf, 'x', 4000);
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    struct advised_append a = { .fd = open(path, O_WRONLY | O_APPEND) };
    if (fd < 0 || a.fd < 0 || write(fd, buf, 4000) != 4000) {
        printf("create %s: %s\n", path, strerror(errno));
        return 1;
    }
    pthread_t t;
    if (pthread_create(&t, NULL, append_forever, &a) != 0)
        return 1;
    long rounds = 0, zeros = 0, failed = 0;
    for (time_t end = time(NULL) + seconds; time(NULL) < end; rounds++) {
        struct stat st;
        if (fstat(fd, &st) != 0) {
            failed++;
            break;
        }
        off_t at = st.st_size > 65536 ? st.st_size - 65536 : 0;
        struct radvisory ra = { .ra_offset = at, .ra_count = 131072 };
        failed += fcntl(fd, F_RDADVISE, &ra) != 0;
        if (fstat(fd, &st) != 0) {
            failed++;
            break;
        }
        off_t len = st.st_size - at < (off_t)sizeof(buf) ? st.st_size - at : (off_t)sizeof(buf);
        ssize_t got = pread(fd, buf, (size_t)len, at);
        failed += got <= 0;
        zeros += got > 0 && memchr(buf, 0, (size_t)got) != NULL;
    }
    atomic_store(&a.stop, 1);
    pthread_join(t, NULL);
    close(a.fd);
    close(fd);
    failed += a.failed;
    printf("read advice beside %ld appends: %ld rounds, %ld read zeros inside the file, %ld calls failed\n", a.appends,
           rounds, zeros, failed);
    return zeros != 0 || failed != 0 || rounds == 0 || a.appends == 0;
}

static void *
append_bytes(void *arg)
{
    struct advised_append *a = arg;
    while (!atomic_load(&a->stop) && !a->failed) {
        a->failed = (a->appends && a->appends % 8000 == 0 && ftruncate(a->fd, 4000) != 0) || write(a->fd, "a", 1) != 1;
        a->appends++;
        usleep(50);
    }
    return NULL;
}

static int
store_loop(const char *path, const char *other, int seconds)
{
    static char page[4000];
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    struct advised_append a = { .fd = open(path, O_WRONLY | O_APPEND) };
    volatile uint32_t *counter = MAP_FAILED;
    if (fd >= 0 && a.fd >= 0 && write(fd, page, sizeof(page)) == (ssize_t)sizeof(page))
        counter = mmap(NULL, 16384, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (counter == MAP_FAILED) {
        printf("create and map %s: %s\n", path, strerror(errno));
        return 1;
    }
    pthread_t t;
    if (pthread_create(&t, NULL, append_bytes, &a) != 0) {
        munmap((void *)counter, 16384);
        close(a.fd);
        close(fd);
        return 1;
    }
    uint32_t done = 0, onfile = 0;
    for (time_t end = time(NULL) + seconds; time(NULL) < end;) {
        for (int i = 0; i < 1000; i++)
            counter[25]++;
        done += 1000;
    }
    atomic_store(&a.stop, 1);
    pthread_join(t, NULL);
    int rc = msync((void *)counter, 16384, MS_SYNC) != 0;
    uint32_t kept = counter[25];
    int held = open(other, O_RDONLY), host = open(other, O_RDONLY);
    rc |= host < 0 || pread(host, &onfile, sizeof(onfile), 100) != sizeof(onfile);
    struct stat st;
    size_t appended = fstat(host, &st) == 0 && st.st_size > (off_t)sizeof(page) ? (size_t)st.st_size - sizeof(page) : 0;
    char *tail = malloc(appended + 1);
    size_t wrong = !tail || pread(host, tail, appended, sizeof(page)) != (ssize_t)appended;
    for (size_t i = 0; !wrong && i < appended; i++)
        wrong += tail[i] != 'a';
    free(tail);
    printf("a counter stored to in a tight loop beside %ld appends: %u stores, %u in memory, %u on the Mac, %zu bytes "
           "appended since the last truncation, %zu of them wrong\n", a.appends, done, kept, onfile, appended, wrong);
    munmap((void *)counter, 16384);
    close(host);
    close(held);
    close(a.fd);
    close(fd);
    return rc || a.failed || a.appends == 0 || kept != done || onfile != done || appended == 0 || wrong != 0;
}

enum { FULL_PAGES = 128 };

static int
map_fail(const char *path, int seconds)
{
    size_t size = (size_t)FULL_PAGES * 16384;
    char *data = malloc(size);
    int fd = open(path, O_RDWR);
    volatile char *map = fd < 0 || !data ? MAP_FAILED : mmap(NULL, 16384 + size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        printf("open and map %s: %s\n", path, strerror(errno));
        return 1;
    }
    memset(data, 'w', size);
    for (int i = 0; i < FULL_PAGES; i++)
        map[16384 + 5000 + (off_t)i * 16384] = 'm';
    sleep((unsigned)seconds);
    ssize_t wrote = pwrite(fd, data, size, 16384);
    int refused = wrote < 0 ? errno : 0, kept = 0;
    for (int i = 0; i < FULL_PAGES; i++) {
        off_t at = 5000 + (off_t)i * 16384;
        kept += map[16384 + at] == (wrote > at ? 'w' : 'm');
    }
    char last = map[16384 + 5000 + (off_t)(FULL_PAGES - 1) * 16384];
    int pushed = fsync(fd), unpushed = pushed ? errno : 0;
    sleep((unsigned)seconds);
    int again = fsync(fd);
    printf("a write over %d stored pages returned %zd (%s), %d of the stores read as they should and the last reads %c, "
           "fsync then said %s and later %s\n", FULL_PAGES, wrote, strerror(refused), kept, last, strerror(unpushed),
           again ? strerror(errno) : "ok");
    munmap((void *)map, 16384 + size);
    close(fd);
    free(data);
    return !(wrote < (ssize_t)size && kept == FULL_PAGES && pushed != 0 && again == 0);
}

static int
map_unpushed(const char *path, int seconds)
{
    size_t size = (size_t)FULL_PAGES * 16384;
    int fd = open(path, O_RDWR), reader = open(path, O_RDONLY);
    volatile char *map = fd < 0 ? MAP_FAILED : mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED || reader < 0) {
        printf("open and map %s: %s\n", path, strerror(errno));
        return 1;
    }
    close(fd);
    sleep((unsigned)seconds);
    for (int i = 0; i < FULL_PAGES; i++)
        map[100 + (off_t)i * 16384] = 'k';
    int pushed = fsync(reader), unpushed = pushed ? errno : 0;
    munmap((void *)map, size);
    close(reader);
    sleep((unsigned)seconds);
    sync();
    printf("fsync of stores to %d pages said %s before the mapping and the last descriptor went away\n", FULL_PAGES,
           strerror(unpushed));
    return pushed == 0;
}

enum { CHAOS_THREADS = 8, CHAOS_LOW = 100000, CHAOS_HIGH = 262144 };

static struct {
    const char *path;
    volatile char *map;
    atomic_int stop;
    atomic_long done[CHAOS_THREADS], failed[CHAOS_THREADS];
} chaos;

static void
chaos_count(int who, bool bad)
{
    atomic_fetch_add(&chaos.done[who], 1);
    atomic_fetch_add(&chaos.failed[who], bad);
}

static void *
chaos_write(void *arg)
{
    static char block[4096];
    int fd = open(chaos.path, O_WRONLY);
    (void)arg;
    while (!atomic_load(&chaos.stop))
        chaos_count(0, pwrite(fd, block, sizeof(block), arc4random_uniform(CHAOS_HIGH - 4096)) != (ssize_t)sizeof(block));
    close(fd);
    return NULL;
}

static void *
chaos_store(void *arg)
{
    (void)arg;
    while (!atomic_load(&chaos.stop)) {
        chaos.map[arc4random_uniform(81920)] = 'm';
        chaos_count(1, msync((void *)chaos.map, 81920, MS_SYNC) != 0);
    }
    return NULL;
}

static void *
chaos_read(void *arg)
{
    struct stat st;
    char buf[4096];
    int fd = open(chaos.path, O_RDONLY);
    (void)arg;
    while (!atomic_load(&chaos.stop))
        chaos_count(2, fstat(fd, &st) != 0 ||
                       pread(fd, buf, sizeof(buf), arc4random_uniform(CHAOS_LOW - 4096)) != (ssize_t)sizeof(buf));
    close(fd);
    return NULL;
}

static void *
chaos_size(void *arg)
{
    int fd = open(chaos.path, O_RDWR);
    (void)arg;
    while (!atomic_load(&chaos.stop)) {
        chaos_count(3, ftruncate(fd, CHAOS_LOW + arc4random_uniform(CHAOS_HIGH - CHAOS_LOW)) != 0);
        usleep(500);
    }
    close(fd);
    return NULL;
}

static void *
chaos_sync(void *arg)
{
    int fd = open(chaos.path, O_RDWR);
    (void)arg;
    while (!atomic_load(&chaos.stop))
        chaos_count(4, fsync(fd) != 0);
    close(fd);
    return NULL;
}

static void *
chaos_map(void *arg)
{
    (void)arg;
    while (!atomic_load(&chaos.stop)) {
        int fd = open(chaos.path, O_RDWR);
        char *map = fd < 0 ? MAP_FAILED : mmap(NULL, 16384, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (map != MAP_FAILED) {
            map[100] = 'r';
            munmap(map, 16384);
        }
        chaos_count(5, map == MAP_FAILED || close(fd) != 0);
    }
    return NULL;
}

static void *
chaos_self(void *arg)
{
    int fd = open(chaos.path, O_RDONLY);
    (void)arg;
    while (!atomic_load(&chaos.stop))
        chaos_count(6, pread(fd, (void *)(chaos.map + 16384), 16384, 49152) != 16384);
    close(fd);
    return NULL;
}

static void *
chaos_append(void *arg)
{
    int fd = open(chaos.path, O_WRONLY | O_APPEND);
    (void)arg;
    while (!atomic_load(&chaos.stop)) {
        chaos_count(7, write(fd, "a", 1) != 1);
        usleep(200);
    }
    close(fd);
    return NULL;
}

static int
chaos_run(const char *path, int seconds)
{
    static const char *names[CHAOS_THREADS] = { "writes", "stores with msync", "stats and reads", "truncations", "fsyncs",
                                                "mappings made and removed", "reads into the mapping", "appends" };
    void *(*run[CHAOS_THREADS])(void *) = { chaos_write, chaos_store, chaos_read, chaos_size, chaos_sync, chaos_map,
                                            chaos_self, chaos_append };
    pthread_t t[CHAOS_THREADS];
    long before[CHAOS_THREADS] = { 0 }, bad = 0;
    int quiet[CHAOS_THREADS] = { 0 }, fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    chaos.path = path;
    chaos.map = fd < 0 || ftruncate(fd, CHAOS_HIGH) != 0 ? MAP_FAILED
                                                         : mmap(NULL, 81920, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (chaos.map == MAP_FAILED) {
        printf("create and map %s: %s\n", path, strerror(errno));
        return 1;
    }
    for (int i = 0; i < CHAOS_THREADS; i++)
        pthread_create(&t[i], NULL, run[i], NULL);
    for (int s = 0; s < seconds; s++) {
        sleep(1);
        int worst = 0;
        for (int i = 0; i < CHAOS_THREADS; i++) {
            long now = atomic_load(&chaos.done[i]);
            quiet[i] = now == before[i] ? quiet[i] + 1 : 0;
            worst = quiet[i] > worst ? quiet[i] : worst;
            before[i] = now;
        }
        if (worst < 5)
            continue;
        printf("after %d s on one file, no progress for 5 s in:", s + 1);
        for (int i = 0; i < CHAOS_THREADS; i++)
            if (quiet[i] >= 5)
                printf(" %s (%ld done),", names[i], before[i]);
        printf(" the rest still runs\n");
        _exit(3);
    }
    atomic_store(&chaos.stop, 1);
    printf("%d s on one file:", seconds);
    for (int i = 0; i < CHAOS_THREADS; i++) {
        pthread_join(t[i], NULL);
        printf(" %ld %s,", atomic_load(&chaos.done[i]), names[i]);
        bad += atomic_load(&chaos.failed[i]);
    }
    printf(" %ld calls failed\n", bad);
    munmap((void *)chaos.map, 81920);
    close(fd);
    return bad != 0;
}

static int
locks_child(const char *path, off_t start, bool free)
{
    pid_t child = fork();
    int status = 0;
    if (child == 0) {
        struct flock want = { .l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = start, .l_len = 100 }, seen = want;
        int fd = open(path, O_RDWR), got = fcntl(fd, F_GETLK, &seen), taken = fcntl(fd, F_SETLK, &want);
        bool held = got == 0 && seen.l_type == F_WRLCK && seen.l_pid == getppid();
        _exit(free ? taken != 0 : !(taken == -1 && (errno == EAGAIN || errno == EACCES) && held));
    }
    return child > 0 && waitpid(child, &status, 0) == child && WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static int
locks_run(const char *path)
{
    struct flock range = { .l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 100 };
    int a = open(path, O_RDWR | O_CREAT, 0644), b = open(path, O_RDWR);
    CHECK(a >= 0 && b >= 0 && flock(a, LOCK_EX) == 0, "open and flock %s: %s", path, strerror(errno));
    CHECK(flock(b, LOCK_EX | LOCK_NB) == -1 && errno == EWOULDBLOCK, "a second descriptor took the flock: %s", strerror(errno));
    CHECK(flock(a, LOCK_UN) == 0 && flock(b, LOCK_EX | LOCK_NB) == 0 && flock(b, LOCK_UN) == 0,
          "the flock was not free after its release: %s", strerror(errno));
    CHECK(fcntl(a, F_SETLK, &range) == 0, "fcntl F_SETLK: %s", strerror(errno));
    CHECK(locks_child(path, 50, false) == 0, "another process took or did not see a write lock over bytes 0 to 100");
    CHECK(locks_child(path, 100, true) == 0, "another process could not lock the bytes beside it");
    CHECK(close(b) == 0 && locks_child(path, 50, true) == 0, "the lock outlived the close of a descriptor of its process");
    close(a);
    printf("flock and fcntl locks: %d failures\n", failures);
    return failures ? 1 : 0;
}

enum { OWNERS = 6, OWNED = 50000, OWNED_TAIL = 100000 };

struct owned {
    const char *path;
    volatile unsigned char *map;
    unsigned char *good;
    off_t base;
    unsigned seed;
    long steps;
    char why[200];
};

static atomic_int owned_stop;

static void
owned_differs(struct owned *r, const char *what, const unsigned char *got, size_t off, size_t len)
{
    for (size_t i = 0; i < len && !r->why[0]; i++) {
        if (got[i] != r->good[off + i])
            snprintf(r->why, sizeof(r->why), "step %ld, %s at %lld for %zu: byte %lld is %d, its writer last put %d there",
                     r->steps, what, (long long)r->base + (long long)off, len, (long long)r->base + (long long)(off + i),
                     got[i], r->good[off + i]);
    }
}

static void *
owned_work(void *arg)
{
    struct owned *r = arg;
    unsigned char *buf = malloc(OWNED);
    int fd = open(r->path, O_RDWR);
    if (fd < 0 || !buf)
        snprintf(r->why, sizeof(r->why), "open: %s", strerror(errno));
    while (!atomic_load(&owned_stop) && !r->why[0]) {
        size_t off = (size_t)rand_r(&r->seed) % OWNED, len = 1 + (size_t)rand_r(&r->seed) % 20000;
        int kind = rand_r(&r->seed) % 100;
        unsigned char mark = (unsigned char)(r->steps % 251 + 1);
        len = off + len > OWNED ? OWNED - off : len;
        if (kind < 25) {
            memset(buf, mark, len);
            if (pwrite(fd, buf, len, r->base + (off_t)off) != (ssize_t)len)
                snprintf(r->why, sizeof(r->why), "step %ld, write: %s", r->steps, strerror(errno));
            memset(r->good + off, mark, len);
        } else if (kind < 50) {
            memset((void *)(r->map + r->base + off), mark, len);
            memset(r->good + off, mark, len);
        } else if (kind < 70) {
            if (pread(fd, buf, len, r->base + (off_t)off) != (ssize_t)len)
                snprintf(r->why, sizeof(r->why), "step %ld, read: %s", r->steps, strerror(errno));
            owned_differs(r, "read", buf, off, len);
        } else if (kind < 90) {
            memcpy(buf, (const void *)(r->map + r->base + off), len);
            owned_differs(r, "the mapping", buf, off, len);
        } else {
            off_t first = (r->base + (off_t)off) & ~(off_t)16383;
            off_t last = (r->base + (off_t)(off + len) + 16383) & ~(off_t)16383;
            if (msync((void *)(r->map + first), (size_t)(last - first), MS_SYNC) != 0)
                snprintf(r->why, sizeof(r->why), "step %ld, msync: %s", r->steps, strerror(errno));
        }
        r->steps++;
    }
    close(fd);
    free(buf);
    return NULL;
}

static void *
owned_others(void *arg)
{
    struct owned *r = arg;
    struct stat st;
    int fd = open(r->path, O_RDWR);
    while (!atomic_load(&owned_stop) && fd >= 0) {
        int kind = rand_r(&r->seed) % 3;
        off_t size = (off_t)OWNERS * OWNED + rand_r(&r->seed) % OWNED_TAIL;
        if ((kind == 0 && fsync(fd) != 0) || (kind == 1 && fstat(fd, &st) != 0) || (kind == 2 && ftruncate(fd, size) != 0))
            snprintf(r->why, sizeof(r->why), "step %ld, fsync, stat or truncate: %s", r->steps, strerror(errno));
        r->steps++;
        usleep(200);
    }
    close(fd);
    return NULL;
}

static int
owned_run(const char *path, const char *other, int seconds)
{
    static struct owned work[OWNERS], rest[2];
    static unsigned char back[OWNED];
    pthread_t t[OWNERS + 2];
    size_t span = (size_t)OWNERS * OWNED;
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644), bad = fd < 0 || ftruncate(fd, (off_t)span) != 0;
    volatile unsigned char *map = bad ? MAP_FAILED : mmap(NULL, span, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    long steps = 0;
    if (map == MAP_FAILED) {
        printf("create and map %s: %s\n", path, strerror(errno));
        return 1;
    }
    for (int i = 0; i < OWNERS + 2; i++) {
        struct owned *r = i < OWNERS ? &work[i] : &rest[i - OWNERS];
        *r = (struct owned){ .path = path, .map = map, .good = calloc(1, OWNED), .base = (off_t)i * OWNED,
                              .seed = (unsigned)i + 1 };
        pthread_create(&t[i], NULL, i < OWNERS ? owned_work : owned_others, r);
    }
    sleep((unsigned)seconds);
    atomic_store(&owned_stop, 1);
    for (int i = 0; i < OWNERS + 2; i++)
        pthread_join(t[i], NULL);
    bad = fsync(fd) != 0;
    int held = open(other, O_RDONLY), host = open(other, O_RDONLY);
    for (int i = 0; i < OWNERS; i++) {
        struct owned *r = &work[i];
        if (!r->why[0] && pread(fd, back, OWNED, r->base) == OWNED)
            owned_differs(r, "the last read", back, 0, OWNED);
        if (!r->why[0]) {
            memcpy(back, (const void *)(map + r->base), OWNED);
            owned_differs(r, "the mapping at the end", back, 0, OWNED);
        }
        if (!r->why[0] && pread(host, back, OWNED, r->base) == OWNED)
            owned_differs(r, "the Mac", back, 0, OWNED);
        if (r->why[0])
            printf("region %d: %s\n", i, r->why);
        bad |= r->why[0] != 0;
        steps += r->steps;
    }
    for (int i = 0; i < 2; i++) {
        if (rest[i].why[0])
            printf("beside the regions: %s\n", rest[i].why);
        bad |= rest[i].why[0] != 0;
    }
    printf("%d threads worked %d s on their own regions of one file, %ld writes, stores, reads and msyncs beside %ld "
           "fsyncs, stats and truncations of the tail: %s\n", OWNERS, seconds, steps, rest[0].steps + rest[1].steps,
           bad ? "FAILED" : "every byte is what its writer last put there, in the guest and on the Mac");
    close(host);
    close(held);
    munmap((void *)map, span);
    close(fd);
    return bad;
}

enum { MODEL_MAX = 262144 };

static int
model_differs(const char *what, int step, const unsigned char *got, const unsigned char *good, size_t off, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (got[i] != good[i]) {
            printf("step %d, %s at %zu for %zu: byte %zu is %d, the model says %d\n", step, what, off, len, off + i, got[i],
                   good[i]);
            return 1;
        }
    }
    return 0;
}

static int
model_host(const char *other, int step, const unsigned char *good, size_t size)
{
    static unsigned char back[MODEL_MAX];
    struct stat st;
    int held = open(other, O_RDONLY), fd = open(other, O_RDONLY);
    int bad = fd < 0 || fstat(fd, &st) != 0 || (size_t)st.st_size != size || pread(fd, back, MODEL_MAX, 0) != (ssize_t)size;
    if (bad)
        printf("step %d, the Mac: %lld bytes, the model says %zu: %s\n", step, fd < 0 ? -1LL : (long long)st.st_size, size,
               strerror(errno));
    bad = bad || model_differs("the Mac", step, back, good, 0, size);
    close(fd);
    close(held);
    return bad;
}

static int
model_run(const char *path, const char *other, int steps, unsigned seed)
{
    static unsigned char good[MODEL_MAX], buf[MODEL_MAX];
    size_t size = 0;
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644), bad = fd < 0, step = 0;
    volatile unsigned char *map = bad ? MAP_FAILED : mmap(NULL, MODEL_MAX, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    long counts[10] = { 0 };
    bad = bad || map == MAP_FAILED;
    srandom(seed);
    for (step = 1; step <= steps && !bad; step++) {
        int kind = (int)(random() % 100);
        size_t off = (size_t)random() % MODEL_MAX, len = 1 + (size_t)random() % 40000;
        unsigned char mark = (unsigned char)(step % 251 + 1);
        len = off + len > MODEL_MAX ? MODEL_MAX - off : len;
        if (kind < 30) {
            memset(buf, mark, len);
            bad = pwrite(fd, buf, len, (off_t)off) != (ssize_t)len;
            if (off > size)
                memset(good + size, 0, off - size);
            memset(good + off, mark, len);
            size = off + len > size ? off + len : size;
            counts[0]++;
        } else if (kind < 50) {
            size_t want = off >= size ? 0 : len < size - off ? len : size - off;
            bad = pread(fd, buf, len, (off_t)off) != (ssize_t)want || model_differs("read", step, buf, good + off, off, want);
            counts[1]++;
        } else if (kind < 60) {
            bad = ftruncate(fd, (off_t)off) != 0;
            if (off > size)
                memset(good + size, 0, off - size);
            size = off;
            counts[2]++;
        } else if (kind < 75 && size) {
            off %= size;
            len = len < size - off ? len : size - off;
            memcpy(buf, (const void *)(map + off), len);
            bad = model_differs("mapping", step, buf, good + off, off, len);
            counts[3]++;
        } else if (kind < 90 && size) {
            off %= size;
            len = len < size - off ? len : size - off;
            memset((void *)(map + off), mark, len);
            memset(good + off, mark, len);
            bad = (random() & 1) && msync((void *)map, size, MS_SYNC) != 0;
            counts[4]++;
        } else if (kind < 92) {
            bad = close(fd) != 0 || (fd = open(path, O_RDWR)) < 0;
            counts[5]++;
        } else if (kind < 94) {
            bad = fsync(fd) != 0;
            counts[6]++;
        } else if (kind >= 94 && kind < 96) {
            bad = fsync(fd) != 0 || model_host(other, step, good, size);
            counts[7]++;
        } else if (kind >= 96) {
            int host = fsync(fd) == 0 ? open(other, O_WRONLY | (kind < 98 ? O_APPEND : 0)) : -1;
            len = len < MODEL_MAX - size ? len : MODEL_MAX - size;
            memset(buf, mark, len);
            bad = host < 0 || (kind < 98 ? write(host, buf, len) != (ssize_t)len : ftruncate(host, (off_t)off) != 0);
            if (kind < 98) {
                memset(good + size, mark, len);
                size += len;
            } else {
                if (off > size)
                    memset(good + size, 0, off - size);
                size = off;
            }
            bad = close(host) != 0 || bad || close(fd) != 0 || (fd = open(path, O_RDWR)) < 0;
            counts[kind < 98 ? 8 : 9]++;
        }
        if (bad)
            printf("step %d of seed %u failed, kind %d, at %zu for %zu, with %zu bytes: %s\n", step, seed, kind, off, len,
                   size, strerror(errno));
    }
    bad = bad || fsync(fd) != 0 || model_host(other, step, good, size);
    printf("%d steps of seed %u against a model: %ld writes, %ld reads, %ld truncations, %ld reads and %ld stores through "
           "a mapping, %ld reopens, %ld fsyncs, %ld comparisons with the Mac, %ld appends and %ld truncations on the Mac, "
           "%s\n", step - 1, seed, counts[0], counts[1], counts[2], counts[3], counts[4], counts[5], counts[6], counts[7],
           counts[8], counts[9], bad ? "FAILED" : "all equal");
    return bad;
}

static int
short_write(const char *path, int seconds)
{
    size_t size = 1 << 20;
    char *data = malloc(size), *back = malloc(size);
    int fd = open(path, O_RDWR);
    if (!data || !back || fd < 0) {
        printf("open %s: %s\n", path, strerror(errno));
        return 1;
    }
    memset(data, 'p', size);
    ssize_t first = pwrite(fd, data, size, 0);
    int refused = first < 0 ? errno : 0;
    size_t kept = first > 0 ? (size_t)first : 0;
    bool same = pread(fd, back, kept, 0) == (ssize_t)kept && !memcmp(back, data, kept);
    sleep((unsigned)seconds);
    ssize_t second = pwrite(fd, data, size, 0);
    same = same && pread(fd, back, size, 0) == (ssize_t)size && !memcmp(back, data, size);
    printf("a megabyte written with less than that free returned %zd (%s), then %zd once there was room\n", first,
           strerror(refused), second);
    close(fd);
    return !(first > 0 && first < (ssize_t)size && second == (ssize_t)size && same);
}

static int
hold_readers(const char *path, const char *how, int seconds)
{
    struct radvisory ra = { .ra_offset = 0, .ra_count = 65536 };
    char byte;
    int wfd = open(path, O_WRONLY), rc = wfd < 0;
    if (!rc && !strcmp(how, "advise"))
        rc = fcntl(wfd, F_RDADVISE, &ra) != 0;
    if (!rc && !strcmp(how, "read-close")) {
        int rfd = open(path, O_RDONLY);
        rc = rfd < 0 || read(rfd, &byte, 1) != 1;
        sleep(2);
        rc |= rfd < 0 || close(rfd) != 0;
    }
    printf("%s beside a writer: %s\n", how, rc ? strerror(errno) : "ok");
    fflush(stdout);
    sleep((unsigned)seconds);
    if (wfd >= 0)
        close(wfd);
    return rc;
}

static int
advise_close(const char *path, int mib)
{
    struct radvisory ra = { .ra_offset = 0, .ra_count = mib << 20 };
    int fd = open(path, O_RDONLY);
    if (fd < 0 || fcntl(fd, F_RDADVISE, &ra) != 0) {
        printf("read advice: %s\n", strerror(errno));
        return 1;
    }
    return close(fd) != 0;
}

struct reopener {
    const char *path;
    _Atomic int *go;
    int round, delay_us;
    off_t base, span;
    long bad;
};

static void *
reopen_one(void *arg)
{
    struct reopener *o = arg;
    unsigned char *buf = malloc(1 << 20);
    while (atomic_load(o->go) != o->round)
        sched_yield();
    for (double t = now() + o->delay_us / 1e6; now() < t;)
        ;
    int fd = open(o->path, O_RDONLY);
    if (!buf || fd < 0 || fcntl(fd, F_NOCACHE, 1) != 0)
        o->bad++;
    unsigned seed = (unsigned)o->delay_us * 2654435761u;
    for (double until = now() + 0.25; !o->bad && now() < until;) {
        seed = seed * 1103515245u + 12345u;
        off_t off = o->base + (off_t)(seed % (unsigned)(o->span >> 20)) * (1 << 20);
        o->bad += pread(fd, buf, 1 << 20, off) != 1 << 20;
    }
    if (fd >= 0)
        close(fd);
    free(buf);
    return NULL;
}

static int
reopen_close(const char *path, int rounds)
{
    static char buf[128 << 10];
    _Atomic int go = 0;
    struct stat st;
    double longest = 0;
    long bad = 0;
    int waited = 0, slow = 0;
    off_t at = 0;

    if (stat(path, &st) != 0)
        return 1;
    for (int r = 1; r <= rounds; r++) {
        pthread_t tid[8];
        struct reopener o[8];
        int n = 0;
        for (; n < 8; n++) {
            o[n] = (struct reopener){ path, &go, r, 20 + 10 * n, st.st_size - (8 << 20), 8 << 20, 0 };
            if (pthread_create(&tid[n], NULL, reopen_one, &o[n]) != 0)
                break;
        }
        bad += n < 8;
        int fd = open(path, O_RDONLY);
        for (int i = 0; fd >= 0 && i < 4; i++, at += (off_t)sizeof(buf))
            bad += pread(fd, buf, sizeof(buf), at) != (ssize_t)sizeof(buf);
        atomic_store(&go, r);
        double t0 = now();
        bad += fd < 0 || close(fd) != 0;
        double took = now() - t0;
        for (int i = 0; i < n; i++) {
            pthread_join(tid[i], NULL);
            bad += o[i].bad;
        }
        longest = took > longest ? took : longest;
        waited += took > 20e-6;
        slow += took > 0.05;
    }
    printf("reopen-close: %d rounds, %d closes over 20 us, longest %.2f ms, %d over 50 ms, %ld failures\n", rounds,
           waited, longest * 1e3, slow, bad);
    return slow || bad;
}

static size_t
resident(void *map, size_t len, char *vec)
{
    size_t pages = (len + (size_t)getpagesize() - 1) / (size_t)getpagesize(), n = 0;

    if (mincore(map, len, vec) != 0)
        return 0;
    for (size_t i = 0; i < pages; i++)
        n += (vec[i] & MINCORE_INCORE) != 0;
    return n;
}

static int
rdadvise(const char *path)
{
    struct stat st;
    int fd = open(path, O_RDONLY);
    if (fd < 0 || fstat(fd, &st) != 0 || st.st_size == 0)
        return 1;
    size_t len = (size_t)st.st_size, pages = (len + (size_t)getpagesize() - 1) / (size_t)getpagesize();
    void *map = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
    char *vec = malloc(pages);
    if (map == MAP_FAILED || !vec)
        return 1;
    size_t before = resident(map, len, vec), after = 0;
    struct radvisory ra = { 0, (int)len }, past = { st.st_size, 1 };
    int err = fcntl(fd, F_RDADVISE, &ra) == 0 ? 0 : errno;
    double t0 = now();
    while (!err && (after = resident(map, len, vec)) < pages && now() < t0 + 2)
        usleep(1000);
    double took = now() - t0;
    int past_err = fcntl(fd, F_RDADVISE, &past) == 0 ? 0 : errno;
    printf("rdadvise: %zu of %zu pages resident before, %zu %.1f ms after the advice, which gave %s, %s past the end\n",
           before, pages, after, took * 1e3, err ? strerror(err) : "no error",
           past_err ? strerror(past_err) : "no error");
    munmap(map, len);
    close(fd);
    return !(before == 0 && !err && after == pages && past_err == EFBIG);
}

static int
hold(const char *path, int seconds)
{
    char buf[4096];
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("open: %s\n", strerror(errno));
        return 1;
    }
    sleep((unsigned)seconds);
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n < 0)
        printf("read: %s\n", strerror(errno));
    else
        printf("%zd bytes\n", n);
    close(fd);
    return n < 0;
}

static int
classic_readdir(const char *dir)
{
    struct old_dirent {
        uint32_t d_ino;
        uint16_t d_reclen;
        uint8_t d_type;
        uint8_t d_namlen;
        char d_name[256];
    };
    static char buf[8192];
    int fd = open(dir, O_RDONLY | O_DIRECTORY);
    long base = 0, listed = 0, want = 0;
    int got;

    if (fd < 0)
        return 1;
    while ((got = syscall(SYS_getdirentries, fd, buf, sizeof(buf), &base)) > 0) {
        for (int at = 0; at < got; at += ((struct old_dirent *)(buf + at))->d_reclen)
            listed++;
    }
    int err = got < 0 ? errno : 0;
    close(fd);
    DIR *d = opendir(dir);
    struct dirent *de;
    while (d && (de = readdir(d)) != NULL)
        want += de->d_namlen <= 255;
    if (d)
        closedir(d);
    printf("old getdirentries: %ld of %ld entries with names up to 255 bytes, %s\n", listed, want,
           err ? strerror(err) : "end");
    return !err && listed == want ? 0 : 1;
}

static int
mount_as(uid_t uid, gid_t gid, const char *spec, const char *dir)
{
    struct {
        const char *fspec;
    } args = { spec };
    if (setgid(gid) != 0 || setuid(uid) != 0)
        return 1;
    if (mount("virtiofs", dir, MNT_RDONLY, &args) != 0) {
        printf("mount: %s\n", strerror(errno));
        return 0;
    }
    printf("mounted\n");
    return unmount(dir, 0) != 0;
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

static int
bench_random(const char *path, int count)
{
    struct stat st;
    int fd = open(path, O_RDONLY);
    size_t page = (size_t)getpagesize();
    void *buf = valloc(page);
    if (count <= 0 || fd < 0 || fstat(fd, &st) != 0 || st.st_size < (off_t)page || !buf ||
        fcntl(fd, F_NOCACHE, 1) != 0)
        return 1;
    size_t pages = (size_t)st.st_size / page;
    unsigned seed = 1;
    double t0 = now();
    for (int i = 0; i < count; i++) {
        seed = seed * 1103515245u + 12345u;
        if (pread(fd, buf, page, (off_t)(seed % pages * page)) != (ssize_t)page)
            return 1;
    }
    double direct = (now() - t0) / count * 1e6;
    char *map = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED || madvise(map, (size_t)st.st_size, MADV_RANDOM) != 0)
        return 1;
    t0 = now();
    for (int i = 0; i < count; i++) {
        seed = seed * 1103515245u + 12345u;
        (void)((volatile const char *)map)[seed % pages * page];
    }
    double touched = (now() - t0) / count * 1e6;
    printf("%s: %.1f us per uncached %zu KiB read at a random offset, %.1f us per first touch of a random page\n",
           path, direct, page >> 10, touched);
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
bench_cold(const char *path, int count)
{
    struct stat st;
    int fd = open(path, O_RDONLY);
    size_t page = (size_t)getpagesize();
    void *buf = valloc(page);
    if (fd < 0 || fstat(fd, &st) != 0 || !buf || count <= 0 || (off_t)count * (off_t)page > st.st_size ||
        fcntl(fd, F_RDAHEAD, 0) != 0)
        return 1;
    size_t pages = (size_t)st.st_size / page;
    double t0 = now();
    for (int i = 0; i < count; i++) {
        if (pread(fd, buf, page, (off_t)(((size_t)i * 769 + 13) % pages * page)) != (ssize_t)page)
            return 1;
    }
    printf("%s: %.1f us per first read of %zu KiB at a random offset\n", path, (now() - t0) / count * 1e6, page >> 10);
    return 0;
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
    double opened = (now() - t0) / (double)n * 1e6, reads[2];
    static char small[65536];
    for (int pass = 0; pass < 2; pass++) {
        t0 = now();
        for (size_t i = 0; i < n; i++) {
            snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
            int fd = open(path, O_RDONLY);
            if (fd >= 0 && read(fd, small, sizeof(small)) < 0)
                printf("read %s: %s\n", path, strerror(errno));
            close(fd);
        }
        reads[pass] = (now() - t0) / (double)n * 1e6;
    }
    printf("%s: readdir of %zu entries %.2f ms, lstat %.1f us each first, then %.1f us at %.2f CPUs busy, "
           "open+close %.1f us each, open+read+close %.1f us each first and then %.1f us\n", dir, n, listed * 1e3, cold,
           warm, busy, opened, reads[0], reads[1]);
    return 0;
}

static unsigned
seed_of(const char *name)
{
    return (unsigned)fnv(0xcbf29ce484222325ULL, (const unsigned char *)name, strlen(name));
}

static void
fill(unsigned seed, off_t at, unsigned char *buf, size_t n)
{
    for (size_t i = 0; i < n; i++)
        buf[i] = (unsigned char)((((uint64_t)seed << 32) ^ (uint64_t)(at + (off_t)i)) * 0x9e3779b97f4a7c15ULL >> 56);
}

static int
write_at(int fd, unsigned seed, off_t at, size_t size)
{
    unsigned char *buf = malloc(size ? size : 1);
    fill(seed, at, buf, size);
    ssize_t n = pwrite(fd, buf, size, at);
    free(buf);
    return n == (ssize_t)size ? 0 : -1;
}

static int
put(const char *name, size_t size, size_t step, mode_t mode)
{
    int fd = open(name, O_WRONLY | O_CREAT | O_EXCL, 0600);
    CHECK(fd >= 0, "create %s: %s", name, strerror(errno));
    if (fd < 0)
        return -1;
    unsigned char *buf = malloc(step);
    for (size_t done = 0; done < size;) {
        size_t n = size - done < step ? size - done : step;
        fill(seed_of(name), (off_t)done, buf, n);
        ssize_t w = write(fd, buf, n);
        CHECK(w == (ssize_t)n, "write %s at %zu: %zd %s", name, done, w, strerror(errno));
        if (w != (ssize_t)n)
            break;
        done += n;
    }
    free(buf);
    CHECK(fchmod(fd, mode) == 0, "fchmod %s: %s", name, strerror(errno));
    CHECK(close(fd) == 0, "close %s: %s", name, strerror(errno));
    return 0;
}

static int stamps;

static void
stamp(const char *path)
{
    struct stat st;
    CHECK(lstat(path, &st) == 0, "lstat %s: %s", path, strerror(errno));
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        char **names = NULL;
        size_t n = 0, cap = 0;
        struct dirent *de;
        while (d && (de = readdir(d)) != NULL) {
            if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
                continue;
            if (n == cap)
                names = realloc(names, (cap = cap ? cap * 2 : 64) * sizeof(*names));
            names[n++] = strdup(de->d_name);
        }
        if (d)
            closedir(d);
        qsort(names, n, sizeof(*names), byname);
        for (size_t i = 0; i < n; i++) {
            char child[2048];
            snprintf(child, sizeof(child), "%s/%s", path, names[i]);
            stamp(child);
            free(names[i]);
        }
        free(names);
    }
    struct timespec ts[2] = { { 981173106 + stamps, 123456789 }, { 981173106 + stamps, 987654321 } };
    stamps++;
    CHECK(utimensat(AT_FDCWD, path, ts, AT_SYMLINK_NOFOLLOW) == 0, "utimensat %s: %s", path, strerror(errno));
}

static int
write_tree(const char *root)
{
    static const size_t sizes[] = { 0, 1, 4095, 4096, 4097, 16383, 16384, 16385, 131071, 131072, 131073, 1048583 };
    char name[64];
    int fd;

    CHECK(mkdir(root, 0755) == 0 && chdir(root) == 0, "mkdir %s: %s", root, strerror(errno));
    CHECK(mkdir("files", 0755) == 0 && mkdir("chunks", 0755) == 0, "mkdir: %s", strerror(errno));
    for (size_t i = 0; i < sizeof(sizes) / sizeof(*sizes); i++) {
        snprintf(name, sizeof(name), "files/%zu", sizes[i]);
        put(name, sizes[i], sizes[i] ? sizes[i] : 1, 0644);
        snprintf(name, sizeof(name), "chunks/%zu", sizes[i]);
        put(name, sizes[i], 1000, 0644);
    }

    put("inplace", 50000, 50000, 0644);
    fd = open("inplace", O_WRONLY);
    CHECK(fd >= 0 && write_at(fd, 7, 16000, 3000) == 0 && write_at(fd, 8, 49990, 20) == 0, "inplace: %s",
          strerror(errno));
    close(fd);

    put("rewritten", 20000, 20000, 0644);
    fd = open("rewritten", O_WRONLY);
    unsigned char head[5000];
    fill(11, 0, head, sizeof(head));
    CHECK(fd >= 0 && write(fd, head, sizeof(head)) == (ssize_t)sizeof(head), "rewritten: %s", strerror(errno));
    close(fd);

    put("otrunc", 50000, 50000, 0644);
    fd = open("otrunc", O_WRONLY | O_TRUNC);
    CHECK(fd >= 0 && write(fd, head, 100) == 100, "otrunc: %s", strerror(errno));
    close(fd);

    put("truncated", 100000, 100000, 0644);
    CHECK(truncate("truncated", 5000) == 0, "truncate: %s", strerror(errno));
    fd = open("truncated", O_WRONLY);
    CHECK(fd >= 0 && ftruncate(fd, 20000) == 0 && write_at(fd, 9, 30000, 10) == 0, "ftruncate: %s",
          strerror(errno));
    close(fd);

    fd = open("appended", O_WRONLY | O_CREAT | O_EXCL | O_APPEND, 0644);
    for (int i = 0; i < 10; i++) {
        unsigned char rec[777];
        fill(20 + (unsigned)i, 0, rec, sizeof(rec));
        lseek(fd, 0, SEEK_SET);
        CHECK(write(fd, rec, sizeof(rec)) == (ssize_t)sizeof(rec), "append %d: %s", i, strerror(errno));
    }
    close(fd);

    put("mapped", 40000, 40000, 0644);
    fd = open("mapped", O_RDWR);
    char *map = fd < 0 ? MAP_FAILED : mmap(NULL, 40000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    CHECK(map != MAP_FAILED, "mmap: %s", strerror(errno));
    if (map != MAP_FAILED) {
        memset(map, 'A', 100);
        memset(map + 16380, 'B', 11);
        memset(map + 39990, 'C', 10);
        CHECK(msync(map, 40000, MS_SYNC) == 0, "msync: %s", strerror(errno));
        memset(map + 20000, 'D', 11);
        munmap(map, 40000);
    }
    close(fd);

    fd = open("far", O_RDWR | O_CREAT | O_EXCL, 0644);
    char far[3] = { 0 }, hole[3] = { 1, 1, 1 };
    struct stat st;
    CHECK(fd >= 0 && pwrite(fd, "far", 3, 5LL << 30) == 3, "write past 4 GiB: %s", strerror(errno));
    CHECK(fstat(fd, &st) == 0 && st.st_size == (5LL << 30) + 3, "size past 4 GiB %lld", (long long)st.st_size);
    CHECK(pread(fd, far, 3, 5LL << 30) == 3 && !memcmp(far, "far", 3), "read past 4 GiB");
    CHECK(pread(fd, hole, 3, 4LL << 30) == 3 && !hole[0] && !hole[1] && !hole[2], "read of a hole");
    CHECK(ftruncate(fd, 4096) == 0, "ftruncate from past 4 GiB: %s", strerror(errno));
    close(fd);

    CHECK(mkdir("d", 0755) == 0 && mkdir("d/a", 0755) == 0 && mkdir("d/a/b", 0755) == 0 &&
          mkdir("d/a/b/c", 0755) == 0 && mkdir("d700", 0700) == 0 && mkdir("d750", 0750) == 0,
          "mkdir: %s", strerror(errno));
    CHECK(chmod("d700", 0700) == 0 && chmod("d750", 0750) == 0, "chmod of a directory: %s", strerror(errno));
    char longer[1001];
    memset(longer, 't', 1000);
    longer[1000] = 0;
    CHECK(symlink("files/4097", "l-rel") == 0 && symlink("/etc/hosts", "l-abs") == 0 &&
          symlink("nowhere", "l-dangling") == 0 && symlink(longer, "l-long") == 0, "symlink: %s", strerror(errno));
    CHECK(link("files/4096", "hard") == 0, "link: %s", strerror(errno));
    CHECK(mkfifo("fifo", 0644) == 0 && chmod("fifo", 0644) == 0, "mkfifo: %s", strerror(errno));

    put("r1", 10, 10, 0644);
    CHECK(rename("r1", "r2") == 0, "rename: %s", strerror(errno));
    put("r3", 20, 20, 0644);
    put("r4", 30, 30, 0644);
    CHECK(rename("r3", "r4") == 0, "rename over a file: %s", strerror(errno));
    CHECK(mkdir("rd1", 0755) == 0, "mkdir rd1: %s", strerror(errno));
    put("rd1/x", 5, 5, 0644);
    CHECK(rename("rd1", "d/rd2") == 0, "rename of a directory: %s", strerror(errno));
    put("moved", 7, 7, 0644);
    CHECK(rename("moved", "d/a/moved") == 0, "rename into another directory: %s", strerror(errno));
    put("gone", 100, 100, 0644);
    CHECK(unlink("gone") == 0, "unlink: %s", strerror(errno));
    CHECK(mkdir("gonedir", 0755) == 0 && rmdir("gonedir") == 0, "rmdir: %s", strerror(errno));
    put("exec", 100, 100, 0644);
    CHECK(chmod("exec", 0755) == 0 && chmod("files/1", 0600) == 0 && chmod("files/0", 0444) == 0,
          "chmod: %s", strerror(errno));
    stamp(".");
    chdir("/");
    printf("write-tree: %d failures\n", failures);
    return failures ? 1 : 0;
}

static void
outcome(const char *what, int rc)
{
    printf("%s: %s\n", what, rc < 0 ? strerror(errno) : "ok");
}

static int
rw_errors(const char *root)
{
    char buf[128], name[300];
    int fd;

    if (mkdir(root, 0755) != 0 || chdir(root) != 0)
        return 1;
    outcome("mkdir", mkdir("d", 0755));
    outcome("mkdir of an existing name", mkdir("d", 0755));
    fd = open("d/f", O_WRONLY | O_CREAT | O_EXCL, 0644);
    outcome("create", fd);
    close(fd);
    outcome("rmdir of a directory that is not empty", rmdir("d"));
    outcome("unlink of a directory", unlink("d"));
    outcome("rmdir of a file", rmdir("d/f"));
    outcome("rename of a directory into itself", rename("d", "d/sub"));
    outcome("exclusive create of an existing file", open("d/f", O_WRONLY | O_CREAT | O_EXCL, 0644));
    fd = open("d/f", O_RDONLY);
    outcome("write to a descriptor open for reading", (int)write(fd, "x", 1));
    close(fd);
    outcome("truncate of a directory", truncate("d", 0));
    outcome("open of a directory for writing", open("d", O_WRONLY));
    outcome("hard link to a directory", link("d", "dl"));
    outcome("hard link over an existing name", link("d/f", "d/f"));
    memset(name, 'n', 256);
    name[256] = 0;
    outcome("create of a 256-byte name", open(name, O_WRONLY | O_CREAT, 0644));
    name[255] = 0;
    fd = open(name, O_WRONLY | O_CREAT, 0644);
    outcome("create of a 255-byte name", fd);
    close(fd);
    outcome("unlink of a 255-byte name", unlink(name));
    outcome("mkdir under a missing directory", mkdir("missing/x", 0755));
    outcome("rename of a missing name", rename("missing", "x"));
    outcome("unlink of a missing name", unlink("missing"));
    fd = open("a", O_WRONLY | O_CREAT | O_EXCL, 0644);
    close(fd);
    outcome("mkdir dir2", mkdir("dir2", 0755));
    outcome("rename of a file over a directory", rename("a", "dir2"));
    outcome("rename of a directory over a file", rename("dir2", "a"));
    outcome("mkdir dir3", mkdir("dir3", 0755));
    fd = open("dir3/x", O_WRONLY | O_CREAT | O_EXCL, 0644);
    close(fd);
    outcome("rename over a directory that is not empty", rename("dir2", "dir3"));
    outcome("mkdir d/empty", mkdir("d/empty", 0755));
    outcome("rename over an empty directory", rename("dir2", "d/empty"));
    outcome("symlink over an existing name", symlink("x", "a"));
    outcome("symlink loop", symlink("loop", "loop"));
    outcome("open of a symlink loop", open("loop", O_RDONLY));
    outcome("getxattr of a missing attribute", (int)getxattr("a", "user.none", buf, sizeof(buf), 0, 0));
    outcome("setxattr", setxattr("a", "user.x", "1", 1, 0, XATTR_CREATE));
    outcome("XATTR_CREATE of an existing attribute", setxattr("a", "user.x", "2", 1, 0, XATTR_CREATE));
    outcome("XATTR_REPLACE of a missing attribute", setxattr("a", "user.y", "2", 1, 0, XATTR_REPLACE));
    outcome("removexattr of a missing attribute", removexattr("a", "user.none", 0));
    memset(buf, 'v', 100);
    outcome("setxattr of 100 bytes", setxattr("a", "user.big", buf, 100, 0, 0));
    outcome("getxattr into a buffer too small", (int)getxattr("a", "user.big", buf, 10, 0, 0));
    outcome("getxattr into a buffer of size 0", (int)getxattr("a", "user.big", buf, 0, 0, 0));
    outcome("listxattr into a buffer of size 0", (int)listxattr("a", buf, 0, 0));
    outcome("getxattr of a directory's missing attribute", (int)getxattr("d", "user.none", buf, sizeof(buf), 0, 0));
    outcome("chflags 0", chflags("a", 0));
    fd = open("case", O_WRONLY | O_CREAT | O_EXCL, 0644);
    close(fd);
    long sensitive = pathconf("case", _PC_CASE_SENSITIVE);
    printf("case sensitive: %ld\n", sensitive);
    outcome("rename to another case", rename("case", "CASE"));
    DIR *d = opendir(".");
    struct dirent *de;
    while (d && (de = readdir(d)) != NULL)
        if (!strcasecmp(de->d_name, "case"))
            printf("listed after the rename: %s\n", de->d_name);
    if (d)
        closedir(d);
    struct stat st;
    outcome("lookup in another case", stat("cAsE", &st));
    chdir("/");
    return 0;
}

static int
ro_share(const char *mnt)
{
    char p[1024], q[1024];

    snprintf(p, sizeof(p), "%s/small/4097", mnt);
    snprintf(q, sizeof(q), "%s/created", mnt);
    outcome("create", open(q, O_WRONLY | O_CREAT, 0644));
    outcome("mkdir", mkdir(q, 0755));
    outcome("symlink", symlink("x", q));
    outcome("link", link(p, q));
    outcome("rename", rename(p, q));
    outcome("unlink", unlink(p));
    outcome("open for writing", open(p, O_WRONLY));
    outcome("truncate", truncate(p, 0));
    outcome("chmod", chmod(p, 0600));
    outcome("utimes", utimes(p, NULL));
    outcome("setxattr", setxattr(p, "user.test", "v", 1, 0, 0));
    snprintf(q, sizeof(q), "%s/small", mnt);
    outcome("rmdir", rmdir(q));
    return 0;
}

static int
xattr_list(const char *path, int all)
{
    char names[65536], value[65536];
    ssize_t n = listxattr(path, names, sizeof(names), XATTR_NOFOLLOW);
    if (n < 0) {
        printf("listxattr: %s\n", strerror(errno));
        return 1;
    }
    char *list[256];
    int count = 0;
    for (char *p = names; p < names + n && count < 256; p += strlen(p) + 1)
        if (all || strcmp(p, XATTR_PROVENANCE))
            list[count++] = p;
    qsort(list, (size_t)count, sizeof(*list), byname);
    for (int i = 0; i < count; i++) {
        ssize_t size = getxattr(path, list[i], NULL, 0, 0, XATTR_NOFOLLOW);
        ssize_t got = getxattr(path, list[i], value, sizeof(value), 0, XATTR_NOFOLLOW);
        printf("%s size=%zd got=%zd fnv=%016llx\n", list[i], size, got,
               (unsigned long long)fnv(0xcbf29ce484222325ULL, (unsigned char *)value, got > 0 ? (size_t)got : 0));
    }
    return 0;
}

static int
xattrs(const char *path)
{
    unsigned char big[65536];
    fill(30, 0, big, sizeof(big));
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
    CHECK(fd >= 0, "create %s: %s", path, strerror(errno));
    close(fd);
    CHECK(setxattr(path, "user.empty", "", 0, 0, 0) == 0, "empty value: %s", strerror(errno));
    CHECK(setxattr(path, "user.one", "1", 1, 0, 0) == 0, "one byte: %s", strerror(errno));
    CHECK(setxattr(path, "com.example.big", big, sizeof(big), 0, 0) == 0, "64 KiB value: %s", strerror(errno));
    CHECK(setxattr(path, "user.replaced", "old", 3, 0, 0) == 0, "set: %s", strerror(errno));
    CHECK(setxattr(path, "user.replaced", "newer", 5, 0, XATTR_REPLACE) == 0, "replace: %s", strerror(errno));
    CHECK(setxattr(path, "user.removed", "x", 1, 0, 0) == 0, "set: %s", strerror(errno));
    CHECK(removexattr(path, "user.removed", 0) == 0, "remove: %s", strerror(errno));
    unsigned char back[65536];
    CHECK(getxattr(path, "com.example.big", back, sizeof(back), 0, 0) == (ssize_t)sizeof(big) &&
          !memcmp(back, big, sizeof(big)), "64 KiB value read back");
    xattr_list(path, 0);
    return failures ? 1 : 0;
}

static int
coherence(const char *path)
{
    unsigned char data[65536], got[4];
    fill(40, 0, data, sizeof(data));
    int fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0644);
    CHECK(fd >= 0 && write(fd, data, sizeof(data)) == (ssize_t)sizeof(data), "write: %s", strerror(errno));
    char *map = mmap(NULL, sizeof(data), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    CHECK(map != MAP_FAILED, "mmap: %s", strerror(errno));
    if (fd < 0 || map == MAP_FAILED)
        return 1;
    CHECK(!memcmp(map, data, sizeof(data)), "a mapping sees what write wrote");
    CHECK(pwrite(fd, "xyz", 3, 20000) == 3 && !memcmp(map + 20000, "xyz", 3), "a mapping sees a later write");
    memcpy(map + 30000, "MAP", 3);
    CHECK(msync(map, sizeof(data), MS_SYNC) == 0 && pread(fd, got, 3, 30000) == 3 && !memcmp(got, "MAP", 3),
          "read sees a store through the mapping after msync");
    memcpy(map + 40000, "NOS", 3);
    CHECK(pread(fd, got, 3, 40000) == 3 && !memcmp(got, "NOS", 3), "read sees a store through the mapping at once");
    CHECK(pwrite(fd, "PW", 2, 40001) == 2 && !memcmp(map + 40000, "NPW", 3),
          "a write next to a store through the mapping keeps both");
    map[5000] = 'K';
    map[30000] = 'L';
    CHECK(ftruncate(fd, 10000) == 0 && pread(fd, got, 1, 9999) == 1 && pread(fd, got, 1, 10000) == 0,
          "a shrunk file ends at its new size");
    CHECK(pread(fd, got, 1, 5000) == 1 && got[0] == 'K', "a store before the cut point survives the truncation");
    CHECK(ftruncate(fd, sizeof(data)) == 0 && map[20000] == 0 && map[9999] == (char)data[9999],
          "a file grown again reads zeros where it was cut");
    munmap(map, sizeof(data));
    close(fd);
    int again = open(path, O_RDONLY);
    CHECK(again >= 0 && pread(again, got, 3, 40000) == 3 && !got[0], "the cut part stays zero after close");
    close(again);
    printf("coherence: %d failures\n", failures);
    return failures ? 1 : 0;
}

static int
open_unlink(const char *dir)
{
    char held[1024], moved[1024], buf[8] = { 0 };
    struct stat st;

    snprintf(held, sizeof(held), "%s/held", dir);
    snprintf(moved, sizeof(moved), "%s/moved", dir);
    CHECK(mkdir(dir, 0755) == 0, "mkdir %s: %s", dir, strerror(errno));
    int fd = open(held, O_RDWR | O_CREAT | O_EXCL, 0644);
    CHECK(fd >= 0 && write(fd, "one", 3) == 3, "create: %s", strerror(errno));
    CHECK(unlink(held) == 0, "unlink of an open file: %s", strerror(errno));
    CHECK(access(held, F_OK) != 0, "the removed name is gone");
    CHECK(write(fd, "two", 3) == 3, "write after the unlink: %s", strerror(errno));
    CHECK(pread(fd, buf, 6, 0) == 6 && !memcmp(buf, "onetwo", 6), "read after the unlink: %s", strerror(errno));
    CHECK(fstat(fd, &st) == 0 && st.st_size == 6, "fstat after the unlink: size %lld", (long long)st.st_size);
    CHECK(fsync(fd) == 0 && fcntl(fd, F_FULLFSYNC) == 0 && fcntl(fd, F_BARRIERFSYNC) == 0, "sync: %s",
          strerror(errno));
    CHECK(close(fd) == 0, "close: %s", strerror(errno));
    fd = open(held, O_RDWR | O_CREAT | O_EXCL, 0644);
    CHECK(fd >= 0 && write(fd, "abc", 3) == 3, "create again: %s", strerror(errno));
    CHECK(rename(held, moved) == 0, "rename of an open file: %s", strerror(errno));
    CHECK(write(fd, "def", 3) == 3, "write after the rename: %s", strerror(errno));
    CHECK(close(fd) == 0, "close: %s", strerror(errno));
    memset(buf, 0, sizeof(buf));
    fd = open(moved, O_RDONLY);
    CHECK(fd >= 0 && read(fd, buf, 6) == 6 && !memcmp(buf, "abcdef", 6), "the renamed file has both writes");
    close(fd);
    printf("open-unlink: %d failures\n", failures);
    return failures ? 1 : 0;
}

static int
map_pressure(const char *path, int mib)
{
    size_t size = (size_t)mib << 20, page = (size_t)getpagesize(), bad = 0;
    int fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0644);
    CHECK(fd >= 0 && ftruncate(fd, (off_t)size) == 0, "create %s: %s", path, strerror(errno));
    unsigned char *map = fd < 0 ? MAP_FAILED : mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    CHECK(map != MAP_FAILED, "mmap: %s", strerror(errno));
    if (map == MAP_FAILED)
        return 1;
    double t0 = now();
    for (size_t at = 0; at < size; at += page) {
        uint64_t stamp = (at / page + 1) * 0x9e3779b97f4a7c15ULL;
        memcpy(map + at, &stamp, sizeof(stamp));
        memcpy(map + at + page - sizeof(stamp), &stamp, sizeof(stamp));
    }
    CHECK(munmap(map, size) == 0 && close(fd) == 0, "munmap: %s", strerror(errno));
    double stored = now() - t0;
    unsigned char *buf = malloc(1 << 20);
    fd = open(path, O_RDONLY);
    CHECK(fd >= 0 && fcntl(fd, F_NOCACHE, 1) == 0, "reopen: %s", strerror(errno));
    for (size_t at = 0; fd >= 0 && at < size; at += 1 << 20) {
        if (pread(fd, buf, 1 << 20, (off_t)at) != 1 << 20) {
            bad += (1 << 20) / page;
            continue;
        }
        for (size_t p = 0; p < 1 << 20; p += page) {
            uint64_t stamp = ((at + p) / page + 1) * 0x9e3779b97f4a7c15ULL, head, tail;
            memcpy(&head, buf + p, sizeof(head));
            memcpy(&tail, buf + p + page - sizeof(tail), sizeof(tail));
            bad += head != stamp || tail != stamp || buf[p + page / 2] != 0;
        }
    }
    close(fd);
    free(buf);
    CHECK(bad == 0, "%zu of %zu pages differ", bad, size / page);
    CHECK(unlink(path) == 0, "unlink: %s", strerror(errno));
    printf("map-pressure: %d MiB stored through a mapping in %.1f s, %zu pages wrong\n", mib, stored, bad);
    return failures ? 1 : 0;
}

struct storer {
    int fd;
    off_t at;
    int rounds;
    atomic_int go;
    atomic_int done;
};

static void *
race_write(void *arg)
{
    struct storer *r = arg;
    for (int i = 1; i <= r->rounds; i++) {
        while (atomic_load(&r->go) != i)
            sched_yield();
        char c = (char)('A' + i % 26);
        if (r->at < 0)
            ftruncate(r->fd, 10000);
        else
            pwrite(r->fd, &c, 1, r->at);
        atomic_store(&r->done, i);
    }
    return NULL;
}

static void
store_race(const char *name, off_t at)
{
    char page[4000];
    int fd = open(name, O_RDWR | O_CREAT | O_EXCL, 0644);
    memset(page, 'x', sizeof(page));
    CHECK(fd >= 0 && write(fd, page, sizeof(page)) == (ssize_t)sizeof(page), "create %s: %s", name, strerror(errno));
    char *map = fd < 0 ? MAP_FAILED : mmap(NULL, 16384, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    CHECK(map != MAP_FAILED, "mmap %s: %s", name, strerror(errno));
    if (map == MAP_FAILED) {
        close(fd);
        return;
    }
    struct storer r = { .fd = fd, .at = at, .rounds = 1000 };
    pthread_t t;
    pthread_create(&t, NULL, race_write, &r);
    int lost = 0, wrote = 0, stale = 0;
    for (int i = 1; i <= r.rounds; i++) {
        char want = (char)('a' + i % 26), back = 0;
        if (at < 0)
            map[5000] = 'Z';
        uint64_t until = clock_gettime_nsec_np(CLOCK_UPTIME_RAW) + arc4random_uniform(200000);
        atomic_store(&r.go, i);
        while (clock_gettime_nsec_np(CLOCK_UPTIME_RAW) < until)
            ;
        map[100] = want;
        while (atomic_load(&r.done) != i)
            sched_yield();
        if (msync(map, 16384, MS_SYNC) != 0 || pread(fd, &back, 1, 100) != 1 || back != want)
            lost++;
        if (at >= 0 && (pread(fd, &back, 1, at) != 1 || back != (char)('A' + i % 26)))
            wrote++;
        if (at < 0 && (pread(fd, &back, 1, 5000) != 1 || back != 0))
            stale++;
        ftruncate(fd, sizeof(page));
    }
    pthread_join(t, NULL);
    munmap(map, 16384);
    close(fd);
    CHECK(lost == 0 && wrote == 0 && stale == 0,
          "%s: of %d rounds, %d stores and %d writes beside them were lost, %d stores past the end came back", name,
          r.rounds, lost, wrote, stale);
}

struct seer {
    int fd;
    int rounds;
    size_t size;
    off_t at;
    volatile uint32_t *counter;
    atomic_int go;
    atomic_int done;
    atomic_int stop;
    atomic_int failed;
};

static void *
seen_store(void *arg)
{
    struct seer *s = arg;
    while (!atomic_load(&s->stop))
        s->counter[25]++;
    return NULL;
}

static void *
seen_write(void *arg)
{
    struct seer *s = arg;
    char *data = malloc(s->size);
    for (int i = 1; i <= s->rounds; i++) {
        while (atomic_load(&s->go) != i)
            sched_yield();
        if (data)
            memset(data, 'A' + i % 26, s->size);
        if (!data || pwrite(s->fd, data, s->size, s->at) != (ssize_t)s->size)
            atomic_store(&s->failed, 1);
        atomic_store(&s->done, i);
    }
    free(data);
    return NULL;
}

static const char *peer;

static int
peer_open(const char *name, int flags)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", peer, name);
    return open(path, flags);
}

static bool
on_host(const char *name, off_t at, char want)
{
    char back = 0;
    int held = peer_open(name, O_RDONLY), fd = peer_open(name, O_RDONLY);
    bool same = fd >= 0 && pread(fd, &back, 1, at) == 1 && back == want;
    close(fd);
    close(held);
    return same;
}

static void
seen_race(const char *name, bool cached)
{
    static char page[20000];
    int fd = open(name, O_RDWR | O_CREAT | O_EXCL, 0644);
    memset(page, 'x', sizeof(page));
    CHECK(fd >= 0 && write(fd, page, sizeof(page)) == (ssize_t)sizeof(page), "create %s: %s", name, strerror(errno));
    volatile char *map = fd < 0 ? MAP_FAILED : mmap(NULL, 65536, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    struct seer s = { .fd = fd, .rounds = 1000, .size = 20000, .at = 4000, .counter = (volatile uint32_t *)map };
    struct kevent ev, got;
    struct timespec now = { 0, 0 }, wait = { 2, 0 };
    int kq = kqueue();
    EV_SET(&ev, fd, EVFILT_VNODE, EV_ADD | EV_CLEAR, NOTE_EXTEND, 0, NULL);
    pthread_t t, u;
    bool ready = map != MAP_FAILED && kq >= 0 && kevent(kq, &ev, 1, NULL, 0, NULL) == 0 &&
                 pthread_create(&t, NULL, seen_write, &s) == 0;
    CHECK(ready, "map and watch %s: %s", name, strerror(errno));
    bool storing = ready && pthread_create(&u, NULL, seen_store, &s) == 0;
    int seen = 0, lost = 0, stale = 0, failed = !storing;
    for (int i = 1; ready && i <= s.rounds; i++) {
        char want = (char)('A' + i % 26);
        while (kevent(kq, NULL, 0, &got, 1, &now) > 0)
            ;
        if (cached)
            map[20500] = 'Z';
        uint64_t touch = clock_gettime_nsec_np(CLOCK_UPTIME_RAW) + arc4random_uniform(400000);
        atomic_store(&s.go, i);
        if (!cached)
            kevent(kq, NULL, 0, &got, 1, &wait);
        while (cached && clock_gettime_nsec_np(CLOCK_UPTIME_RAW) < touch)
            ;
        uint64_t until = clock_gettime_nsec_np(CLOCK_UPTIME_RAW) + 2000000000;
        while (map[16484] != want && clock_gettime_nsec_np(CLOCK_UPTIME_RAW) < until)
            ;
        bool saw = map[16484] == want;
        stale += saw && map[20500] != want;
        if (saw) {
            map[16584] = 'M';
            map[22100] = 'N';
            failed += msync((void *)(map + 16384), 16384, MS_SYNC) != 0;
        }
        while (atomic_load(&s.done) != i)
            sched_yield();
        seen += saw;
        lost += saw && !(on_host(name, 16584, 'M') && on_host(name, 22100, 'N'));
        stale += saw && !on_host(name, 20500, want);
        failed += (!cached && ftruncate(fd, 16384) != 0) || ftruncate(fd, sizeof(page)) != 0;
    }
    atomic_store(&s.stop, 1);
    if (ready)
        pthread_join(t, NULL);
    if (storing)
        pthread_join(u, NULL);
    if (map != MAP_FAILED)
        munmap((void *)map, 65536);
    close(kq);
    close(fd);
    CHECK(seen == s.rounds && lost == 0 && stale == 0 && failed == 0 && !atomic_load(&s.failed),
          "%s: in %d rounds a mapper saw the write's bytes %d times, %d times a store made after that did not reach "
          "the Mac, %d times a store made past the old end before the write outlived it, and %d calls failed", name,
          s.rounds, seen, lost, stale, failed + atomic_load(&s.failed));
}

static void
mapped_race(const char *name)
{
    size_t size = 4 << 20;
    char *page = malloc(size);
    int fd = open(name, O_RDWR | O_CREAT | O_EXCL, 0644);
    if (page)
        memset(page, 'x', size);
    struct seer s = { .fd = fd, .rounds = 300, .size = size };
    pthread_t t;
    bool ready = page && fd >= 0 && write(fd, page, size) == (ssize_t)size && pthread_create(&t, NULL, seen_write, &s) == 0;
    free(page);
    CHECK(ready, "create %s: %s", name, strerror(errno));
    int seen = 0, lost = 0, failed = 0;
    for (int i = 1; ready && i <= s.rounds; i++) {
        char want = (char)('A' + i % 26);
        uint64_t touch = clock_gettime_nsec_np(CLOCK_UPTIME_RAW) + arc4random_uniform(8000000);
        atomic_store(&s.go, i);
        while (clock_gettime_nsec_np(CLOCK_UPTIME_RAW) < touch)
            ;
        volatile char *map = mmap(NULL, 16384, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        bool saw = map != MAP_FAILED && map[100] == want;
        if (saw) {
            map[200] = 'M';
            failed += msync((void *)map, 16384, MS_SYNC) != 0;
        }
        while (atomic_load(&s.done) != i)
            sched_yield();
        seen += saw;
        lost += saw && !on_host(name, 200, 'M');
        failed += map == MAP_FAILED || msync((void *)map, 16384, MS_INVALIDATE) != 0 || munmap((void *)map, 16384) != 0;
    }
    if (ready)
        pthread_join(t, NULL);
    close(fd);
    CHECK(seen > 0 && lost == 0 && failed == 0 && !atomic_load(&s.failed),
          "%s: in %d rounds a mapping made during the write saw its bytes %d times, %d times a store made after that "
          "did not reach the Mac, and %d calls failed", name, s.rounds, seen, lost, failed + atomic_load(&s.failed));
}

static void
gap_write(const char *name)
{
    static char page[20000];
    int fd = open(name, O_RDWR | O_CREAT | O_EXCL, 0644);
    memset(page, 'x', sizeof(page));
    bool made = fd >= 0 && write(fd, page, sizeof(page)) == (ssize_t)sizeof(page);
    volatile char *map = made ? mmap(NULL, 32768, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0) : MAP_FAILED;
    int host = made ? peer_open(name, O_WRONLY | O_APPEND) : -1;
    CHECK(map != MAP_FAILED && host >= 0, "create, map and open %s on both mounts: %s", name, strerror(errno));
    if (map != MAP_FAILED && host >= 0) {
        map[16384] = 'g';
        memset(page, 'H', 4000);
        CHECK(write(host, page, 4000) == 4000 && pwrite(fd, "G", 1, 24000) == 1,
              "%s: an append on the Mac and a write past it: %s", name, strerror(errno));
        CHECK(map[16384] == 'g' && map[20000] == 'H' && map[23999] == 'H' && map[24000] == 'G',
              "%s: after a write past bytes the Mac appended, the mapping reads %d %d %d %d", name, map[16384],
              map[20000], map[23999], map[24000]);
        CHECK(msync((void *)map, 32768, MS_SYNC) == 0 && on_host(name, 16384, 'g') && on_host(name, 20000, 'H') &&
              on_host(name, 23999, 'H') && on_host(name, 24000, 'G'),
              "%s: the bytes the Mac appended did not outlive the push of the guest's page", name);
    }
    if (map != MAP_FAILED)
        munmap((void *)map, 32768);
    close(host);
    close(fd);
}

static void
host_append(const char *name)
{
    char back[8] = "";
    int fd = open(name, O_WRONLY | O_CREAT | O_EXCL | O_APPEND, 0644);
    bool made = fd >= 0 && write(fd, "abc", 3) == 3;
    int host = made ? peer_open(name, O_WRONLY | O_APPEND) : -1;
    CHECK(host >= 0 && write(host, "HH", 2) == 2 && write(fd, "g", 1) == 1,
          "%s: an append on the Mac and one in the guest: %s", name, strerror(errno));
    close(host);
    close(fd);
    host = peer_open(name, O_RDONLY);
    ssize_t got = pread(host, back, sizeof(back) - 1, 0);
    CHECK(got == 6 && !memcmp(back, "abcHHg", 6), "%s: after an append on the Mac and one in the guest the Mac holds %s",
          name, back);
    close(host);
}

static void *
sync_write(void *arg)
{
    struct seer *s = arg;
    while (!atomic_load(&s->stop)) {
        if (pwrite(s->fd, "w", 1, 8000) != 1)
            atomic_store(&s->failed, 1);
        s->rounds++;
    }
    return NULL;
}

static void
sync_race(const char *name)
{
    static char page[16384];
    int fd = open(name, O_RDWR | O_CREAT | O_EXCL, 0644);
    memset(page, 'x', sizeof(page));
    bool made = fd >= 0 && write(fd, page, sizeof(page)) == (ssize_t)sizeof(page);
    volatile char *map = made ? mmap(NULL, 16384, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0) : MAP_FAILED;
    struct seer s = { .fd = fd };
    pthread_t t;
    bool ready = map != MAP_FAILED && pthread_create(&t, NULL, sync_write, &s) == 0;
    CHECK(ready, "create and map %s: %s", name, strerror(errno));
    int rounds = 10000, late = 0, failed = 0;
    for (int i = 1; ready && i <= rounds; i++) {
        char want = (char)('A' + i % 26);
        map[100] = want;
        failed += msync((void *)map, 16384, MS_SYNC) != 0;
        late += !on_host(name, 100, want);
    }
    atomic_store(&s.stop, 1);
    if (ready)
        pthread_join(t, NULL);
    if (map != MAP_FAILED)
        munmap((void *)map, 16384);
    close(fd);
    CHECK(late == 0 && failed == 0 && s.rounds > 0 && !atomic_load(&s.failed),
          "%s: beside %d writes to the same page, msync returned %d times of %d before its store was on the Mac, and "
          "%d calls failed", name, s.rounds, late, rounds, failed + atomic_load(&s.failed));
}

static void *
range_reader(void *arg)
{
    static char buf[32768];
    struct seer *s = arg;
    while (!atomic_load(&s->stop)) {
        if (pread(s->fd, buf, sizeof(buf), 0) != (ssize_t)sizeof(buf))
            atomic_store(&s->failed, 1);
        s->rounds++;
    }
    return NULL;
}

static void
read_race(const char *name)
{
    static char page[49152];
    int fd = open(name, O_RDWR | O_CREAT | O_EXCL, 0644);
    memset(page, 'x', sizeof(page));
    bool made = fd >= 0 && write(fd, page, sizeof(page)) == (ssize_t)sizeof(page);
    volatile char *map = made ? mmap(NULL, 49152, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0) : MAP_FAILED;
    struct seer s = { .fd = fd, .counter = (volatile uint32_t *)(map + 20480) };
    pthread_t t, u;
    bool ready = map != MAP_FAILED && pthread_create(&t, NULL, range_reader, &s) == 0;
    CHECK(ready, "create and map %s: %s", name, strerror(errno));
    bool storing = ready && pthread_create(&u, NULL, seen_store, &s) == 0;
    int rounds = 20000, late = 0, failed = !storing;
    for (int i = 1; ready && i <= rounds; i++) {
        char want = (char)('A' + i % 26);
        map[16484] = '-';
        failed += msync((void *)(map + 16384), 16384, MS_SYNC) != 0;
        uint64_t again = clock_gettime_nsec_np(CLOCK_UPTIME_RAW) + arc4random_uniform(150000);
        while (clock_gettime_nsec_np(CLOCK_UPTIME_RAW) < again)
            ;
        map[16484] = want;
        failed += msync((void *)(map + 16384), 16384, MS_SYNC) != 0;
        late += !on_host(name, 16484, want);
    }
    atomic_store(&s.stop, 1);
    if (ready)
        pthread_join(t, NULL);
    if (storing)
        pthread_join(u, NULL);
    if (map != MAP_FAILED)
        munmap((void *)map, 49152);
    close(fd);
    CHECK(late == 0 && failed == 0 && s.rounds > 0 && !atomic_load(&s.failed),
          "%s: beside %d reads of the page before it and its own and a tight storer, msync returned %d times of %d "
          "before its store was on the Mac, and %d calls failed", name, s.rounds, late, rounds,
          failed + atomic_load(&s.failed));
}

static void *
self_reader(void *arg)
{
    struct seer *s = arg;
    char *map = (char *)(uintptr_t)s->counter;
    for (int i = 0; i < s->rounds; i++) {
        if (msync(map, 16384, MS_INVALIDATE) != 0 || pread(s->fd, map, 16384, 16384) != 16384)
            atomic_store(&s->failed, 1);
    }
    atomic_store(&s->stop, 1);
    return NULL;
}

static void
self_read(const char *name)
{
    static char page[32768];
    int fd = open(name, O_RDWR | O_CREAT | O_EXCL, 0644);
    memset(page, 'x', sizeof(page));
    bool made = fd >= 0 && write(fd, page, sizeof(page)) == (ssize_t)sizeof(page);
    volatile char *map = made ? mmap(NULL, 32768, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0) : MAP_FAILED;
    struct seer s = { .fd = fd, .rounds = 2000, .counter = (volatile uint32_t *)map };
    pthread_t t;
    bool ready = map != MAP_FAILED && pthread_create(&t, NULL, self_reader, &s) == 0;
    CHECK(ready, "create and map %s: %s", name, strerror(errno));
    int writes = 0, failed = 0;
    while (ready && !atomic_load(&s.stop)) {
        failed += pwrite(fd, page, sizeof(page), 0) != (ssize_t)sizeof(page);
        writes++;
    }
    if (ready)
        pthread_join(t, NULL);
    if (map != MAP_FAILED)
        munmap((void *)map, 32768);
    close(fd);
    CHECK(writes > 0 && failed == 0 && !atomic_load(&s.failed),
          "%s: %d reads into a mapping of the file beside %d writes over both pages, %d calls failed", name, s.rounds,
          writes, failed + atomic_load(&s.failed));
}

static void
wired_write(const char *name)
{
    static char page[32768];
    int fd = open(name, O_RDWR | O_CREAT | O_EXCL, 0644);
    memset(page, 'x', sizeof(page));
    bool made = fd >= 0 && write(fd, page, sizeof(page)) == (ssize_t)sizeof(page);
    volatile char *map = made ? mmap(NULL, 16384, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0) : MAP_FAILED;
    CHECK(map != MAP_FAILED && mlock((void *)map, 16384) == 0, "create, map and wire %s: %s", name, strerror(errno));
    if (map == MAP_FAILED) {
        close(fd);
        return;
    }
    map[100] = 'm';
    CHECK(pwrite(fd, "W", 1, 200) == 1 && map[200] == 'W' && map[100] == 'm',
          "%s: after a write into a wired page the mapping reads %d %d", name, map[100], map[200]);
    map[300] = 'n';
    CHECK(pwrite(fd, "V", 1, 400) == 1 && munlock((void *)map, 16384) == 0 && msync((void *)map, 16384, MS_SYNC) == 0,
          "%s: write, unwire and msync: %s", name, strerror(errno));
    CHECK(on_host(name, 100, 'm') && on_host(name, 200, 'W') && on_host(name, 300, 'n') && on_host(name, 400, 'V'),
          "%s: the stores to a wired page and the writes into it are not all on the Mac", name);
    munmap((void *)map, 16384);
    close(fd);
}

static void
wired_past(const char *name)
{
    static char data[65536];
    char back = 0;
    int fd = open(name, O_RDWR | O_CREAT | O_EXCL, 0644);
    memset(data, 'A', sizeof(data));
    bool made = fd >= 0 && write(fd, data, sizeof(data)) == (ssize_t)sizeof(data);
    volatile char *map = made ? mmap(NULL, 65536, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0) : MAP_FAILED;
    CHECK(map != MAP_FAILED && mlock((void *)map, 65536) == 0, "create, map and wire %s: %s", name, strerror(errno));
    if (map == MAP_FAILED) {
        close(fd);
        return;
    }
    map[40000] = 'o';
    memset(data, 'B', sizeof(data));
    CHECK(ftruncate(fd, 4000) == 0 && pwrite(fd, data, 61536, 4000) == 61536, "%s: truncate and write again: %s", name,
          strerror(errno));
    CHECK(pread(fd, &back, 1, 40000) == 1 && back == 'B' && map[40000] == 'B' && map[50000] == 'B',
          "%s: after a write over wired pages a truncation left behind, read(2) gives %d and the mapping %d and %d", name,
          back, map[40000], map[50000]);
    CHECK(munlock((void *)map, 65536) == 0 && msync((void *)map, 65536, MS_SYNC) == 0 && on_host(name, 40000, 'B') &&
          on_host(name, 50000, 'B') && on_host(name, 3999, 'A'),
          "%s: the write over wired pages a truncation left behind is not what the Mac holds", name);
    munmap((void *)map, 65536);
    close(fd);
}

static void *
turn_write(void *arg)
{
    static char block[4096];
    struct seer *s = arg;
    while (!atomic_load(&s->stop)) {
        if (pwrite(s->fd, block, sizeof(block), 0) != (ssize_t)sizeof(block))
            atomic_store(&s->failed, 1);
        s->rounds++;
    }
    return NULL;
}

static void
turns(const char *name)
{
    struct stat st;
    char byte;
    int fd = open(name, O_RDWR | O_CREAT | O_EXCL, 0644);
    struct seer s = { .fd = fd };
    pthread_t t;
    bool ready = fd >= 0 && ftruncate(fd, 16384) == 0 && pthread_create(&t, NULL, turn_write, &s) == 0;
    CHECK(ready, "create %s: %s", name, strerror(errno));
    long calls[2] = { 0, 0 };
    uint64_t worst[2] = { 0, 0 };
    int failed = 0;
    for (int kind = 0; ready && kind < 2; kind++) {
        uint64_t end = clock_gettime_nsec_np(CLOCK_UPTIME_RAW) + 1000000000, at;
        while ((at = clock_gettime_nsec_np(CLOCK_UPTIME_RAW)) < end) {
            failed += kind ? pread(fd, &byte, 1, 8192) != 1 : fstat(fd, &st) != 0;
            uint64_t took = clock_gettime_nsec_np(CLOCK_UPTIME_RAW) - at;
            worst[kind] = took > worst[kind] ? took : worst[kind];
            calls[kind]++;
        }
    }
    atomic_store(&s.stop, 1);
    if (ready)
        pthread_join(t, NULL);
    close(fd);
    printf("%s: beside %d writes in two seconds, %ld stats (the slowest %llu us) and %ld reads (the slowest %llu us)\n",
           name, s.rounds, calls[0], worst[0] / 1000, calls[1], worst[1] / 1000);
    CHECK(failed == 0 && !atomic_load(&s.failed) && s.rounds > 0 && calls[0] > 0 && calls[1] * 4 > s.rounds,
          "%s: %d calls failed beside a writer, or the readers got less than a quarter of its turns", name,
          failed + atomic_load(&s.failed));
}

static int
link_pair(const char *dir, const char *step)
{
    struct stat st;
    if (chdir(dir) != 0) {
        printf("chdir %s: %s\n", dir, strerror(errno));
        return 1;
    }
    if (!strcmp(step, "make")) {
        int fd = open("linked", O_WRONLY | O_CREAT | O_EXCL, 0644);
        int rc = fd < 0 || close(fd) != 0 || link("linked", "linked2") != 0;
        outcome("make two links", rc ? -1 : 0);
        return rc;
    }
    int rc = unlink("linked") != 0 || unlink("linked2") != 0 || stat("linked2", &st) == 0;
    for (int i = 0; i < 64 && !rc; i++) {
        char name[32];
        snprintf(name, sizeof(name), "flush%d", i);
        int fd = open(name, O_WRONLY | O_CREAT | O_EXCL, 0644);
        rc = fd < 0 || close(fd) != 0 || unlink(name) != 0;
    }
    outcome("remove both links", rc ? -1 : 0);
    return rc;
}

static int
raw_create(const char *path)
{
    int fd = open_dprotected_np(path, O_RDONLY | O_CREAT | O_EXCL, -1, O_DP_GETRAWUNENCRYPTED, 0644);
    if (fd >= 0) {
        close(fd);
        printf("a raw open was allowed\n");
        return 1;
    }
    printf("raw create: %s\n", strerror(errno));
    return errno != EPERM;
}

static int
case_probe(const char *dir)
{
    if (mkdir(dir, 0755) != 0 || chdir(dir) != 0) {
        printf("mkdir %s: %s\n", dir, strerror(errno));
        return 1;
    }
    long removed = -2;
    int fd = open("x", O_RDWR | O_CREAT | O_EXCL, 0644);
    if (fd >= 0 && unlink("x") == 0)
        removed = fpathconf(fd, _PC_CASE_SENSITIVE);
    if (fd >= 0)
        close(fd);
    fd = open("bar", O_WRONLY | O_CREAT | O_EXCL, 0644);
    int rc = fd < 0 || close(fd) != 0 || rename("bar", "BAR") != 0;
    bool upper = false, lower = false;
    DIR *d = opendir(".");
    for (struct dirent *e; d && (e = readdir(d));) {
        upper |= !strcmp(e->d_name, "BAR");
        lower |= !strcmp(e->d_name, "bar");
    }
    if (d)
        closedir(d);
    printf("case-sensitive answered for a removed file: %ld, after the rename: %ld, BAR %s, bar %s\n", removed,
           pathconf("BAR", _PC_CASE_SENSITIVE), upper ? "listed" : "missing", lower ? "listed" : "missing");
    chdir("/");
    return rc || !upper || lower;
}

static int
rw_edge(const char *dir, const char *other)
{
    char page[16384], back[16];
    struct stat st;
    int fd;

    peer = other;
    CHECK(mkdir(dir, 0755) == 0 && chdir(dir) == 0, "mkdir %s: %s", dir, strerror(errno));
    fd = open("view", O_WRONLY | O_CREAT | O_EXCL, 0644);
    CHECK(fd >= 0 && pwrite(fd, "1", 1, 0) == 1 && on_host("view", 0, '1') && pwrite(fd, "2", 1, 0) == 1 &&
          on_host("view", 0, '2'), "the second mount does not show at once what the first one wrote: %s", strerror(errno));
    close(fd);
    fd = open("readonly-made", O_WRONLY | O_CREAT | O_EXCL, 0444);
    CHECK(fd >= 0 && write(fd, "abc", 3) == 3 && close(fd) == 0, "a file created read-only is written by its creator: %s",
          strerror(errno));
    fd = open("readonly-open", O_RDONLY | O_CREAT | O_EXCL, 0644);
    CHECK(fd >= 0 && fstat(fd, &st) == 0 && st.st_size == 0, "create for reading: %s", strerror(errno));
    CHECK(fsync(fd) == 0, "fsync of a descriptor open for reading: %s", strerror(errno));
    close(fd);
    int dfd = open(".", O_RDONLY);
    CHECK(dfd >= 0 && fsync(dfd) == 0, "fsync of a directory: %s", strerror(errno));
    close(dfd);

    for (int grow = 0; grow < 2; grow++) {
        const char *name = grow ? "tail-write" : "tail-truncate";
        fd = open(name, O_RDWR | O_CREAT | O_EXCL, 0644);
        memset(page, 'x', 4000);
        CHECK(fd >= 0 && write(fd, page, 4000) == 4000, "create %s: %s", name, strerror(errno));
        char *map = mmap(NULL, sizeof(page), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        CHECK(map != MAP_FAILED, "mmap %s: %s", name, strerror(errno));
        if (map == MAP_FAILED)
            continue;
        map[0] = 'A';
        map[5000] = 'Z';
        if (grow)
            CHECK(pwrite(fd, "END", 3, 20000) == 3, "write past the page: %s", strerror(errno));
        else
            CHECK(ftruncate(fd, 10000) == 0, "ftruncate to grow: %s", strerror(errno));
        CHECK(msync(map, sizeof(page), MS_SYNC) == 0 && munmap(map, sizeof(page)) == 0, "msync: %s", strerror(errno));
        memset(back, 1, sizeof(back));
        CHECK(pread(fd, back, 1, 5000) == 1 && back[0] == 0, "%s: a store past the end reached the file", name);
        CHECK(pread(fd, back, 1, 0) == 1 && back[0] == 'A', "%s: a store inside the file was lost", name);
        close(fd);
    }

    store_race("race-truncate", -1);
    store_race("race-write", 20000);
    store_race("race-append", 4000);
    store_race("race-inside", 2000);
    seen_race("race-seen", false);
    seen_race("race-tail", true);
    mapped_race("race-mapped");
    sync_race("race-sync");
    read_race("race-read");
    self_read("race-self");
    gap_write("gap-write");
    host_append("host-append");
    wired_write("wired");
    wired_past("wired-past");
    turns("turns");

    fd = open("largest", O_WRONLY | O_CREAT | O_EXCL, 0644);
    errno = 0;
    CHECK(pwrite(fd, "x", 1, INT64_MAX - 1000) < 0 && errno == EFBIG, "a write a thousand bytes below the largest offset: %s",
          strerror(errno));
    errno = 0;
    CHECK(ftruncate(fd, INT64_MAX - 1000) != 0 && errno == EFBIG,
          "a truncation to a thousand bytes below the largest offset: %s", strerror(errno));
    CHECK(fstat(fd, &st) == 0 && st.st_size == 0, "a refused write and truncation left %lld bytes", (long long)st.st_size);
    close(fd);

    fd = open("times", O_WRONLY | O_CREAT | O_EXCL, 0644);
    close(fd);
    char value[8];
    errno = 0;
    CHECK(getxattr("times", XATTR_RESOURCEFORK_NAME, value, sizeof(value), 1, 0) < 0 && errno == EINVAL,
          "getxattr of a resource fork at a position: %s", strerror(errno));
    errno = 0;
    CHECK(setxattr("times", XATTR_RESOURCEFORK_NAME, "x", 1, 1, 0) != 0 && errno == EINVAL,
          "setxattr of a resource fork at a position: %s", strerror(errno));
    struct timeval old[2] = { { 981173106, 0 }, { 981173106, 0 } };
    CHECK(utimes("times", old) == 0 && utimes("times", NULL) == 0 && stat("times", &st) == 0 &&
          st.st_mtimespec.tv_sec > 981173106 + 86400, "utimes(NULL) sets the current time: mtime %ld",
          (long)st.st_mtimespec.tv_sec);
    errno = 0;
    CHECK(chflags("times", UF_HIDDEN) != 0 && errno == ENOTSUP, "chflags with a flag: %s", strerror(errno));
    errno = 0;
    CHECK(mknod("chr", S_IFCHR | 0600, 0) != 0, "mknod of a character device succeeded");
    chdir("/");
    printf("rw-edge: %d failures\n", failures);
    return failures ? 1 : 0;
}

static int
map_hold(const char *path, int seconds)
{
    int fd = open(path, O_RDWR);
    char *map = fd < 0 ? MAP_FAILED : mmap(NULL, 16384, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    struct stat st;
    if (map == MAP_FAILED) {
        printf("mmap: %s\n", strerror(errno));
        return 1;
    }
    map[0] = 'A';
    sleep((unsigned)seconds);
    int rc = fstat(fd, &st);
    printf("size seen after the host appended: %lld\n", rc == 0 ? (long long)st.st_size : -1LL);
    rc |= msync(map, 16384, MS_SYNC) | munmap(map, 16384);
    close(fd);
    return rc != 0;
}

static int
map_reader(const char *path, int seconds)
{
    int rfd = open(path, O_RDONLY), wfd = open(path, O_RDWR);
    char *map = wfd < 0 ? MAP_FAILED : mmap(NULL, 16384, PROT_READ | PROT_WRITE, MAP_SHARED, wfd, 0);
    if (rfd < 0 || map == MAP_FAILED) {
        printf("open or mmap: %s\n", strerror(errno));
        return 1;
    }
    map[0] = 'W';
    close(wfd);
    int rc = munmap(map, 16384);
    printf("unmapped: %s\n", rc ? strerror(errno) : "ok");
    fflush(stdout);
    sleep((unsigned)seconds);
    close(rfd);
    return rc != 0;
}

static int
map_store(const char *path, int seconds)
{
    int fd = open(path, O_RDWR);
    char *map = fd < 0 ? MAP_FAILED : mmap(NULL, 16384, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        printf("mmap: %s\n", strerror(errno));
        return 1;
    }
    close(fd);
    sleep((unsigned)seconds);
    map[0] = 'M';
    int rc = msync(map, 16384, MS_SYNC);
    printf("msync after the descriptor closed: %s\n", rc ? strerror(errno) : "ok");
    rc |= munmap(map, 16384);
    return rc != 0;
}

static int
map_sync(const char *path, int seconds)
{
    int fd = open(path, O_RDWR);
    char *map = fd < 0 ? MAP_FAILED : mmap(NULL, 16384, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        printf("mmap: %s\n", strerror(errno));
        return 1;
    }
    memcpy(map, "synced", 6);
    sync();
    sleep((unsigned)seconds);
    int rc = munmap(map, 16384);
    close(fd);
    return rc != 0;
}

static int
hold_mode(const char *path, const char *how, int seconds)
{
    int flags = !strcmp(how, "evtonly") ? O_RDONLY | O_EVTONLY : !strcmp(how, "wronly") ? O_WRONLY :
                !strcmp(how, "rdonly-trunc") ? O_RDONLY | O_TRUNC :
                !strcmp(how, "rdonly-creat") ? O_RDONLY | O_CREAT | O_EXCL : O_RDONLY;
    char buf[64];
    int fd = open(path, flags, 0644);
    if (fd < 0) {
        printf("open: %s\n", strerror(errno));
        return 1;
    }
    sleep((unsigned)seconds);
    ssize_t n = (flags & O_ACCMODE) == O_WRONLY ? 0 : read(fd, buf, sizeof(buf));
    if (n < 0)
        printf("read: %s\n", strerror(errno));
    else
        printf("%zd bytes\n", n);
    close(fd);
    return n < 0;
}

static int
hold_write(const char *path, int seconds)
{
    int fd = open(path, O_WRONLY | O_APPEND);
    if (fd < 0) {
        printf("open: %s\n", strerror(errno));
        return 1;
    }
    sleep((unsigned)seconds);
    ssize_t n = write(fd, "held\n", 5);
    if (n < 0)
        printf("write: %s\n", strerror(errno));
    else
        printf("%zd bytes\n", n);
    int rc = close(fd);
    return n != 5 || rc != 0;
}

struct rw_worker {
    const char *dir;
    int id;
    int shared;
    double until;
    long files;
    int bad;
};

static void *
rw_work(void *arg)
{
    struct rw_worker *w = arg;
    char sub[1024], path[1200], moved[1200];
    unsigned seed = (unsigned)w->id * 7919u + 1;
    unsigned char *buf = malloc(300000), *back = malloc(300000);
    if (w->shared)
        snprintf(sub, sizeof(sub), "%s/shared", w->dir);
    else
        snprintf(sub, sizeof(sub), "%s/t%d", w->dir, w->id);
    if (!w->shared && mkdir(sub, 0755) != 0)
        w->bad++;
    for (long k = 0; now() < w->until; k++) {
        seed = seed * 1103515245u + 12345u;
        size_t size = seed % 300000, step = 1 + (seed >> 8) % 70000;
        snprintf(path, sizeof(path), "%s/f%d-%ld", sub, w->id, k);
        snprintf(moved, sizeof(moved), "%s/g%d-%ld", sub, w->id, k);
        fill(seed, 0, buf, size);
        int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
        for (size_t done = 0; fd >= 0 && done < size; done += step)
            if (write(fd, buf + done, size - done < step ? size - done : step) < 0)
                w->bad++;
        if (fd < 0 || close(fd) != 0)
            w->bad++;
        if (rename(path, moved) != 0)
            w->bad++;
        fd = open(moved, O_RDONLY);
        ssize_t n = fd < 0 ? -1 : read(fd, back, 300000);
        if (n != (ssize_t)size || memcmp(buf, back, size) != 0)
            w->bad++;
        if (fd >= 0)
            close(fd);
        if (k % 2 && unlink(moved) != 0)
            w->bad++;
        w->files++;
    }
    free(buf);
    free(back);
    return NULL;
}

struct appender {
    int fd, id, records;
    const char *path;
};

static void *
region_work(void *arg)
{
    struct appender *a = arg;
    unsigned char *buf = malloc(65536);
    void *bad = NULL;
    for (int r = 0; r < a->records && !bad; r++) {
        fill((unsigned)(a->id * 1000 + r), 0, buf, 65536);
        if (pwrite(a->fd, buf, 65536, (off_t)a->id * 65536) != 65536)
            bad = (void *)1;
    }
    free(buf);
    return bad;
}

static void *
append_work(void *arg)
{
    struct appender *a = arg;
    char rec[64];
    int fd = open(a->path, O_WRONLY | O_APPEND);
    void *bad = fd < 0 ? (void *)1 : NULL;
    for (int i = 0; i < a->records && !bad; i++) {
        snprintf(rec, sizeof(rec), "%04d %08d %-48s\n", a->id, i, "record");
        if (write(fd, rec, 63) != 63)
            bad = (void *)1;
    }
    if (fd >= 0)
        close(fd);
    return bad;
}

static int
rw_stress(const char *dir, int threads, int seconds)
{
    pthread_t t[64];
    struct rw_worker w[64];
    struct appender a[64];
    char log[1024];
    long files = 0;
    int bad = 0;

    if (threads < 1 || threads > 64 || mkdir(dir, 0755) != 0)
        return 2;
    double until = now() + seconds;
    for (int i = 0; i < threads; i++) {
        w[i] = (struct rw_worker){ .dir = dir, .id = i, .until = until };
        pthread_create(&t[i], NULL, rw_work, &w[i]);
    }
    for (int i = 0; i < threads; i++) {
        pthread_join(t[i], NULL);
        files += w[i].files;
        bad += w[i].bad;
    }
    snprintf(log, sizeof(log), "%s/shared", dir);
    if (mkdir(log, 0755) != 0)
        return 1;
    until = now() + seconds;
    for (int i = 0; i < threads; i++) {
        w[i] = (struct rw_worker){ .dir = dir, .id = i, .shared = 1, .until = until };
        pthread_create(&t[i], NULL, rw_work, &w[i]);
    }
    for (int i = 0; i < threads; i++) {
        pthread_join(t[i], NULL);
        files += w[i].files;
        bad += w[i].bad;
    }
    snprintf(log, sizeof(log), "%s/regions", dir);
    int rfd = open(log, O_RDWR | O_CREAT | O_EXCL, 0644);
    if (rfd < 0)
        return 1;
    for (int i = 0; i < threads; i++) {
        a[i] = (struct appender){ .fd = rfd, .id = i, .records = 50 };
        pthread_create(&t[i], NULL, region_work, &a[i]);
    }
    for (int i = 0; i < threads; i++) {
        void *r;
        pthread_join(t[i], &r);
        bad += r != NULL;
    }
    unsigned char *expect = malloc(65536), *got = malloc(65536);
    for (int i = 0; i < threads; i++) {
        fill((unsigned)(i * 1000 + 49), 0, expect, 65536);
        if (pread(rfd, got, 65536, (off_t)i * 65536) != 65536 || memcmp(expect, got, 65536))
            bad++;
    }
    free(expect);
    free(got);
    close(rfd);
    snprintf(log, sizeof(log), "%s/log", dir);
    int fd = open(log, O_WRONLY | O_CREAT | O_EXCL | O_APPEND, 0644);
    if (fd < 0)
        return 1;
    for (int i = 0; i < threads; i++) {
        a[i] = (struct appender){ .fd = fd, .id = i, .records = 2000, .path = log };
        pthread_create(&t[i], NULL, append_work, &a[i]);
    }
    for (int i = 0; i < threads; i++) {
        void *r;
        pthread_join(t[i], &r);
        bad += r != NULL;
    }
    close(fd);
    struct stat st;
    long want = (long)threads * 2000 * 63;
    FILE *f = fopen(log, "r");
    int *next = calloc((size_t)threads, sizeof(int));
    char line[128];
    long lines = 0;
    while (f && fgets(line, sizeof(line), f)) {
        int id, seq;
        if (sscanf(line, "%d %d", &id, &seq) != 2 || id < 0 || id >= threads || seq != next[id]++ ||
            strlen(line) != 63)
            bad++;
        lines++;
    }
    if (f)
        fclose(f);
    free(next);
    CHECK(stat(log, &st) == 0 && st.st_size == want, "appended log is %lld bytes, want %ld", (long long)st.st_size,
          want);
    CHECK(bad == 0, "%d bad results", bad);
    printf("rw-stress: %d threads wrote and checked %ld files in %d s each in their own and in one shared directory, "
           "wrote regions of one file and appended %ld records to another, %d failures\n", threads, files, seconds,
           lines, failures);
    return failures ? 1 : 0;
}

static int
bench_write(const char *path, int mib)
{
    static const size_t steps[] = { 1 << 20, 128 << 10, 16 << 10, 4 << 10 };
    unsigned char *buf = malloc(1 << 20);
    fill(50, 0, buf, 1 << 20);
    printf("write %s\n", path);
    for (size_t s = 0; s < sizeof(steps) / sizeof(*steps); s++) {
        size_t step = steps[s];
        off_t total = (off_t)(step >= (128 << 10) ? mib : mib / 4) << 20;
        int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0)
            return 1;
        double t0 = now(), c0 = cpu();
        for (off_t done = 0; done < total; done += (off_t)step)
            if (write(fd, buf, step) != (ssize_t)step)
                return 1;
        double dt = now() - t0, dc = cpu() - c0;
        double t1 = now();
        fsync(fd);
        double synced = now() - t1;
        close(fd);
        printf("  %zu KiB writes: %.0f MB/s, %.1f us per write, %.2f CPUs busy, then fsync %.1f ms\n", step >> 10,
               total / dt / 1e6, dt / (total / (double)step) * 1e6, dc / dt, synced * 1e3);
    }
    unlink(path);
    free(buf);
    return 0;
}

static int
bench_files(const char *dir, int count)
{
    char path[1024], other[1024];
    unsigned char data[4096];
    fill(60, 0, data, sizeof(data));
    if (mkdir(dir, 0755) != 0)
        return 1;
    double t0 = now();
    for (int i = 0; i < count; i++) {
        snprintf(path, sizeof(path), "%s/f%d", dir, i);
        int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
        if (fd < 0 || write(fd, data, sizeof(data)) != (ssize_t)sizeof(data) || close(fd) != 0)
            return 1;
    }
    double created = (now() - t0) / count * 1e6;
    t0 = now();
    for (int i = 0; i < count; i++) {
        snprintf(path, sizeof(path), "%s/f%d", dir, i);
        int fd = open(path, O_WRONLY | O_APPEND);
        if (fd < 0 || write(fd, data, 16) != 16 || close(fd) != 0)
            return 1;
    }
    double appended = (now() - t0) / count * 1e6;
    t0 = now();
    for (int i = 0; i < count; i++) {
        snprintf(path, sizeof(path), "%s/f%d", dir, i);
        snprintf(other, sizeof(other), "%s/g%d", dir, i);
        if (rename(path, other) != 0)
            return 1;
    }
    double renamed = (now() - t0) / count * 1e6;
    t0 = now();
    for (int i = 0; i < count; i++) {
        snprintf(other, sizeof(other), "%s/g%d", dir, i);
        if (unlink(other) != 0)
            return 1;
    }
    double removed = (now() - t0) / count * 1e6;
    t0 = now();
    for (int i = 0; i < count / 10; i++) {
        snprintf(path, sizeof(path), "%s/d%d", dir, i);
        if (mkdir(path, 0755) != 0 || rmdir(path) != 0)
            return 1;
    }
    double dirs = (now() - t0) / (count / 10) * 1e6;
    rmdir(dir);
    printf("%s: %d files, us each: create+write 4 KiB+close %.1f, open+append+close %.1f, rename %.1f, "
           "unlink %.1f, mkdir+rmdir %.1f\n", dir, count, created, appended, renamed, removed, dirs);
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
    if (argc == 4 && !strcmp(argv[1], "bench-cold"))
        return bench_cold(argv[2], atoi(argv[3]));
    if (argc == 4 && !strcmp(argv[1], "bench-random"))
        return bench_random(argv[2], atoi(argv[3]));
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
    if (argc == 4 && !strcmp(argv[1], "read-close"))
        return read_close(argv[2], atoi(argv[3]));
    if (argc == 4 && !strcmp(argv[1], "reopen-close"))
        return reopen_close(argv[2], atoi(argv[3]));
    if (argc == 4 && !strcmp(argv[1], "hold"))
        return hold(argv[2], atoi(argv[3]));
    if (argc == 3 && !strcmp(argv[1], "rdadvise"))
        return rdadvise(argv[2]);
    if (argc == 3 && !strcmp(argv[1], "classic-readdir"))
        return classic_readdir(argv[2]);
    if (argc == 6 && !strcmp(argv[1], "mount-as"))
        return mount_as((uid_t)atoi(argv[2]), (gid_t)atoi(argv[3]), argv[4], argv[5]);
    if (argc >= 4 && !strcmp(argv[1], "lookups"))
        return lookups(atoi(argv[2]), argv + 3);
    if (argc == 3 && !strcmp(argv[1], "ro-share"))
        return ro_share(argv[2]);
    if (argc == 4 && !strcmp(argv[1], "truncate")) {
        int rc = truncate(argv[2], atoll(argv[3]));
        outcome("truncate", rc);
        return rc != 0;
    }
    if (argc == 3 && !strcmp(argv[1], "write-tree"))
        return write_tree(argv[2]);
    if (argc == 3 && !strcmp(argv[1], "rw-errors"))
        return rw_errors(argv[2]);
    if (argc == 3 && !strcmp(argv[1], "xattrs"))
        return xattrs(argv[2]);
    if ((argc == 3 || argc == 4) && !strcmp(argv[1], "xattr-list"))
        return xattr_list(argv[2], argc == 4 && !strcmp(argv[3], "all"));
    if (argc == 3 && !strcmp(argv[1], "coherence"))
        return coherence(argv[2]);
    if (argc == 4 && !strcmp(argv[1], "rw-edge"))
        return rw_edge(argv[2], argv[3]);
    if (argc == 4 && !strcmp(argv[1], "map-hold"))
        return map_hold(argv[2], atoi(argv[3]));
    if (argc == 3 && !strcmp(argv[1], "exec-fail"))
        return exec_fail(argv[2]);
    if (argc == 5 && !strcmp(argv[1], "store-loop"))
        return store_loop(argv[2], argv[3], atoi(argv[4]));
    if (argc == 4 && !strcmp(argv[1], "map-fail"))
        return map_fail(argv[2], atoi(argv[3]));
    if (argc == 4 && !strcmp(argv[1], "short-write"))
        return short_write(argv[2], atoi(argv[3]));
    if (argc == 4 && !strcmp(argv[1], "map-unpushed"))
        return map_unpushed(argv[2], atoi(argv[3]));
    if (argc == 4 && !strcmp(argv[1], "chaos"))
        return chaos_run(argv[2], atoi(argv[3]));
    if (argc == 3 && !strcmp(argv[1], "locks"))
        return locks_run(argv[2]);
    if (argc == 5 && !strcmp(argv[1], "owned"))
        return owned_run(argv[2], argv[3], atoi(argv[4]));
    if (argc == 6 && !strcmp(argv[1], "model"))
        return model_run(argv[2], argv[3], atoi(argv[4]), (unsigned)atoi(argv[5]));
    if (argc == 5 && !strcmp(argv[1], "hold-readers"))
        return hold_readers(argv[2], argv[3], atoi(argv[4]));
    if (argc == 4 && !strcmp(argv[1], "advise-append"))
        return advise_append(argv[2], atoi(argv[3]));
    if (argc == 4 && !strcmp(argv[1], "advise-close"))
        return advise_close(argv[2], atoi(argv[3]));
    if (argc == 4 && !strcmp(argv[1], "link-pair"))
        return link_pair(argv[2], argv[3]);
    if (argc == 3 && !strcmp(argv[1], "raw-create"))
        return raw_create(argv[2]);
    if (argc == 3 && !strcmp(argv[1], "case-probe"))
        return case_probe(argv[2]);
    if (argc == 4 && !strcmp(argv[1], "map-reader"))
        return map_reader(argv[2], atoi(argv[3]));
    if (argc == 4 && !strcmp(argv[1], "map-store"))
        return map_store(argv[2], atoi(argv[3]));
    if (argc == 4 && !strcmp(argv[1], "map-sync"))
        return map_sync(argv[2], atoi(argv[3]));
    if (argc == 5 && !strcmp(argv[1], "hold-mode"))
        return hold_mode(argv[2], argv[3], atoi(argv[4]));
    if (argc == 4 && !strcmp(argv[1], "map-pressure"))
        return map_pressure(argv[2], atoi(argv[3]));
    if (argc == 3 && !strcmp(argv[1], "open-unlink"))
        return open_unlink(argv[2]);
    if (argc == 4 && !strcmp(argv[1], "hold-write"))
        return hold_write(argv[2], atoi(argv[3]));
    if (argc == 5 && !strcmp(argv[1], "rw-stress"))
        return rw_stress(argv[2], atoi(argv[3]), atoi(argv[4]));
    if (argc == 4 && !strcmp(argv[1], "bench-write"))
        return bench_write(argv[2], atoi(argv[3]));
    if (argc == 4 && !strcmp(argv[1], "bench-files"))
        return bench_files(argv[2], atoi(argv[3]));
    fprintf(stderr, "usage: t_virtiofs tree DIR | errors MNT SPEC | stress DIR THREADS ROUNDS | bench FILE |"
                    " bench-meta DIR ROUNDS\n");
    return 2;
}
