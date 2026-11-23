/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The floating-point environment with the arm64 ABI of macOS's <fenv.h>: the exception flags
 * are FPSR bits, the rounding mode is the RMode field of FPCR, and fenv_t holds both registers.
 * The SDK's openlibm header is a different ABI, so it is not included here.
 */

#include <stdint.h>

typedef struct {
    unsigned long long __fpsr;
    unsigned long long __fpcr;
} kido_fenv_t;

typedef unsigned short kido_fexcept_t;

#define KIDO_FE_OVERFLOW 0x0004
#define KIDO_FE_UNDERFLOW 0x0008
#define KIDO_FE_INEXACT 0x0010
#define KIDO_FE_ALL_EXCEPT 0x009f
#define KIDO_FE_ROUND_MASK 0x00c00000
#define KIDO_FPCR_TRAP_MASK 0x00009f00
#define KIDO_FPCR_FLUSH_TO_ZERO 0x01000000

const kido_fenv_t _FE_DFL_ENV = { 0, 0 };
const kido_fenv_t _FE_DFL_DISABLE_DENORMS_ENV = { 0, KIDO_FPCR_FLUSH_TO_ZERO };

static inline uint64_t
read_fpsr(void)
{
    uint64_t fpsr;

    __asm__ volatile("mrs %0, fpsr" : "=r"(fpsr));
    return fpsr;
}

static inline void
write_fpsr(uint64_t fpsr)
{
    __asm__ volatile("msr fpsr, %0" : : "r"(fpsr));
}

static inline uint64_t
read_fpcr(void)
{
    uint64_t fpcr;

    __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
    return fpcr;
}

static inline void
write_fpcr(uint64_t fpcr)
{
    __asm__ volatile("msr fpcr, %0" : : "r"(fpcr));
}

int
feclearexcept(int excepts)
{
    write_fpsr(read_fpsr() & ~(uint64_t)(excepts & KIDO_FE_ALL_EXCEPT));
    return 0;
}

int
fegetexceptflag(kido_fexcept_t *flagp, int excepts)
{
    *flagp = (kido_fexcept_t)(read_fpsr() & (uint64_t)(excepts & KIDO_FE_ALL_EXCEPT));
    return 0;
}

int
fesetexceptflag(const kido_fexcept_t *flagp, int excepts)
{
    uint64_t mask = (uint64_t)(excepts & KIDO_FE_ALL_EXCEPT);

    write_fpsr((read_fpsr() & ~mask) | (*flagp & mask));
    return 0;
}

int
feraiseexcept(int excepts)
{
    if (excepts & (KIDO_FE_OVERFLOW | KIDO_FE_UNDERFLOW))
        excepts |= KIDO_FE_INEXACT;
    write_fpsr(read_fpsr() | (uint64_t)(excepts & KIDO_FE_ALL_EXCEPT));
    return 0;
}

int
fetestexcept(int excepts)
{
    return (int)(read_fpsr() & (uint64_t)(excepts & KIDO_FE_ALL_EXCEPT));
}

int
fegetround(void)
{
    return (int)(read_fpcr() & KIDO_FE_ROUND_MASK);
}

int
fesetround(int round)
{
    if (round & ~KIDO_FE_ROUND_MASK)
        return round & ~KIDO_FE_ROUND_MASK;
    write_fpcr((read_fpcr() & ~(uint64_t)KIDO_FE_ROUND_MASK) | (uint64_t)round);
    return 0;
}

int
fegetenv(kido_fenv_t *envp)
{
    envp->__fpsr = read_fpsr();
    envp->__fpcr = read_fpcr();
    return 0;
}

int
feholdexcept(kido_fenv_t *envp)
{
    fegetenv(envp);
    write_fpsr(envp->__fpsr & ~(uint64_t)KIDO_FE_ALL_EXCEPT);
    write_fpcr(envp->__fpcr & ~(uint64_t)KIDO_FPCR_TRAP_MASK);
    return 0;
}

int
fesetenv(const kido_fenv_t *envp)
{
    write_fpsr(envp->__fpsr);
    write_fpcr(envp->__fpcr);
    return 0;
}

int
feupdateenv(const kido_fenv_t *envp)
{
    int raised = (int)(read_fpsr() & KIDO_FE_ALL_EXCEPT);

    fesetenv(envp);
    return feraiseexcept(raised);
}
