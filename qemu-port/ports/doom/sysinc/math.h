/* Freestanding <math.h> shim. DOOM's renderer is fixed-point; the only live
 * floating-point use is fabs() in the (disabled) mouse acceleration path.
 * Implementations live in e32_shim.c and avoid libm entirely. */
#ifndef E32_DOOM_MATH_H
#define E32_DOOM_MATH_H

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern int math_errhandling;

double fabs(double x);
float fabsf(float x);
double sin(double x);
float sinf(float x);
double cos(double x);
float cosf(float x);
double tan(double x);
double sqrt(double x);
double floor(double x);
double ceil(double x);
double pow(double x, double y);
double atan2(double y, double x);
double fmod(double x, double y);
double log(double x);
double exp(double x);

#endif /* E32_DOOM_MATH_H */
