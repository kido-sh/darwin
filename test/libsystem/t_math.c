/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <complex.h>
#include <fenv.h>
#include <float.h>
#include <math.h>
#include "check.h"

#pragma STDC FENV_ACCESS ON

static volatile double zero = 0.0, one = 1.0, tiny = 1e-30;

extern int __isnanl(long double);
extern int __isinfl(long double);
extern int __isfinitel(long double);
extern int __isnormall(long double);
extern int __signbitl(long double);
extern int __isnand(double);
extern int __isinfd(double);
extern int __isfinited(double);
extern int __isnormald(double);
extern int __signbitd(double);
extern int __isnanf(float);
extern int __isinff(float);
extern int __isfinitef(float);
extern int __isnormalf(float);
extern int __signbitf(float);

__attribute__((noinline))
static void
scrub_stack(void)
{
    volatile unsigned char junk[256];

    for (size_t i = 0; i < sizeof(junk); i++)
        junk[i] = 0xaa;
}

static void
check_ld_classify(long double v, int exp_class, int exp_nan, int exp_inf,
    int exp_finite, int exp_normal, int exp_sign)
{
    volatile long double x = v;

    CHECK(fpclassify(x) == exp_class);
    CHECK(!!isnan(x) == exp_nan);
    CHECK(!!isinf(x) == exp_inf);
    CHECK(!!isfinite(x) == exp_finite);
    CHECK(!!isnormal(x) == exp_normal);
    if (exp_sign >= 0)
        CHECK(!!signbit(x) == exp_sign);

    scrub_stack();
    CHECK(__fpclassifyl(x) == exp_class);
    scrub_stack();
    CHECK(!!__isnanl(x) == exp_nan);
    scrub_stack();
    CHECK(!!__isinfl(x) == exp_inf);
    scrub_stack();
    CHECK(!!__isfinitel(x) == exp_finite);
    if (exp_class != FP_SUBNORMAL) {
        scrub_stack();
        CHECK(!!__isnormall(x) == exp_normal);
    }
    if (exp_sign >= 0) {
        scrub_stack();
        CHECK(!!__signbitl(x) == exp_sign);
    }
}

static void
test_long_double_math(void)
{
    long double (*volatile p_sqrtl)(long double) = sqrtl;
    long double (*volatile p_powl)(long double, long double) = powl;
    long double (*volatile p_sinl)(long double) = sinl;
    long double (*volatile p_cosl)(long double) = cosl;
    long double (*volatile p_atan2l)(long double, long double) = atan2l;
    long double (*volatile p_expl)(long double) = expl;
    long double (*volatile p_logl)(long double) = logl;
    long double (*volatile p_log2l)(long double) = log2l;
    long double (*volatile p_hypotl)(long double, long double) = hypotl;
    long double (*volatile p_fmal)(long double, long double, long double) = fmal;
    long double (*volatile p_fabsl)(long double) = fabsl;
    long double (*volatile p_floorl)(long double) = floorl;
    long double (*volatile p_ceill)(long double) = ceill;
    long double (*volatile p_roundl)(long double) = roundl;
    long double (*volatile p_fmodl)(long double, long double) = fmodl;
    long double (*volatile p_scalbnl)(long double, int) = scalbnl;
    long double (*volatile p_scalblnl)(long double, long) = scalblnl;
    long double (*volatile p_nexttowardl)(long double, long double) = nexttowardl;
    long double (*volatile p_cabsl)(long double complex) = cabsl;
    long double complex (*volatile p_csqrtl)(long double complex) = csqrtl;
    long double complex (*volatile p_cexpl)(long double complex) = cexpl;
    volatile long double a = 3.0L, b = 4.0L, pi = (long double)M_PI;
    long double complex z = a + b * I;
    long double complex r;

    CHECK(p_sqrtl(9.0L) == 3.0L);
    CHECK(p_powl(2.0L, 10.0L) == 1024.0L);
    CHECK(p_sinl(0.0L) == 0.0L);
    CHECK(p_fabsl(p_sinl((long double)M_PI_2) - 1.0L) < 1e-12L);
    CHECK(p_cosl(0.0L) == 1.0L);
    CHECK(p_fabsl(p_cosl(pi) + 1.0L) < 1e-12L);
    CHECK(p_fabsl(p_atan2l(1.0L, 1.0L) - (long double)M_PI_4) < 1e-12L);
    CHECK(p_fabsl(p_expl(1.0L) - (long double)M_E) < 1e-12L);
    CHECK(p_fabsl(p_logl((long double)M_E) - 1.0L) < 1e-12L);
    CHECK(p_log2l(1024.0L) == 10.0L);
    CHECK(p_hypotl(a, b) == 5.0L);
    CHECK(p_fmal(a, b, 5.0L) == 17.0L);
    CHECK(p_fabsl(-3.5L) == 3.5L);
    CHECK(p_floorl(2.75L) == 2.0L && p_floorl(-2.25L) == -3.0L);
    CHECK(p_ceill(2.25L) == 3.0L && p_ceill(-2.75L) == -2.0L);
    CHECK(p_roundl(2.5L) == 3.0L && p_roundl(-2.5L) == -3.0L);
    CHECK(p_fmodl(7.5L, 2.0L) == 1.5L);
    CHECK(p_scalbnl(1.5L, 3) == 12.0L);
    CHECK(p_scalblnl(1.5L, 4L) == 24.0L);
    CHECK(isinf(p_scalblnl(1.0L, 1L << 32)) && p_scalblnl(1.0L, 1L << 32) > 0);
    CHECK(p_scalblnl(1.0L, -(1L << 32)) == 0.0L);
    CHECK(p_nexttowardl(1.0L, 2.0L) == (long double)nextafter(1.0, 2.0));
    CHECK(p_nexttowardl(1.0L, 0.0L) == (long double)nextafter(1.0, 0.0));
    CHECK(p_nexttowardl(1.0L, 1.0L) == 1.0L);
    CHECK(p_cabsl(z) == 5.0L);
    r = p_csqrtl(z);
    CHECK(p_fabsl(creall(r) - 2.0L) < 1e-12L && p_fabsl(cimagl(r) - 1.0L) < 1e-12L);
    r = p_cexpl(0.0L + pi * I);
    CHECK(p_fabsl(creall(r) + 1.0L) < 1e-12L && p_fabsl(cimagl(r)) < 1e-12L);
}

static void
test_long_double_classify(void)
{
    volatile double d_norm = 1.5, d_negz = -0.0, d_nan = NAN, d_inf = INFINITY;
    volatile float f_norm = 1.5f, f_negz = -0.0f, f_nan = NAN, f_inf = INFINITY;

    check_ld_classify(1.5L, FP_NORMAL, 0, 0, 1, 1, 0);
    check_ld_classify(-1.5L, FP_NORMAL, 0, 0, 1, 1, 1);
    check_ld_classify(0.0L, FP_ZERO, 0, 0, 1, 0, 0);
    check_ld_classify(-0.0L, FP_ZERO, 0, 0, 1, 0, 1);
    check_ld_classify((long double)NAN, FP_NAN, 1, 0, 0, 0, -1);
    check_ld_classify((long double)INFINITY, FP_INFINITE, 0, 1, 0, 0, 0);
    check_ld_classify(-(long double)INFINITY, FP_INFINITE, 0, 1, 0, 0, 1);
    check_ld_classify(LDBL_TRUE_MIN, FP_SUBNORMAL, 0, 0, 1, 0, 0);
    check_ld_classify(-LDBL_TRUE_MIN, FP_SUBNORMAL, 0, 0, 1, 0, 1);

    CHECK(__isfinited(d_norm) != 0 && __isfinited(d_inf) == 0);
    CHECK(__isinfd(d_inf) != 0 && __isinfd(d_norm) == 0);
    CHECK(__isnand(d_nan) != 0 && __isnand(d_norm) == 0);
    CHECK(__isnormald(d_norm) != 0 && __isnormald(d_negz) == 0);
    CHECK(__signbitd(d_negz) != 0 && __signbitd(d_norm) == 0);

    CHECK(__isfinitef(f_norm) != 0 && __isfinitef(f_inf) == 0);
    CHECK(__isinff(f_inf) != 0 && __isinff(f_norm) == 0);
    CHECK(__isnanf(f_nan) != 0 && __isnanf(f_norm) == 0);
    CHECK(__isnormalf(f_norm) != 0 && __isnormalf(f_negz) == 0);
    CHECK(__signbitf(f_negz) != 0 && __signbitf(f_norm) == 0);
}

int
main(void)
{
    fenv_t env, held;
    fexcept_t flags;
    volatile double r;

    CHECK(nexttoward(1.0, 2.0L) == nextafter(1.0, 2.0));
    CHECK(nexttoward(1.0, 0.0L) == nextafter(1.0, 0.0));
    CHECK(nexttoward(1.0, 1.0L) == 1.0);
    CHECK(nexttoward(0.0, 1.0L) == DBL_TRUE_MIN);
    CHECK(nexttoward(-0.0, -1.0L) == -DBL_TRUE_MIN);
    CHECK(isnan(nexttoward(NAN, 1.0L)));
    CHECK(nexttoward(DBL_MAX, INFINITY) == INFINITY);
    CHECK(nexttowardf(1.0f, 1.0L + 1e-10L) == nextafterf(1.0f, 2.0f));
    CHECK(nexttowardf(1.0f, 1.0L - 1e-10L) == nextafterf(1.0f, 0.0f));
    CHECK(nexttowardf(1.0f, 1.0L) == 1.0f);
    CHECK(nexttowardf(FLT_MAX, (long double)DBL_MAX) == INFINITY);

    CHECK(feclearexcept(FE_ALL_EXCEPT) == 0);
    CHECK(fetestexcept(FE_ALL_EXCEPT) == 0);
    CHECK(feraiseexcept(FE_DIVBYZERO | FE_INEXACT) == 0);
    CHECK(fetestexcept(FE_ALL_EXCEPT) == (FE_DIVBYZERO | FE_INEXACT));
    CHECK(feclearexcept(FE_DIVBYZERO) == 0);
    CHECK(fetestexcept(FE_ALL_EXCEPT) == FE_INEXACT);
    CHECK(feraiseexcept(FE_FLUSHTOZERO) == 0 && fetestexcept(FE_FLUSHTOZERO) == FE_FLUSHTOZERO);
    feclearexcept(FE_ALL_EXCEPT);

    r = one / zero;
    CHECK(fetestexcept(FE_ALL_EXCEPT) == FE_DIVBYZERO);
    feclearexcept(FE_ALL_EXCEPT);
    r = sqrt(-one);
    CHECK(fetestexcept(FE_INVALID) == FE_INVALID);
    feclearexcept(FE_ALL_EXCEPT);
    r = DBL_MAX * (one + one);
    CHECK(fetestexcept(FE_ALL_EXCEPT) == (FE_OVERFLOW | FE_INEXACT));
    feclearexcept(FE_ALL_EXCEPT);
    r = DBL_MIN / (one + one + one);
    CHECK(fetestexcept(FE_UNDERFLOW | FE_INEXACT) == (FE_UNDERFLOW | FE_INEXACT));

    CHECK(fegetexceptflag(&flags, FE_ALL_EXCEPT) == 0);
    CHECK(flags == (FE_UNDERFLOW | FE_INEXACT));
    feclearexcept(FE_ALL_EXCEPT);
    CHECK(fesetexceptflag(&flags, FE_UNDERFLOW) == 0);
    CHECK(fetestexcept(FE_ALL_EXCEPT) == FE_UNDERFLOW);
    feclearexcept(FE_ALL_EXCEPT);

    CHECK(fegetround() == FE_TONEAREST);
    CHECK(fesetround(FE_UPWARD) == 0 && fegetround() == FE_UPWARD);
    r = one + tiny;
    CHECK(r > 1.0);
    CHECK(fesetround(FE_DOWNWARD) == 0 && fegetround() == FE_DOWNWARD);
    r = one - tiny;
    CHECK(r < 1.0);
    CHECK(fesetround(FE_TOWARDZERO) == 0 && fegetround() == FE_TOWARDZERO);
    CHECK(fesetround(0x12345) == 0x12345 && fegetround() == FE_TOWARDZERO);
    CHECK(fesetround(FE_TONEAREST) == 0);
    r = one + tiny;
    CHECK(r == 1.0);

    feclearexcept(FE_ALL_EXCEPT);
    CHECK(feraiseexcept(FE_OVERFLOW) == 0 && fetestexcept(FE_ALL_EXCEPT) == (FE_OVERFLOW | FE_INEXACT));
    feclearexcept(FE_ALL_EXCEPT);
    CHECK(feraiseexcept(FE_UNDERFLOW) == 0 && fetestexcept(FE_ALL_EXCEPT) == (FE_UNDERFLOW | FE_INEXACT));
    feclearexcept(FE_ALL_EXCEPT);
    fesetround(FE_UPWARD);
    feraiseexcept(FE_INVALID);
    CHECK(fegetenv(&env) == 0);
    CHECK(fesetenv(FE_DFL_ENV) == 0);
    CHECK(fegetround() == FE_TONEAREST && fetestexcept(FE_ALL_EXCEPT) == 0);
    CHECK(fesetenv(&env) == 0);
    CHECK(fegetround() == FE_UPWARD && fetestexcept(FE_ALL_EXCEPT) == FE_INVALID);
    CHECK(feholdexcept(&held) == 0);
    CHECK(fetestexcept(FE_ALL_EXCEPT) == 0 && fegetround() == FE_UPWARD);
    feraiseexcept(FE_DIVBYZERO);
    fesetround(FE_TONEAREST);
    CHECK(feupdateenv(&held) == 0);
    CHECK(fegetround() == FE_UPWARD && fetestexcept(FE_ALL_EXCEPT) == (FE_INVALID | FE_DIVBYZERO));

    CHECK(fesetenv(FE_DFL_DISABLE_DENORMS_ENV) == 0);
    r = DBL_MIN / (one + one + one + one);
    CHECK(r == 0.0);
    CHECK(fesetenv(FE_DFL_ENV) == 0);
    r = DBL_MIN / (one + one + one + one);
    CHECK(r == DBL_MIN / 4);
    CHECK(fegetround() == FE_TONEAREST);
    (void)r;
    test_long_double_math();
    test_long_double_classify();
    return checks_done("math");
}
