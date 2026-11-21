#ifndef E32_DOOM_SYS_TIME_H
#define E32_DOOM_SYS_TIME_H

typedef long suseconds_t;
struct timeval { long tv_sec; long tv_usec; };
struct timezone { int tz_minuteswest; int tz_dsttime; };
int gettimeofday(struct timeval *tv, void *tz);

#endif /* E32_DOOM_SYS_TIME_H */
