/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dispatch/dispatch.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sched.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <unistd.h>
#if __has_feature(ptrauth_intrinsics)
#include <ptrauth.h>
#endif

#include "../libsystem/check.h"

#define NKEYS 5
#define SAMPLES 4
#define GA 4

#define PAC_OP(op, value, modifier) ({ \
        uint64_t v_ = (value); \
        __asm__ volatile(".arch_extension pauth\n" op " %0, %1" : "+r"(v_) : "r"((uint64_t)(modifier))); \
        v_; \
    })

static const char *const key_names[NKEYS] = { "IA", "IB", "DA", "DB", "GA" };

static int target(int x) { return x * 3; }
static int (*volatile target_fp)(int) = target;

static uint64_t
raw_value(int i)
{
    return 0x100000000ULL + 0x1000ULL * (uint64_t)i;
}

static uint64_t
sign(int key, uint64_t value, uint64_t modifier)
{
    uint64_t r;

    switch (key) {
    case 0: return PAC_OP("pacia", value, modifier);
    case 1: return PAC_OP("pacib", value, modifier);
    case 2: return PAC_OP("pacda", value, modifier);
    case 3: return PAC_OP("pacdb", value, modifier);
    default:
        __asm__ volatile(".arch_extension pauth\npacga %0, %1, %2" : "=r"(r) : "r"(value), "r"(modifier));
        return r;
    }
}

static uint64_t
auth(int key, uint64_t value, uint64_t modifier)
{
    switch (key) {
    case 0: return PAC_OP("autia", value, modifier);
    case 1: return PAC_OP("autib", value, modifier);
    case 2: return PAC_OP("autda", value, modifier);
    default: return PAC_OP("autdb", value, modifier);
    }
}

static uint64_t
strip_code(uint64_t value)
{
    __asm__ volatile(".arch_extension pauth\nxpaci %0" : "+r"(value));
    return value;
}

static void
signatures(uint64_t out[NKEYS][SAMPLES])
{
    for (int k = 0; k < NKEYS; k++) {
        for (int i = 0; i < SAMPLES; i++) {
            out[k][i] = sign(k, raw_value(i), 42 + (uint64_t)i);
        }
    }
}

static int
any_differs(const uint64_t a[SAMPLES], const uint64_t b[SAMPLES])
{
    for (int i = 0; i < SAMPLES; i++) {
        if (a[i] != b[i]) {
            return 1;
        }
    }
    return 0;
}

static int
ptrauth_enabled(void)
{
    int v = -1;
    size_t n = sizeof(v);
    return sysctlbyname("machdep.ptrauth_enabled", &v, &n, NULL, 0) == 0 ? v : -1;
}

static uint64_t own[NKEYS][SAMPLES];

static void *
recheck_thread(void *arg)
{
    uintptr_t bad = 0;

    (void)arg;
    for (int round = 0; round < 500; round++) {
        for (int k = 0; k < NKEYS; k++) {
            for (int i = 0; i < SAMPLES; i++) {
                bad += sign(k, raw_value(i), 42 + (uint64_t)i) != own[k][i];
                if (k != GA) {
                    bad += auth(k, own[k][i], 42 + (uint64_t)i) != raw_value(i);
                }
            }
        }
        sched_yield();
    }
    return (void *)bad;
}

static volatile int handled;
static void on_usr1(int sig) { handled = sig; }
static void *thread_main(void *arg) { return (void *)((uintptr_t)arg + (uintptr_t)target_fp(1)); }
static int cmp_int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }
static jmp_buf jb;
static __attribute__((noinline)) void jump_back(void) { longjmp(jb, 7); }

int
main(int argc, char **argv)
{
    signatures(own);
    if (argc > 1 && strcmp(argv[1], "keys") == 0) {
        for (int k = 0; k < NKEYS; k++) {
            for (int i = 0; i < SAMPLES; i++) {
                printf("%016llx\n", (unsigned long long)own[k][i]);
            }
        }
        return 0;
    }
#if __arm64e__
    const int keys = 1;
#else
    const int keys = 0;
#endif
    CHECK(ptrauth_enabled() == keys);

    for (int k = 0; k < GA; k++) {
        int signed_values = 0;
        for (int i = 0; i < SAMPLES; i++) {
            signed_values += own[k][i] != raw_value(i);
            CHECK(auth(k, own[k][i], 42 + (uint64_t)i) == raw_value(i));
        }
        int as_abi = keys ? signed_values > 0 : signed_values == 0;
        if (!as_abi) {
            printf("  key %s: %d of %d values signed\n", key_names[k], signed_values, SAMPLES);
        }
        CHECK(as_abi);
    }
#if __has_feature(ptrauth_intrinsics)
    CHECK((uint64_t)__builtin_ptrauth_sign_unauthenticated((void *)raw_value(0), ptrauth_key_asia, 42) == own[0][0]);
    CHECK((uint64_t)__builtin_ptrauth_sign_unauthenticated((void *)raw_value(0), ptrauth_key_asib, 42) == own[1][0]);
    CHECK((uint64_t)__builtin_ptrauth_sign_unauthenticated((void *)raw_value(0), ptrauth_key_asda, 42) == own[2][0]);
    CHECK((uint64_t)__builtin_ptrauth_sign_unauthenticated((void *)raw_value(0), ptrauth_key_asdb, 42) == own[3][0]);
    CHECK((uint64_t)__builtin_ptrauth_sign_generic_data(raw_value(0), 42) == own[GA][0]);
#endif

    pthread_t threads[4];
    void *bad = NULL;
    uintptr_t mismatches = 0;
    for (int t = 0; t < 4; t++) {
        CHECK(pthread_create(&threads[t], NULL, recheck_thread, NULL) == 0);
    }
    for (int t = 0; t < 4; t++) {
        CHECK(pthread_join(threads[t], &bad) == 0);
        mismatches += (uintptr_t)bad;
    }
    CHECK(mismatches == 0);

    int fds[2];
    CHECK(pipe(fds) == 0);
    pid_t pid = fork();
    if (pid == 0) {
        uint64_t forked[NKEYS][SAMPLES];
        signatures(forked);
        _exit(write(fds[1], forked, sizeof(forked)) == sizeof(forked) ? 0 : 1);
    }
    uint64_t forked[NKEYS][SAMPLES];
    memset(forked, 0, sizeof(forked));
    CHECK(read(fds[0], forked, sizeof(forked)) == sizeof(forked));
    waitpid(pid, NULL, 0);
    close(fds[0]);
    close(fds[1]);
    for (int k = 0; k < NKEYS; k++) {
        int kept = !any_differs(forked[k], own[k]);
        if (!kept) {
            printf("  key %s changed in the forked child\n", key_names[k]);
        }
        CHECK(kept);
    }

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s keys", argv[0]);
    uint64_t other[NKEYS][SAMPLES];
    int got = 0;
    FILE *f = popen(cmd, "r");
    CHECK(f != NULL);
    if (f != NULL) {
        unsigned long long v;
        while (got < NKEYS * SAMPLES && fscanf(f, "%llx", &v) == 1) {
            other[got / SAMPLES][got % SAMPLES] = v;
            got++;
        }
        pclose(f);
    }
    CHECK(got == NKEYS * SAMPLES);
    for (int k = 0; k < NKEYS && got == NKEYS * SAMPLES; k++) {
        int fresh = keys || k == GA;
        int as_expected = any_differs(other[k], own[k]) == fresh;
        if (!as_expected) {
            printf("  key %s of another exec: %s\n", key_names[k], fresh ? "same as this process's" : "differs");
        }
        CHECK(as_expected);
    }

    CHECK(((int (*)(int))sign(0, strip_code((uint64_t)target_fp), 0))(1) == 3);
    pid = fork();
    if (pid == 0) {
        int (*fp)(int) = (int (*)(int))auth(0, sign(0, strip_code((uint64_t)target_fp), 99), 0);
        _exit(fp(1) == 3 ? 0 : 2);
    }
    int st = 0;
    waitpid(pid, &st, 0);
    CHECK(keys ? WIFSIGNALED(st) : (WIFEXITED(st) && WEXITSTATUS(st) == 0));

    signal(SIGUSR1, on_usr1);
    raise(SIGUSR1);
    CHECK(handled == SIGUSR1);
    pthread_t t;
    void *ret = NULL;
    CHECK(pthread_create(&t, NULL, thread_main, (void *)10) == 0 && pthread_join(t, &ret) == 0);
    CHECK((uintptr_t)ret == 13);
    int a[] = { 5, 3, 9, 1 };
    qsort(a, 4, sizeof(int), cmp_int);
    CHECK(a[0] == 1 && a[3] == 9);
    int j = setjmp(jb);
    if (j == 0) {
        jump_back();
    }
    CHECK(j == 7);
    int (*dl)(const char *) = (int (*)(const char *))dlsym(RTLD_DEFAULT, "atoi");
    CHECK(dl != NULL && dl("41") == 41);

    int base = 40;
    __block int (^inner)(int) = ^(int v) { return v + base; };
    void *on_stack = (void *)&inner;
    __block int sum = 0;
    __block void *moved = NULL;
    dispatch_group_t group = dispatch_group_create();
    dispatch_group_async(group, dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
        sum = inner(2);
        moved = (void *)&inner;
    });
    CHECK(dispatch_group_wait(group, dispatch_time(DISPATCH_TIME_NOW, 30 * NSEC_PER_SEC)) == 0);
    CHECK(sum == 42 && moved != NULL && moved != on_stack);
    dispatch_release(group);
    int b[] = { 5, 3, 9, 1 };
    qsort_b(b, 4, sizeof(int), ^(const void *x, const void *y) { return *(const int *)x - *(const int *)y; });
    CHECK(b[0] == 1 && b[3] == 9);

    return checks_done(keys ? "pac arm64e" : "pac arm64");
}
