/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../libsystem/check.h"

#define ENC_LRCPC2(sz, opc, imm9, rn, rt) \
    (((uint32_t)(sz) << 30) | 0x19000000U | ((uint32_t)(opc) << 22) | \
    (((uint32_t)(imm9) & 0x1FFU) << 12) | ((uint32_t)(rn) << 5) | (uint32_t)(rt))

#define ENC_RMIF(rn, lsh, mask) \
    (0xBA000400U | (((uint32_t)(lsh) & 0x3FU) << 15) | \
    (((uint32_t)(rn) & 0x1FU) << 5) | ((uint32_t)(mask) & 0xFU))

#define ENC_SETF8(rn)  (0x3A00080DU | (((uint32_t)(rn) & 0x1FU) << 5))
#define ENC_SETF16(rn) (0x3A00480DU | (((uint32_t)(rn) & 0x1FU) << 5))

#define LRCPC2_STORE(sz, imm9, base_ptr, val64) do { \
        register uint64_t r_base __asm__("x9") = (uint64_t)(base_ptr); \
        register uint64_t r_val __asm__("x10") = (uint64_t)(val64); \
        __asm__ volatile( \
            ".inst %c[insn]\n" \
            :: "r"(r_base), "r"(r_val), \
               [insn] "i"(ENC_LRCPC2(sz, 0, imm9, 9, 10)) \
            : "memory"); \
    } while (0)

#define LRCPC2_STORE_ZR(sz, imm9, base_ptr) do { \
        register uint64_t r_base __asm__("x9") = (uint64_t)(base_ptr); \
        __asm__ volatile( \
            ".inst %c[insn]\n" \
            :: "r"(r_base), \
               [insn] "i"(ENC_LRCPC2(sz, 0, imm9, 9, 31)) \
            : "memory"); \
    } while (0)

#define LRCPC2_LOAD(sz, opc, imm9, base_ptr) ({ \
        register uint64_t r_base __asm__("x9") = (uint64_t)(base_ptr); \
        register uint64_t r_val __asm__("x10") = 0xAAAAAAAAAAAAAAAAULL; \
        __asm__ volatile( \
            ".inst %c[insn]\n" \
            : "+r"(r_val) \
            : "r"(r_base), \
              [insn] "i"(ENC_LRCPC2(sz, opc, imm9, 9, 10)) \
            : "memory"); \
        r_val; \
    })

#define LRCPC2_LOAD_ZR(sz, opc, imm9, base_ptr) do { \
        register uint64_t r_base __asm__("x9") = (uint64_t)(base_ptr); \
        __asm__ volatile( \
            ".inst %c[insn]\n" \
            :: "r"(r_base), \
               [insn] "i"(ENC_LRCPC2(sz, opc, imm9, 9, 31)) \
            : "memory"); \
    } while (0)

static void
check_lrcpc2_all_variants(void)
{
    uint8_t buf[1024] __attribute__((aligned(64)));
    memset(buf, 0xCC, sizeof(buf));
    uint8_t *mid = buf + 512;

    LRCPC2_STORE(0, 17, mid, 0x12345685ULL);
    CHECK(mid[17] == 0x85);
    CHECK(LRCPC2_LOAD(0, 1, 17, mid) == 0x0000000000000085ULL);
    CHECK(LRCPC2_LOAD(0, 2, 17, mid) == 0xFFFFFFFFFFFFFF85ULL);
    CHECK(LRCPC2_LOAD(0, 3, 17, mid) == 0x00000000FFFFFF85ULL);

    LRCPC2_STORE(0, -17, mid, 0x7AULL);
    CHECK(mid[-17] == 0x7A);
    CHECK(LRCPC2_LOAD(0, 1, -17, mid) == 0x7AULL);
    CHECK(LRCPC2_LOAD(0, 2, -17, mid) == 0x7AULL);
    CHECK(LRCPC2_LOAD(0, 3, -17, mid) == 0x7AULL);

    LRCPC2_STORE(0, -256, mid, 0x9BULL);
    CHECK(LRCPC2_LOAD(0, 1, -256, mid) == 0x9BULL);
    LRCPC2_STORE(0, 255, mid, 0xE1ULL);
    CHECK(LRCPC2_LOAD(0, 1, 255, mid) == 0xE1ULL);
    LRCPC2_STORE_ZR(0, 17, mid);
    CHECK(LRCPC2_LOAD(0, 1, 17, mid) == 0ULL);

    LRCPC2_STORE(1, 18, mid, 0x8005ULL);
    CHECK(LRCPC2_LOAD(1, 1, 18, mid) == 0x0000000000008005ULL);
    CHECK(LRCPC2_LOAD(1, 2, 18, mid) == 0xFFFFFFFFFFFF8005ULL);
    CHECK(LRCPC2_LOAD(1, 3, 18, mid) == 0x00000000FFFF8005ULL);

    LRCPC2_STORE(1, -19, mid, 0x7005ULL);
    CHECK(LRCPC2_LOAD(1, 1, -19, mid) == 0x7005ULL);
    CHECK(LRCPC2_LOAD(1, 2, -19, mid) == 0x7005ULL);
    CHECK(LRCPC2_LOAD(1, 3, -19, mid) == 0x7005ULL);

    LRCPC2_STORE(1, 3, mid, 0xFEDCULL);
    CHECK(LRCPC2_LOAD(1, 1, 3, mid) == 0xFEDCULL);
    LRCPC2_STORE_ZR(1, 18, mid);
    CHECK(LRCPC2_LOAD(1, 1, 18, mid) == 0ULL);

    LRCPC2_STORE(2, 20, mid, 0x1122334480001234ULL);
    CHECK(LRCPC2_LOAD(2, 1, 20, mid) == 0x0000000080001234ULL);
    CHECK(LRCPC2_LOAD(2, 2, 20, mid) == 0xFFFFFFFF80001234ULL);

    LRCPC2_STORE(2, -20, mid, 0x70001234ULL);
    CHECK(LRCPC2_LOAD(2, 1, -20, mid) == 0x70001234ULL);
    CHECK(LRCPC2_LOAD(2, 2, -20, mid) == 0x70001234ULL);

    LRCPC2_STORE(2, 5, mid, 0xDEADBEEFULL);
    CHECK(LRCPC2_LOAD(2, 1, 5, mid) == 0xDEADBEEFULL);
    CHECK(LRCPC2_LOAD(2, 2, 5, mid) == 0xFFFFFFFFDEADBEEFULL);
    LRCPC2_STORE_ZR(2, 20, mid);
    CHECK(LRCPC2_LOAD(2, 1, 20, mid) == 0ULL);

    LRCPC2_STORE(3, 24, mid, 0x0123456789ABCDEFULL);
    CHECK(LRCPC2_LOAD(3, 1, 24, mid) == 0x0123456789ABCDEFULL);
    LRCPC2_STORE(3, -24, mid, 0xFEDCBA9876543210ULL);
    CHECK(LRCPC2_LOAD(3, 1, -24, mid) == 0xFEDCBA9876543210ULL);

    LRCPC2_STORE(3, 3, mid, 0x1122334455667788ULL);
    CHECK(LRCPC2_LOAD(3, 1, 3, mid) == 0x1122334455667788ULL);
    LRCPC2_STORE(3, -13, mid, 0xCAFEBABEDEADBEEFULL);
    CHECK(LRCPC2_LOAD(3, 1, -13, mid) == 0xCAFEBABEDEADBEEFULL);

    LRCPC2_STORE_ZR(3, 24, mid);
    CHECK(LRCPC2_LOAD(3, 1, 24, mid) == 0ULL);
    LRCPC2_LOAD_ZR(0, 1, 17, mid);
    LRCPC2_LOAD_ZR(3, 1, 24, mid);

    uintptr_t tagged = (uintptr_t)mid | (0x5AULL << 56);
    LRCPC2_STORE(3, 24, tagged, 0xAABBCCDDEEFF0011ULL);
    CHECK(LRCPC2_LOAD(3, 1, 24, mid) == 0xAABBCCDDEEFF0011ULL);
    CHECK(LRCPC2_LOAD(3, 1, 24, tagged) == 0xAABBCCDDEEFF0011ULL);
}

static void
check_lrcpc2_sp_fp_lr(void)
{
    uint64_t o64 = 0, o32 = 0, o16 = 0, o8 = 0, neg64 = 0;
    uint64_t v64 = 0x13579BDF2468ACE0ULL;
    uint64_t v32 = 0x89ABCDEFULL;
    uint64_t v16 = 0xBEEFULL;
    uint64_t v8 = 0xABULL;

    __asm__ volatile(
        "sub sp, sp, #64\n"
        "mov x10, %[v64]\n"
        ".inst %c[st64]\n"
        "mov x10, #0\n"
        ".inst %c[ld64]\n"
        "mov %[o64], x10\n"
        "mov x10, %[v32]\n"
        ".inst %c[st32]\n"
        "mov x10, #-1\n"
        ".inst %c[ld32]\n"
        "mov %[o32], x10\n"
        "mov x10, %[v16]\n"
        ".inst %c[st16]\n"
        "mov x10, #-1\n"
        ".inst %c[ld16]\n"
        "mov %[o16], x10\n"
        "mov x10, %[v8]\n"
        ".inst %c[st8]\n"
        "mov x10, #-1\n"
        ".inst %c[ld8]\n"
        "mov %[o8], x10\n"
        "add sp, sp, #32\n"
        "mov x10, %[v64]\n"
        ".inst %c[stn64]\n"
        "mov x10, #0\n"
        ".inst %c[ldn64]\n"
        "mov %[neg64], x10\n"
        "add sp, sp, #32\n"
        : [o64] "=&r"(o64), [o32] "=&r"(o32),
          [o16] "=&r"(o16), [o8] "=&r"(o8), [neg64] "=&r"(neg64)
        : [v64] "r"(v64), [v32] "r"(v32), [v16] "r"(v16), [v8] "r"(v8),
          [st64] "i"(ENC_LRCPC2(3, 0, 16, 31, 10)),
          [ld64] "i"(ENC_LRCPC2(3, 1, 16, 31, 10)),
          [st32] "i"(ENC_LRCPC2(2, 0, 24, 31, 10)),
          [ld32] "i"(ENC_LRCPC2(2, 1, 24, 31, 10)),
          [st16] "i"(ENC_LRCPC2(1, 0, 28, 31, 10)),
          [ld16] "i"(ENC_LRCPC2(1, 1, 28, 31, 10)),
          [st8] "i"(ENC_LRCPC2(0, 0, 30, 31, 10)),
          [ld8] "i"(ENC_LRCPC2(0, 1, 30, 31, 10)),
          [stn64] "i"(ENC_LRCPC2(3, 0, -16, 31, 10)),
          [ldn64] "i"(ENC_LRCPC2(3, 1, -16, 31, 10))
        : "x10", "memory"
    );
    CHECK(o64 == v64);
    CHECK(o32 == v32);
    CHECK(o16 == v16);
    CHECK(o8 == v8);
    CHECK(neg64 == v64);

    uint64_t slots[4] = { 0, 0, 0, 0 };
    register uint64_t r_base __asm__("x9") = (uint64_t)(uintptr_t)slots;
    register uint64_t r_val __asm__("x10") = v64;
    register uint64_t r_lr_rt __asm__("x12") = 0;
    register uint64_t r_lr_rn __asm__("x13") = 0;
    __asm__ volatile(
        "mov x30, %[v64]\n"
        ".inst %c[st_lr_rt]\n"
        "mov x30, #0\n"
        ".inst %c[ld_lr_rt]\n"
        "mov %[o_rt], x30\n"
        "mov x30, %[base]\n"
        ".inst %c[st_lr_rn]\n"
        "mov x14, #0\n"
        ".inst %c[ld_lr_rn]\n"
        "mov %[o_rn], x14\n"
        : [o_rt] "=&r"(r_lr_rt), [o_rn] "=&r"(r_lr_rn)
        : [v64] "r"(r_val), [base] "r"(r_base),
          [st_lr_rt] "i"(ENC_LRCPC2(3, 0, 8, 9, 30)),
          [ld_lr_rt] "i"(ENC_LRCPC2(3, 1, 8, 9, 30)),
          [st_lr_rn] "i"(ENC_LRCPC2(3, 0, 16, 30, 10)),
          [ld_lr_rn] "i"(ENC_LRCPC2(3, 1, 16, 30, 14))
        : "x14", "x30", "memory"
    );
    CHECK(slots[1] == v64);
    CHECK(slots[2] == v64);
    CHECK(r_lr_rt == v64);
    CHECK(r_lr_rn == v64);

    uint64_t fp_slots[4] = { 0, 0, 0, 0 };
    register uint64_t f_base __asm__("x9") = (uint64_t)(uintptr_t)fp_slots;
    register uint64_t f_val __asm__("x10") = 0x2468ACE013579BDFULL;
    register uint64_t f_rt_out __asm__("x12") = 0;
    register uint64_t f_rn_out __asm__("x13") = 0;
    __asm__ volatile(
        "mov x11, x29\n"
        "mov x29, %[val]\n"
        ".inst %c[st_fp_rt]\n"
        "mov x29, #0\n"
        ".inst %c[ld_fp_rt]\n"
        "mov %[o_rt], x29\n"
        "mov x29, %[base]\n"
        ".inst %c[st_fp_rn]\n"
        "mov x14, #0\n"
        ".inst %c[ld_fp_rn]\n"
        "mov %[o_rn], x14\n"
        "mov x29, x11\n"
        : [o_rt] "=&r"(f_rt_out), [o_rn] "=&r"(f_rn_out)
        : [val] "r"(f_val), [base] "r"(f_base),
          [st_fp_rt] "i"(ENC_LRCPC2(3, 0, 8, 9, 29)),
          [ld_fp_rt] "i"(ENC_LRCPC2(3, 1, 8, 9, 29)),
          [st_fp_rn] "i"(ENC_LRCPC2(3, 0, 16, 29, 10)),
          [ld_fp_rn] "i"(ENC_LRCPC2(3, 1, 16, 29, 14))
        : "x11", "x14", "memory"
    );
    CHECK(fp_slots[1] == f_val);
    CHECK(fp_slots[2] == f_val);
    CHECK(f_rt_out == f_val);
    CHECK(f_rn_out == f_val);
}

struct sig_result {
    int sig;
    uintptr_t addr;
};

static int fault_pipe_wr = -1;

static void
fault_sig_handler(int sig, siginfo_t *si, void *uc)
{
    (void)uc;
    struct sig_result res;
    res.sig = sig;
    res.addr = (uintptr_t)si->si_addr;
    (void)write(fault_pipe_wr, &res, sizeof(res));
    _exit(100 + sig);
}

static struct sig_result
capture_signal(void (*fn)(uintptr_t), uintptr_t arg)
{
    struct sig_result res = { 0, 0 };
    int pfd[2] = { -1, -1 };
    CHECK(pipe(pfd) == 0);
    if (pfd[0] < 0) {
        return res;
    }

    int st = 0;
    pid_t pid = fork();
    CHECK(pid >= 0);
    if (pid < 0) {
        close(pfd[0]);
        close(pfd[1]);
        return res;
    }
    if (pid == 0) {
        close(pfd[0]);
        fault_pipe_wr = pfd[1];
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_sigaction = fault_sig_handler;
        sa.sa_flags = SA_SIGINFO;
        sigaction(SIGSEGV, &sa, NULL);
        sigaction(SIGBUS, &sa, NULL);
        sigaction(SIGILL, &sa, NULL);
        fn(arg);
        _exit(0);
    }
    close(pfd[1]);
    (void)read(pfd[0], &res, sizeof(res));
    close(pfd[0]);
    CHECK(waitpid(pid, &st, 0) == pid);
    return res;
}

static void
fault_ldapur_off24(uintptr_t addr)
{
    (void)LRCPC2_LOAD(3, 1, 24, addr);
}

static void
fault_ldapur_off0(uintptr_t addr)
{
    (void)LRCPC2_LOAD(3, 1, 0, addr);
}

static void
fault_ldapur_neg256(uintptr_t addr)
{
    (void)LRCPC2_LOAD(3, 1, -256, addr);
}

static void
fault_ldapur_xzr(uintptr_t addr)
{
    LRCPC2_LOAD_ZR(3, 1, 0, addr);
}

static void
fault_stlur_off0(uintptr_t addr)
{
    LRCPC2_STORE(3, 0, addr, 0x42ULL);
}

static void
fault_misaligned_sp(uintptr_t unused)
{
    (void)unused;
    __asm__ volatile(
        "sub sp, sp, #8\n"
        ".inst %c[insn]\n"
        "add sp, sp, #8\n"
        :: [insn] "i"(ENC_LRCPC2(3, 1, 0, 31, 10))
        : "x10", "memory"
    );
}

static void
fault_unalloc_lrcpc2(uintptr_t which)
{
    if (which == 0) {
        __asm__ volatile(".inst 0x99C00020\n" ::: "x0", "memory");
    } else if (which == 1) {
        __asm__ volatile(".inst 0xD9800020\n" ::: "x0", "memory");
    } else {
        __asm__ volatile(".inst 0xD9C00020\n" ::: "x0", "memory");
    }
}

static int
is_access_sig(int sig)
{
    return sig == SIGSEGV || sig == SIGBUS;
}

static void
check_lrcpc2_faults(void)
{
    long pgsz = sysconf(_SC_PAGESIZE);
    size_t sz = pgsz > 0 ? (size_t)pgsz : 16384U;
    struct sig_result r;

    void *none_pg = mmap(NULL, sz, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    CHECK(none_pg != MAP_FAILED);
    if (none_pg != MAP_FAILED) {
        r = capture_signal(fault_ldapur_off24, (uintptr_t)none_pg);
        CHECK(is_access_sig(r.sig) && r.addr == (uintptr_t)none_pg + 24U);

        uintptr_t tagged = (uintptr_t)none_pg | (0x5AULL << 56);
        r = capture_signal(fault_ldapur_off24, tagged);
        CHECK(is_access_sig(r.sig) && r.addr == tagged + 24U);

        r = capture_signal(fault_ldapur_xzr, (uintptr_t)none_pg);
        CHECK(is_access_sig(r.sig) && r.addr == (uintptr_t)none_pg);

        munmap(none_pg, sz);
    }

    void *pair = mmap(NULL, sz * 2U, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    CHECK(pair != MAP_FAILED);
    if (pair != MAP_FAILED) {
        uint8_t *p1 = (uint8_t *)pair;
        uint8_t *p2 = p1 + sz;
        p1[sz - 1] = 0x55;
        CHECK(mprotect(p2, sz, PROT_NONE) == 0);
        uintptr_t straddle = ((uintptr_t)(p2 - 4)) | (0x5AULL << 56);
        uintptr_t p2_tagged = ((uintptr_t)p2) | (0x5AULL << 56);
        r = capture_signal(fault_ldapur_off0, straddle);
        CHECK(is_access_sig(r.sig) && (r.addr == straddle || r.addr == p2_tagged));
        munmap(pair, sz * 2U);
    }

    void *ro_pg = mmap(NULL, sz, PROT_READ, MAP_PRIVATE | MAP_ANON, -1, 0);
    CHECK(ro_pg != MAP_FAILED);
    if (ro_pg != MAP_FAILED) {
        r = capture_signal(fault_stlur_off0, (uintptr_t)ro_pg);
        CHECK(is_access_sig(r.sig) && r.addr == (uintptr_t)ro_pg);
        munmap(ro_pg, sz);
    }

    mach_vm_address_t xo = 0;
    kern_return_t kr = mach_vm_allocate(mach_task_self(), &xo, sz, VM_FLAGS_ANYWHERE);
    CHECK(kr == KERN_SUCCESS);
    if (kr == KERN_SUCCESS) {
        volatile uint32_t *code = (volatile uint32_t *)(uintptr_t)xo;
        code[0] = 0xD65F03C0U;
        kr = mach_vm_protect(mach_task_self(), xo, sz, FALSE, VM_PROT_EXECUTE);
        CHECK(kr == KERN_SUCCESS);
        if (kr == KERN_SUCCESS) {
            r = capture_signal(fault_ldapur_off24, (uintptr_t)xo);
            CHECK(is_access_sig(r.sig) && r.addr == (uintptr_t)xo + 24U);
        }
        mach_vm_deallocate(mach_task_self(), xo, sz);
    }

    r = capture_signal(fault_ldapur_off0, (uintptr_t)0xFFFF800000001000ULL);
    CHECK(is_access_sig(r.sig) && r.addr == (uintptr_t)0xFFFF800000001000ULL);

    r = capture_signal(fault_stlur_off0, (uintptr_t)0xFFFF800000001000ULL);
    CHECK(is_access_sig(r.sig) && r.addr == (uintptr_t)0xFFFF800000001000ULL);

    r = capture_signal(fault_ldapur_neg256, 0U);
    CHECK(is_access_sig(r.sig) && r.addr == (uintptr_t)0xFFFFFFFFFFFFFF00ULL);

    r = capture_signal(fault_ldapur_off0, (uintptr_t)0x0080000000001000ULL);
    CHECK(is_access_sig(r.sig) && r.addr == (uintptr_t)0x0080000000001000ULL);

    r = capture_signal(fault_ldapur_off0, (uintptr_t)0x5A80000000001000ULL);
    CHECK(is_access_sig(r.sig) && r.addr == (uintptr_t)0x5A80000000001000ULL);

    r = capture_signal(fault_misaligned_sp, 0U);
    CHECK(r.sig == SIGBUS);

    for (uintptr_t i = 0; i < 3U; i++) {
        r = capture_signal(fault_unalloc_lrcpc2, i);
        CHECK(r.sig == SIGILL);
    }
}

static uint32_t
exec_sb(uint32_t nzcv)
{
    uint64_t in = (uint64_t)nzcv << 28;
    uint64_t out = 0;
    __asm__ volatile(
        "msr nzcv, %1\n"
        ".inst 0xD50330FF\n"
        "mrs %0, nzcv\n"
        : "=r"(out) : "r"(in) : "cc", "memory"
    );
    return (uint32_t)((out >> 28) & 0xFU);
}

static uint32_t
exec_cfinv(uint32_t nzcv)
{
    uint64_t in = (uint64_t)nzcv << 28;
    uint64_t out = 0;
    __asm__ volatile(
        "msr nzcv, %1\n"
        ".inst 0xD500401F\n"
        "mrs %0, nzcv\n"
        : "=r"(out) : "r"(in) : "cc"
    );
    return (uint32_t)((out >> 28) & 0xFU);
}

static uint32_t
exec_axflag(uint32_t nzcv)
{
    uint64_t in = (uint64_t)nzcv << 28;
    uint64_t out = 0;
    __asm__ volatile(
        "msr nzcv, %1\n"
        ".inst 0xD500405F\n"
        "mrs %0, nzcv\n"
        : "=r"(out) : "r"(in) : "cc"
    );
    return (uint32_t)((out >> 28) & 0xFU);
}

static uint32_t
exec_xaflag(uint32_t nzcv)
{
    uint64_t in = (uint64_t)nzcv << 28;
    uint64_t out = 0;
    __asm__ volatile(
        "msr nzcv, %1\n"
        ".inst 0xD500403F\n"
        "mrs %0, nzcv\n"
        : "=r"(out) : "r"(in) : "cc"
    );
    return (uint32_t)((out >> 28) & 0xFU);
}

#define EXEC_RMIF(nzcv, val64, lsh, mask) ({ \
        uint64_t in_ = (uint64_t)(nzcv) << 28; \
        uint64_t out_ = 0; \
        register uint64_t r_val __asm__("x9") = (uint64_t)(val64); \
        __asm__ volatile( \
            "msr nzcv, %1\n" \
            ".inst %c[insn]\n" \
            "mrs %0, nzcv\n" \
            : "=r"(out_) \
            : "r"(in_), "r"(r_val), \
              [insn] "i"(ENC_RMIF(9, lsh, mask)) \
            : "cc"); \
        (uint32_t)((out_ >> 28) & 0xFU); \
    })

static uint32_t
exec_rmif_xzr(uint32_t nzcv)
{
    uint64_t in = (uint64_t)nzcv << 28;
    uint64_t out = 0;
    __asm__ volatile(
        "msr nzcv, %1\n"
        ".inst %c[insn]\n"
        "mrs %0, nzcv\n"
        : "=r"(out)
        : "r"(in), [insn] "i"(ENC_RMIF(31, 7, 0xA))
        : "cc"
    );
    return (uint32_t)((out >> 28) & 0xFU);
}

static uint32_t
exec_rmif_fp(uint32_t nzcv, uint64_t val)
{
    register uint64_t r_in __asm__("x9") = (uint64_t)nzcv << 28;
    register uint64_t r_val __asm__("x10") = val;
    register uint64_t r_out __asm__("x12") = 0;
    __asm__ volatile(
        "mov x11, x29\n"
        "mov x29, %[val]\n"
        "msr nzcv, %[in]\n"
        ".inst %c[insn]\n"
        "mrs %[out], nzcv\n"
        "mov x29, x11\n"
        : [out] "=&r"(r_out)
        : [in] "r"(r_in), [val] "r"(r_val),
          [insn] "i"(ENC_RMIF(29, 7, 0xA))
        : "x11", "cc"
    );
    return (uint32_t)((r_out >> 28) & 0xFU);
}

static uint32_t
exec_setf8(uint32_t nzcv, uint32_t val)
{
    uint64_t in = (uint64_t)nzcv << 28;
    uint64_t out = 0;
    register uint32_t r_val __asm__("w9") = val;
    __asm__ volatile(
        "msr nzcv, %1\n"
        ".inst %c[insn]\n"
        "mrs %0, nzcv\n"
        : "=r"(out)
        : "r"(in), "r"(r_val), [insn] "i"(ENC_SETF8(9))
        : "cc"
    );
    return (uint32_t)((out >> 28) & 0xFU);
}

static uint32_t
exec_setf8_wzr(uint32_t nzcv)
{
    uint64_t in = (uint64_t)nzcv << 28;
    uint64_t out = 0;
    __asm__ volatile(
        "msr nzcv, %1\n"
        ".inst %c[insn]\n"
        "mrs %0, nzcv\n"
        : "=r"(out)
        : "r"(in), [insn] "i"(ENC_SETF8(31))
        : "cc"
    );
    return (uint32_t)((out >> 28) & 0xFU);
}

static uint32_t
exec_setf16(uint32_t nzcv, uint32_t val)
{
    uint64_t in = (uint64_t)nzcv << 28;
    uint64_t out = 0;
    register uint32_t r_val __asm__("w9") = val;
    __asm__ volatile(
        "msr nzcv, %1\n"
        ".inst %c[insn]\n"
        "mrs %0, nzcv\n"
        : "=r"(out)
        : "r"(in), "r"(r_val), [insn] "i"(ENC_SETF16(9))
        : "cc"
    );
    return (uint32_t)((out >> 28) & 0xFU);
}

static uint32_t
exec_setf16_wzr(uint32_t nzcv)
{
    uint64_t in = (uint64_t)nzcv << 28;
    uint64_t out = 0;
    __asm__ volatile(
        "msr nzcv, %1\n"
        ".inst %c[insn]\n"
        "mrs %0, nzcv\n"
        : "=r"(out)
        : "r"(in), [insn] "i"(ENC_SETF16(31))
        : "cc"
    );
    return (uint32_t)((out >> 28) & 0xFU);
}

static uint32_t
ref_rmif(uint32_t nzcv, uint64_t val, uint32_t lsh, uint32_t mask)
{
    uint32_t r = (uint32_t)((lsh ? ((val >> lsh) | (val << (64U - lsh))) : val) & 0xFU);
    return (nzcv & ~mask) | (r & mask);
}

static uint32_t
ref_setf(uint32_t nzcv, uint32_t val, int is16)
{
    uint32_t msb = is16 ? 15U : 7U;
    uint32_t wmask = is16 ? 0xFFFFU : 0xFFU;
    uint32_t n = (val >> msb) & 1U;
    uint32_t z = (val & wmask) == 0U ? 1U : 0U;
    uint32_t c = (nzcv >> 1) & 1U;
    uint32_t v = ((val >> (msb + 1U)) ^ (val >> msb)) & 1U;
    return (n << 3) | (z << 2) | (c << 1) | v;
}

static void
check_flagm_and_sb(void)
{
    static const uint32_t ax_expected[16] = {
        0x0, 0x4, 0x2, 0x4, 0x4, 0x4, 0x6, 0x4,
        0x0, 0x4, 0x2, 0x4, 0x4, 0x4, 0x6, 0x4
    };
    static const uint32_t xa_expected[16] = {
        0x8, 0x8, 0x2, 0x2, 0x3, 0x3, 0x6, 0x6,
        0x8, 0x8, 0x2, 0x2, 0x3, 0x3, 0x6, 0x6
    };

    for (uint32_t f = 0; f < 16U; f++) {
        CHECK(exec_sb(f) == f);
        CHECK(exec_cfinv(f) == (f ^ 2U));
        CHECK(exec_axflag(f) == ax_expected[f]);
        CHECK(exec_xaflag(f) == xa_expected[f]);
        CHECK(exec_rmif_xzr(f) == (f & ~0xAU));
        CHECK(exec_rmif_fp(f, 0xDEADBEEFCAFEBABEULL) == ref_rmif(f, 0xDEADBEEFCAFEBABEULL, 7U, 0xAU));
        CHECK(exec_setf8_wzr(f) == (0x4U | (f & 2U)));
        CHECK(exec_setf16_wzr(f) == (0x4U | (f & 2U)));
    }

    static const uint64_t rvals[] = {
        0x0ULL,
        0x1ULL,
        0x8000000000000003ULL,
        0xDEADBEEFCAFEBABEULL,
        0x5555555555555555ULL,
        ~0ULL
    };
    for (uint32_t f = 0; f < 16U; f++) {
        for (size_t i = 0; i < sizeof(rvals) / sizeof(rvals[0]); i++) {
            uint64_t v = rvals[i];
            CHECK(EXEC_RMIF(f, v, 0, 0x0) == ref_rmif(f, v, 0U, 0x0U));
            CHECK(EXEC_RMIF(f, v, 0, 0xF) == ref_rmif(f, v, 0U, 0xFU));
            CHECK(EXEC_RMIF(f, v, 7, 0xA) == ref_rmif(f, v, 7U, 0xAU));
            CHECK(EXEC_RMIF(f, v, 62, 0x5) == ref_rmif(f, v, 62U, 0x5U));
            CHECK(EXEC_RMIF(f, v, 63, 0xF) == ref_rmif(f, v, 63U, 0xFU));
        }
    }

    static const uint32_t svals[] = {
        0x00000000U, 0x00000001U, 0x0000007FU, 0x00000080U,
        0x000000FFU, 0x00000100U, 0x0000017FU, 0x00000180U,
        0x000001FFU, 0x00007FFFU, 0x00008000U, 0x0000FFFFU,
        0x00010000U, 0x00017FFFU, 0x00018000U, 0x0001FFFFU,
        0xFFFFFFFFU
    };
    for (uint32_t f = 0; f < 16U; f++) {
        for (size_t i = 0; i < sizeof(svals) / sizeof(svals[0]); i++) {
            CHECK(exec_setf8(f, svals[i]) == ref_setf(f, svals[i], 0));
            CHECK(exec_setf16(f, svals[i]) == ref_setf(f, svals[i], 1));
        }
    }
}

int
main(void)
{
    check_lrcpc2_all_variants();
    check_lrcpc2_sp_fp_lr();
    check_lrcpc2_faults();
    check_flagm_and_sb();
#if __arm64e__
    return checks_done("isa arm64e");
#else
    return checks_done("isa arm64");
#endif
}
