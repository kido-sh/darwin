/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <mach/mach.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <unistd.h>
#if __has_feature(ptrauth_intrinsics)
#include <ptrauth.h>
#endif

#include "../libsystem/check.h"

#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#define TEST_SYS_PIVOT_ROOT 537
#define TEST_CS_EXEMPTION_MASK 0x7fU
#define TEST_TTBR_SELECTOR (1ULL << 55)

#if __DARWIN_OPAQUE_ARM_THREAD_STATE64
#define TS_SET_RAW_PC(ts, v) ((ts).__opaque_pc = (void *)(uintptr_t)(v))
#define TS_SET_RAW_LR(ts, v) ((ts).__opaque_lr = (void *)(uintptr_t)(v))
#define TS_SET_RAW_SP(ts, v) ((ts).__opaque_sp = (void *)(uintptr_t)(v))
#define TS_SET_RAW_FP(ts, v) ((ts).__opaque_fp = (void *)(uintptr_t)(v))
#define TS_SET_NO_PTRAUTH(ts) ((ts).__opaque_flags = __DARWIN_ARM_THREAD_STATE64_FLAGS_NO_PTRAUTH)
#else
#define TS_SET_RAW_PC(ts, v) ((ts).__pc = (uint64_t)(v))
#define TS_SET_RAW_LR(ts, v) ((ts).__lr = (uint64_t)(v))
#define TS_SET_RAW_SP(ts, v) ((ts).__sp = (uint64_t)(v))
#define TS_SET_RAW_FP(ts, v) ((ts).__fp = (uint64_t)(v))
#define TS_SET_NO_PTRAUTH(ts) ((ts).__pad = 1)
#endif

extern int syscall(int number, ...);

static void
dummy_fn(void)
{
}

#if __arm64e__
__attribute__((naked)) static void
pac_thread_ret(void)
{
    __asm__ volatile(
        "mov  w10, #1\n"
        "stlr w10, [x0]\n"
        "1: yield\n"
        "b 1b\n"
    );
}

__attribute__((naked)) static void
pac_thread_entry(void)
{
    __asm__ volatile(
        "mov  x12, sp\n"
        "lsr  x12, x12, #56\n"
        "cmp  x12, #0x5a\n"
        "b.ne 1f\n"
        "lsr  x12, x29, #56\n"
        "cmp  x12, #0x3c\n"
        "b.ne 1f\n"
        "mov  x9, #0x42\n"
        "str  x9, [sp, #-16]\n"
        "ldr  x10, [sp, #-16]\n"
        "str  x10, [x29, #-16]\n"
        "ldr  x11, [x29, #-16]\n"
        "cmp  x11, #0x42\n"
        "b.ne 1f\n"
        "retab\n"
        "1: brk #1\n"
    );
}

static void
on_fatal_sig(int sig)
{
    (void)sig;
    _exit(42);
}

static void
check_arm64e_pac_thread_state(void)
{
    const size_t stack_size = 65536;
    void *stack = mmap(NULL, stack_size, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANON, -1, 0);
    CHECK(stack != MAP_FAILED);
    if (stack == MAP_FAILED) {
        return;
    }

    uintptr_t raw_sp = ((uintptr_t)stack + stack_size - 256) & ~(uintptr_t)15;
    uintptr_t tagged_sp = raw_sp | (0x5aULL << 56);
    uintptr_t tagged_fp = (raw_sp + 64) | (0x3cULL << 56);

    volatile int done = 0;
    arm_thread_state64_t ts;
    memset(&ts, 0, sizeof(ts));
    ts.__x[0] = (uint64_t)(uintptr_t)&done;
    arm_thread_state64_set_pc_fptr(ts, &pac_thread_entry);
    arm_thread_state64_set_sp(ts, tagged_sp);
    arm_thread_state64_set_fp(ts, tagged_fp);

    uint64_t ret_raw = (uint64_t)(uintptr_t)ptrauth_strip((void *)pac_thread_ret,
        ptrauth_key_function_pointer);
    uint64_t ib_lr = ret_raw;
    __asm__ volatile(".arch_extension pauth\npacib %0, %1"
        : "+r"(ib_lr) : "r"((uint64_t)tagged_sp));
    TS_SET_RAW_LR(ts, ib_lr);
    ts.__opaque_flags |= __DARWIN_ARM_THREAD_STATE64_FLAGS_IB_SIGNED_LR;

    CHECK((ts.__opaque_flags & __DARWIN_ARM_THREAD_STATE64_FLAGS_NO_PTRAUTH) == 0);

    thread_act_t thr = MACH_PORT_NULL;
    CHECK(thread_create(mach_task_self(), &thr) == KERN_SUCCESS);
    if (thr != MACH_PORT_NULL) {
        kern_return_t kr = thread_set_state(thr, ARM_THREAD_STATE64,
            (thread_state_t)&ts, ARM_THREAD_STATE64_COUNT);
        CHECK(kr == KERN_SUCCESS);
        if (kr == KERN_SUCCESS) {
            CHECK(thread_resume(thr) == KERN_SUCCESS);
            for (int i = 0; i < 5000 && __atomic_load_n(&done, __ATOMIC_ACQUIRE) == 0; i++) {
                usleep(1000);
            }
            CHECK(__atomic_load_n(&done, __ATOMIC_ACQUIRE) == 1);
        }
        (void)thread_terminate(thr);
        mach_port_deallocate(mach_task_self(), thr);
    }
    munmap(stack, stack_size);
}

static void
check_arm64e_fatal_pac(const char *self_path)
{
    int fpac = 0;
    size_t fpac_len = sizeof(fpac);
    if (sysctlbyname("hw.optional.arm.FEAT_FPAC", &fpac, &fpac_len, NULL, 0) != 0 || fpac == 0) {
        return;
    }

    int st = 0;
    pid_t pid = fork();
    CHECK(pid >= 0);
    if (pid == 0) {
        execl(self_path, self_path, "debug_self", (char *)NULL);
        _exit(127);
    }
    if (pid > 0) {
        CHECK(waitpid(pid, &st, 0) == pid);
        CHECK(WIFSIGNALED(st) && WTERMSIG(st) == SIGKILL);
    }

    st = 0;
    pid = fork();
    CHECK(pid >= 0);
    if (pid == 0) {
        arm_debug_state64_t dbg;
        memset(&dbg, 0, sizeof(dbg));
        kern_return_t kr = thread_set_state(mach_thread_self(), ARM_DEBUG_STATE64,
            (thread_state_t)&dbg, ARM_DEBUG_STATE64_COUNT);
        _exit(kr == KERN_SUCCESS ? 0 : 1);
    }
    if (pid > 0) {
        CHECK(waitpid(pid, &st, 0) == pid);
        CHECK(WIFSIGNALED(st) && WTERMSIG(st) == SIGKILL);
    }

    st = 0;
    pid = fork();
    CHECK(pid >= 0);
    if (pid == 0) {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = on_fatal_sig;
        sigaction(SIGSEGV, &sa, NULL);
        sigaction(SIGBUS, &sa, NULL);
        sigaction(SIGILL, &sa, NULL);
        sigaction(SIGTRAP, &sa, NULL);

        uint64_t raw = (uint64_t)(uintptr_t)ptrauth_strip((void *)dummy_fn,
            ptrauth_key_function_pointer);
        uint64_t signed_bad = raw;
        __asm__ volatile(".arch_extension pauth\npacia %0, %1"
            : "+r"(signed_bad) : "r"((uint64_t)99));
        uint64_t authed = signed_bad;
        __asm__ volatile(".arch_extension pauth\nautia %0, %1"
            : "+r"(authed) : "r"((uint64_t)0));
        __asm__ volatile("blr %0" :: "r"(authed) : "x30", "memory");
        _exit(0);
    }
    if (pid > 0) {
        CHECK(waitpid(pid, &st, 0) == pid);
        CHECK(WIFSIGNALED(st) && WTERMSIG(st) == SIGKILL);
    }

    st = 0;
    pid = fork();
    CHECK(pid >= 0);
    if (pid == 0) {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = on_fatal_sig;
        sigaction(SIGTRAP, &sa, NULL);
        sigaction(SIGILL, &sa, NULL);
        sigaction(SIGSEGV, &sa, NULL);
        __asm__ volatile("brk #0xc470");
        _exit(0);
    }
    if (pid > 0) {
        CHECK(waitpid(pid, &st, 0) == pid);
        CHECK(WIFSIGNALED(st) && WTERMSIG(st) == SIGKILL);
    }
}
#else
static void
check_arm64_cross_abi_thread_state(const char *arm64e_path)
{
    thread_act_t self_thr = MACH_PORT_NULL;
    CHECK(thread_create(mach_task_self(), &self_thr) == KERN_SUCCESS);
    if (self_thr != MACH_PORT_NULL) {
        arm_thread_state64_t ts;
        mach_msg_type_number_t count = ARM_THREAD_STATE64_COUNT;
        memset(&ts, 0, sizeof(ts));
        CHECK(thread_get_state(self_thr, ARM_THREAD_STATE64,
            (thread_state_t)&ts, &count) == KERN_SUCCESS);
        TS_SET_RAW_PC(ts, (uintptr_t)dummy_fn);
        TS_SET_RAW_SP(ts, ((uintptr_t)&ts) & ~(uintptr_t)15);
        CHECK(thread_set_state(self_thr, ARM_THREAD_STATE64,
            (thread_state_t)&ts, ARM_THREAD_STATE64_COUNT) == KERN_SUCCESS);
        (void)thread_terminate(self_thr);
        mach_port_deallocate(mach_task_self(), self_thr);
    }

    int pfd[2] = { -1, -1 };
    int prc = pipe(pfd);
    CHECK(prc == 0);
    if (prc != 0) {
        return;
    }
    pid_t pid = fork();
    CHECK(pid >= 0);
    if (pid == 0) {
        char fd_str[32];
        close(pfd[0]);
        snprintf(fd_str, sizeof(fd_str), "%d", pfd[1]);
        execl(arm64e_path, arm64e_path, "pause", fd_str, (char *)NULL);
        _exit(127);
    }
    close(pfd[1]);
    char ready = 0;
    CHECK(read(pfd[0], &ready, 1) == 1 && ready == 'R');
    close(pfd[0]);

    if (pid > 0) {
        task_t task = MACH_PORT_NULL;
        CHECK(task_for_pid(mach_task_self(), pid, &task) == KERN_SUCCESS);
        if (task != MACH_PORT_NULL) {
            thread_act_array_t threads = NULL;
            mach_msg_type_number_t tcount = 0;
            CHECK(task_threads(task, &threads, &tcount) == KERN_SUCCESS && tcount >= 1);
            if (threads != NULL && tcount >= 1) {
                arm_thread_state64_t ts;
                mach_msg_type_number_t scount = ARM_THREAD_STATE64_COUNT;
                CHECK(thread_suspend(threads[0]) == KERN_SUCCESS);
                CHECK(thread_get_state(threads[0], ARM_THREAD_STATE64,
                    (thread_state_t)&ts, &scount) == KERN_SUCCESS);
                CHECK(thread_set_state(threads[0], ARM_THREAD_STATE64,
                    (thread_state_t)&ts, ARM_THREAD_STATE64_COUNT) == KERN_PROTECTION_FAILURE);
                for (mach_msg_type_number_t i = 0; i < tcount; i++) {
                    mach_port_deallocate(mach_task_self(), threads[i]);
                }
                vm_deallocate(mach_task_self(), (vm_address_t)(uintptr_t)threads,
                    tcount * sizeof(thread_act_t));
            }
            mach_port_deallocate(mach_task_self(), task);
        }
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
    }
}
#endif

static void
check_kernel_range_thread_state(void)
{
    thread_act_t thr = MACH_PORT_NULL;
    CHECK(thread_create(mach_task_self(), &thr) == KERN_SUCCESS);
    if (thr == MACH_PORT_NULL) {
        return;
    }

    arm_thread_state64_t base;
    mach_msg_type_number_t count = ARM_THREAD_STATE64_COUNT;
    memset(&base, 0, sizeof(base));
    CHECK(thread_get_state(thr, ARM_THREAD_STATE64,
        (thread_state_t)&base, &count) == KERN_SUCCESS);

    arm_thread_state64_t ts = base;
    TS_SET_NO_PTRAUTH(ts);
    TS_SET_RAW_PC(ts, TEST_TTBR_SELECTOR | 0x1000ULL);
    CHECK(thread_set_state(thr, ARM_THREAD_STATE64,
        (thread_state_t)&ts, ARM_THREAD_STATE64_COUNT) == KERN_PROTECTION_FAILURE);

    ts = base;
    TS_SET_NO_PTRAUTH(ts);
    TS_SET_RAW_SP(ts, TEST_TTBR_SELECTOR | 0x1000ULL);
    CHECK(thread_set_state(thr, ARM_THREAD_STATE64,
        (thread_state_t)&ts, ARM_THREAD_STATE64_COUNT) == KERN_PROTECTION_FAILURE);

    ts = base;
    TS_SET_NO_PTRAUTH(ts);
    TS_SET_RAW_LR(ts, TEST_TTBR_SELECTOR | 0x1000ULL);
    CHECK(thread_set_state(thr, ARM_THREAD_STATE64,
        (thread_state_t)&ts, ARM_THREAD_STATE64_COUNT) == KERN_PROTECTION_FAILURE);

    ts = base;
    TS_SET_NO_PTRAUTH(ts);
    TS_SET_RAW_FP(ts, TEST_TTBR_SELECTOR | 0x1000ULL);
    CHECK(thread_set_state(thr, ARM_THREAD_STATE64,
        (thread_state_t)&ts, ARM_THREAD_STATE64_COUNT) == KERN_PROTECTION_FAILURE);

    (void)thread_terminate(thr);
    mach_port_deallocate(mach_task_self(), thr);
}

static void
check_legacy_mach_msg(void)
{
    mach_port_t port = MACH_PORT_NULL;
    CHECK(mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port) == KERN_SUCCESS);
    CHECK(mach_port_insert_right(mach_task_self(), port, port,
        MACH_MSG_TYPE_MAKE_SEND) == KERN_SUCCESS);

    mach_msg_header_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.msgh_bits = MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_COPY_SEND);
    tx.msgh_size = sizeof(tx);
    tx.msgh_remote_port = port;
    tx.msgh_id = 0x55555;
    CHECK(mach_msg(&tx, MACH_SEND_MSG | MACH_SEND_TIMEOUT, sizeof(tx), 0,
        MACH_PORT_NULL, 1000, MACH_PORT_NULL) == MACH_MSG_SUCCESS);

    struct {
        mach_msg_header_t hdr;
        mach_msg_trailer_t trailer;
    } rx;
    memset(&rx, 0, sizeof(rx));
    CHECK(mach_msg(&rx.hdr, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(rx),
        port, 1000, MACH_PORT_NULL) == MACH_MSG_SUCCESS);
    CHECK(rx.hdr.msgh_id == 0x55555);
    mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE, -1);
    mach_port_deallocate(mach_task_self(), port);

    int st = 0;
    pid_t pid = fork();
    CHECK(pid >= 0);
    if (pid == 0) {
        mach_port_t cport = MACH_PORT_NULL;
        if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &cport) != KERN_SUCCESS ||
            mach_port_insert_right(mach_task_self(), cport, cport, MACH_MSG_TYPE_MAKE_SEND) != KERN_SUCCESS) {
            _exit(2);
        }
        mach_msg_header_t hdr;
        memset(&hdr, 0, sizeof(hdr));
        hdr.msgh_bits = MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_COPY_SEND);
        hdr.msgh_size = sizeof(hdr);
        hdr.msgh_remote_port = cport;
        hdr.msgh_id = 0x55555;

        register uint64_t x0 __asm__("x0") = (uint64_t)(uintptr_t)&hdr;
        register uint64_t x1 __asm__("x1") = MACH_SEND_MSG | MACH_SEND_TIMEOUT;
        register uint64_t x2 __asm__("x2") = sizeof(hdr);
        register uint64_t x3 __asm__("x3") = 0;
        register uint64_t x4 __asm__("x4") = MACH_PORT_NULL;
        register uint64_t x5 __asm__("x5") = 1000;
        register uint64_t x6 __asm__("x6") = MACH_PORT_NULL;
        register uint64_t x16 __asm__("x16") = (uint64_t)(int64_t)-31;
        __asm__ volatile("svc #0x80"
            : "+r"(x0)
            : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x6), "r"(x16)
            : "memory");
        _exit((int)x0 == 0 ? 0 : 1);
    }
    if (pid > 0) {
        CHECK(waitpid(pid, &st, 0) == pid);
        CHECK(WIFSIGNALED(st) && WTERMSIG(st) == SIGKILL);
    }
}

static void
check_release_sanity(void)
{
    uint32_t cfg = 0xffffffffU;
    size_t len = sizeof(cfg);
    CHECK(sysctlbyname("security.codesigning.config", &cfg, &len, NULL, 0) == 0);
    CHECK((cfg & TEST_CS_EXEMPTION_MASK) == 0);

    errno = 0;
    CHECK(syscall(TEST_SYS_PIVOT_ROOT, "/", "/") == -1 && errno == EPERM);
}

int
main(int argc, char **argv)
{
    if (argc > 2 && strcmp(argv[1], "pause") == 0) {
        int fd = atoi(argv[2]);
        char ch = 'R';
        (void)write(fd, &ch, 1);
        close(fd);
        for (;;) {
            pause();
        }
    }
#if __arm64e__
    if (argc > 1 && strcmp(argv[1], "debug_self") == 0) {
        arm_debug_state64_t dbg;
        memset(&dbg, 0, sizeof(dbg));
        kern_return_t kr = thread_set_state(mach_thread_self(), ARM_DEBUG_STATE64,
            (thread_state_t)&dbg, ARM_DEBUG_STATE64_COUNT);
        return kr == KERN_SUCCESS ? 0 : 1;
    }
    check_arm64e_pac_thread_state();
    check_arm64e_fatal_pac(argv[0]);
#else
    CHECK(argc > 1);
    if (argc > 1) {
        check_arm64_cross_abi_thread_state(argv[1]);
    }
#endif
    check_kernel_range_thread_state();
    check_legacy_mach_msg();
    check_release_sanity();

#if __arm64e__
    return checks_done("hardening arm64e");
#else
    return checks_done("hardening arm64");
#endif
}
