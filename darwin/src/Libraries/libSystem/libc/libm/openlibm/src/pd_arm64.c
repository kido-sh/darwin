#include <stdint.h>

/* ARM64 uses IEEE double for C long double. */

double
nanl(const char *tagp)
{
    (void)tagp;
    return __builtin_nan("");
}

extern double nextafter(double, double);
extern float nextafterf(float, float);

double
nexttoward(double x, long double y)
{
    return nextafter(x, (double)y);
}

float
nexttowardf(float x, long double y)
{
    if (__builtin_isnan(x) || __builtin_isnan(y))
        return (float)(x + y);
    if ((long double)x == y)
        return (float)y;
    return nextafterf(x, x < y ? __builtin_inff() : -__builtin_inff());
}
