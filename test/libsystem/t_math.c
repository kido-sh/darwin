/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fenv.h>
#include <float.h>
#include <math.h>
#include "check.h"

#pragma STDC FENV_ACCESS ON

static volatile double zero = 0.0, one = 1.0, tiny = 1e-30;

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
    return checks_done("math");
}
