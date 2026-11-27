/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#define DISPATCH_ALIAS_V2(sym)

#include <dispatch/dispatch.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <libkern/OSAtomic.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <malloc/malloc.h>
#include <pthread.h>
#include <regex.h>
#include <signal.h>
#include <spawn.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/attr.h>
#include <sys/event.h>
#include <sys/fsgetpath.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/wait.h>
#include <unistd.h>
#include "check.h"

extern char **environ;

struct kevent_qos_s {
    uint64_t ident;
    int16_t filter;
    uint16_t flags;
    uint32_t qos;
    uint64_t udata;
    uint32_t fflags;
    uint32_t xflags;
    int64_t data;
    uint64_t ext[4];
};

extern int kevent_qos(int fd, const struct kevent_qos_s *changelist,
    int nchanges, struct kevent_qos_s *eventlist, int nevents,
    void *data_out, size_t *data_available, unsigned int flags);

static int
cmp_int(const void *a, const void *b)
{
    int va = *(const int *)a;
    int vb = *(const int *)b;

    return (va > vb) - (va < vb);
}

struct mem_cookie {
    char data[64];
    size_t len;
    size_t pos;
};

static int
mem_read(void *cookie, char *buf, int n)
{
    struct mem_cookie *m = cookie;
    size_t avail = m->len - m->pos;
    size_t take = (size_t)n < avail ? (size_t)n : avail;

    memcpy(buf, m->data + m->pos, take);
    m->pos += take;
    return (int)take;
}

static int
mem_write(void *cookie, const char *buf, int n)
{
    struct mem_cookie *m = cookie;
    size_t room = sizeof(m->data) - m->pos;
    size_t put = (size_t)n < room ? (size_t)n : room;

    memcpy(m->data + m->pos, buf, put);
    m->pos += put;
    if (m->pos > m->len)
        m->len = m->pos;
    return (int)put;
}

struct objtype_attr {
    uint32_t length;
    fsobj_type_t objtype;
};

struct dispatch_ctx {
    dispatch_queue_t parent;
    dispatch_queue_t child;
    int ran;
};

static void
dispatch_assert_cb(void *arg)
{
    struct dispatch_ctx *ctx = arg;

    dispatch_assert_queue(ctx->child);
    dispatch_assert_queue(ctx->parent);
    ctx->ran = 1;
}

static void
test_libc_and_kernel_exports(void)
{
    char pbuf[PATH_MAX], bname[PATH_MAX], sbuf[64];
    int arr[] = { 42, -7, 13, 0, 99, -7, 5 };
    const int want[] = { -7, -7, 0, 5, 13, 42, 99 };
    struct mem_cookie mc = { 0 };
    FILE *fp;
    regex_t re;
    regmatch_t pm[2];
    const char *hay = "xxfoo12345yy";
    char tmpl[] = "/tmp/kido-exp-file.XXXXXX";
    char dir[] = "/tmp/kido-exp-dir.XXXXXX";
    struct attrlist al = { .bitmapcount = ATTR_BIT_MAP_COUNT, .commonattr = ATTR_CMN_OBJTYPE };
    struct objtype_attr abuf = { 0 };
    struct stat st;
    struct statfs stfs;
    ssize_t glen;
    int fd, dirfd, pfd[2], kq, status;
    posix_spawn_file_actions_t fa;
    pid_t pid;
    char *argv[] = { "/bin/sh", "-c", "pwd", NULL };
    struct kevent_qos_s ch = { 0 }, ev = { 0 };
    volatile int64_t counter = 41;
    atomic_flag flag = ATOMIC_FLAG_INIT;
    mach_vm_address_t addr = 0;
    integer_t disp = 0, ref = 0;
    void *mem;

    CHECK(basename_r("/usr/bin/clang", pbuf) == pbuf && strcmp(pbuf, "clang") == 0);
    CHECK(basename_r("/usr/bin/", pbuf) == pbuf && strcmp(pbuf, "bin") == 0);
    CHECK(basename_r("/", pbuf) == pbuf && strcmp(pbuf, "/") == 0);
    CHECK(dirname_r("/usr/bin/clang", pbuf) == pbuf && strcmp(pbuf, "/usr/bin") == 0);
    CHECK(dirname_r("clang", pbuf) == pbuf && strcmp(pbuf, ".") == 0);
    CHECK(dirname_r("/", pbuf) == pbuf && strcmp(pbuf, "/") == 0);

    CHECK(strsignal_r(SIGTERM, sbuf, sizeof(sbuf)) == 0 && strstr(sbuf, "Terminated") != NULL);
    CHECK(strsignal_r(9999, sbuf, sizeof(sbuf)) == EINVAL);

    CHECK(heapsort(arr, sizeof(arr) / sizeof(arr[0]), sizeof(arr[0]), cmp_int) == 0);
    CHECK(memcmp(arr, want, sizeof(arr)) == 0);

    fp = funopen(&mc, mem_read, mem_write, NULL, NULL);
    CHECK(fp != NULL);
    if (fp != NULL) {
        memset(sbuf, 0, sizeof(sbuf));
        CHECK(fwrite("hello funopen", 1, 13, fp) == 13);
        CHECK(fflush(fp) == 0);
        mc.pos = 0;
        CHECK(fread(sbuf, 1, 13, fp) == 13 && memcmp(sbuf, "hello funopen", 13) == 0);
        CHECK(fclose(fp) == 0);
    }

    CHECK(regcomp(&re, "foo([0-9]+)", REG_EXTENDED) == 0);
    CHECK(regnexec(&re, hay, 8, 2, pm, 0) == 0 &&
        pm[0].rm_so == 2 && pm[0].rm_eo == 8 &&
        pm[1].rm_so == 5 && pm[1].rm_eo == 8);
    CHECK(regnexec(&re, hay, 5, 2, pm, 0) == REG_NOMATCH);
    regfree(&re);

    fd = mkstemp(tmpl);
    CHECK(fd >= 0);
    if (fd >= 0) {
        CHECK(fgetattrlist(fd, &al, &abuf, sizeof(abuf), 0) == 0 && abuf.objtype == VREG);
        CHECK(fstat(fd, &st) == 0 && fstatfs(fd, &stfs) == 0);
        CHECK(basename_r(tmpl, bname) == bname);
        errno = 0;
        CHECK(fsgetpath(pbuf, 0, &stfs.f_fsid, st.st_ino) == -1 && errno == EINVAL);
        errno = 0;
        glen = fsgetpath(pbuf, sizeof(pbuf), &stfs.f_fsid, st.st_ino);
        CHECK((glen > 0 && strstr(pbuf, bname) != NULL) || (glen == -1 && errno == ENOTSUP));
        close(fd);
        unlink(tmpl);
    }

    CHECK(mkdtemp(dir) != NULL);
    dirfd = open(dir, O_RDONLY);
    CHECK(dirfd >= 0 && pipe(pfd) == 0);
    if (dirfd >= 0) {
        CHECK(posix_spawn_file_actions_init(&fa) == 0);
        CHECK(posix_spawn_file_actions_addfchdir_np(&fa, dirfd) == 0);
        CHECK(posix_spawn_file_actions_addfchdir_np(&fa, -1) == EBADF);
        CHECK(posix_spawn_file_actions_adddup2(&fa, pfd[1], STDOUT_FILENO) == 0);
        CHECK(posix_spawn_file_actions_addclose(&fa, pfd[0]) == 0);
        CHECK(posix_spawn(&pid, "/bin/sh", &fa, NULL, argv, environ) == 0);
        close(pfd[1]);
        memset(pbuf, 0, sizeof(pbuf));
        CHECK(read(pfd[0], pbuf, sizeof(pbuf) - 1) > 0);
        CHECK(basename_r(dir, bname) == bname && strstr(pbuf, bname) != NULL);
        CHECK(waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);
        close(pfd[0]);
        posix_spawn_file_actions_destroy(&fa);
        close(dirfd);
    }
    rmdir(dir);

    kq = kqueue();
    CHECK(kq >= 0 && pipe(pfd) == 0);
    if (kq >= 0) {
        CHECK(write(pfd[1], "x", 1) == 1);
        ch.ident = (uint64_t)pfd[0];
        ch.filter = EVFILT_READ;
        ch.flags = EV_ADD | EV_ENABLE;
        CHECK(kevent_qos(kq, &ch, 1, &ev, 1, NULL, NULL, KEVENT_FLAG_IMMEDIATE) == 1 &&
            ev.ident == (uint64_t)pfd[0] && ev.filter == EVFILT_READ && ev.data == 1);
        close(pfd[0]);
        close(pfd[1]);
        close(kq);
    }

    CHECK(OSAtomicIncrement64(&counter) == 42 && counter == 42);
    CHECK(OSAtomicIncrement64(&counter) == 43 && counter == 43);

    CHECK((atomic_flag_test_and_set)(&flag) == 0);
    CHECK((atomic_flag_test_and_set)(&flag) != 0);
    (atomic_flag_clear)(&flag);
    CHECK((atomic_flag_test_and_set)(&flag) == 0);
    (atomic_flag_clear)(&flag);

    CHECK(mach_vm_allocate(mach_task_self(), &addr, vm_page_size, VM_FLAGS_ANYWHERE) == KERN_SUCCESS);
    *(volatile unsigned char *)(uintptr_t)addr = 0x5a;
    CHECK(mach_vm_page_query(mach_task_self(), addr, &disp, &ref) == KERN_SUCCESS);
    CHECK((disp & VM_PAGE_QUERY_PAGE_PRESENT) != 0);
    CHECK(mach_vm_deallocate(mach_task_self(), addr, vm_page_size) == KERN_SUCCESS);

    mem = malloc(64);
    CHECK(mem != NULL);
    if (mem != NULL) {
        memset(mem, 0x22, 64);
        CHECK(malloc_zone_check(malloc_default_zone()) != 0);
        free(mem);
    }
}

static void
test_aliases_and_libdyld(void)
{
    dispatch_queue_t parent = dispatch_queue_create("kido.exports.parent", DISPATCH_QUEUE_SERIAL);
    dispatch_queue_t child = dispatch_queue_create_with_target("kido.exports.child",
        DISPATCH_QUEUE_SERIAL, parent);
    struct dispatch_ctx ctx = { parent, child, 0 };
    char exe[PATH_MAX];
    uint32_t exelen = sizeof(exe);
    void *libdyld;

    CHECK(parent != NULL && child != NULL);
    if (parent != NULL && child != NULL) {
        dispatch_assert_queue_not(child);
        dispatch_assert_queue_not(parent);
        dispatch_sync_f(child, &ctx, dispatch_assert_cb);
        CHECK(ctx.ran == 1);
        dispatch_release(child);
        dispatch_release(parent);
    }
    CHECK(dlsym(RTLD_DEFAULT, "dispatch_assert_queue") != NULL);
    CHECK(dlsym(RTLD_DEFAULT, "dispatch_assert_queue_not") != NULL);
    CHECK(dlsym(RTLD_DEFAULT, "dispatch_queue_create_with_target") != NULL);
    CHECK(dlsym(RTLD_DEFAULT, "pthread_yield_np") != NULL);
    pthread_yield_np();

    CHECK(dlsym(RTLD_DEFAULT, "_NSGetExecutablePath") != NULL);
    CHECK(dlsym(RTLD_DEFAULT, "_dyld_image_count") != NULL);
    CHECK(dlsym(RTLD_DEFAULT, "dlopen") != NULL);
    CHECK(_NSGetExecutablePath(exe, &exelen) == 0 && exe[0] != '\0');
    CHECK(_dyld_image_count() > 0);

    CHECK(dlsym(RTLD_DEFAULT, "_ZN5dyld315internalInstallEv") == NULL);
    CHECK(dlsym(RTLD_DEFAULT, "_ZN5dyld316compatFuncLookupEPKcPPv") == NULL);
    CHECK(dlsym(RTLD_DEFAULT, "_ZN5dyld318entryVectorForDyldE") == NULL);
    CHECK(dlsym(RTLD_DEFAULT, "_ZNK8objc_opt10objc_opt_t6clsoptEv") == NULL);
    CHECK(dlsym(RTLD_DEFAULT, "_ZNK8objc_opt13objc_selopt_t3getEPKc") == NULL);
    CHECK(dlsym(RTLD_DEFAULT, "_Block_copy_collectable") == NULL);
    CHECK(dlsym(RTLD_DEFAULT, "ccdigest_final") == NULL);

    libdyld = dlopen("/usr/lib/system/libdyld.dylib", RTLD_LAZY | RTLD_NOLOAD);
    CHECK(libdyld != NULL);
    if (libdyld != NULL) {
        CHECK(dlsym(libdyld, "_dyld_image_count") != NULL);
        CHECK(dlsym(libdyld, "_ZTINSt3__117bad_function_callE") == NULL);
        CHECK(dlsym(libdyld, "_ZN5dyld315internalInstallEv") == NULL);
        dlclose(libdyld);
    }
}

int
main(void)
{
    test_libc_and_kernel_exports();
    test_aliases_and_libdyld();
    return checks_done("exports");
}
