/* Minimal freestanding <math.h> for the Lua 5.4 E32 port.
 * Implemented with x87 instructions in e32_shim.c (no hosted libm).
 */
#ifndef QPORT_LUA_MATH_H
#define QPORT_LUA_MATH_H

#ifndef HUGE_VAL
#define HUGE_VAL    (__builtin_huge_val())
#endif
#ifndef INFINITY
#define INFINITY    (__builtin_inff())
#endif
#ifndef NAN
#define NAN         (__builtin_nanf(""))
#endif

double floor(double x);
double ceil(double x);
double fabs(double x);
double fmod(double x, double y);
double sqrt(double x);
double pow(double x, double y);
double ldexp(double x, int e);
double frexp(double x, int *e);
double sin(double x);
double cos(double x);
double tan(double x);
double exp(double x);
double log(double x);
double log10(double x);
double atan(double x);
double atan2(double y, double x);
double asin(double x);
double acos(double x);

#endif /* QPORT_LUA_MATH_H */
