/* Minimal freestanding <time.h> for the Lua 5.4 E32 port. */
#ifndef QPORT_LUA_TIME_H
#define QPORT_LUA_TIME_H
#include <stddef.h>
typedef int time_t;
typedef int clock_t;
#define CLOCKS_PER_SEC 1000000
time_t time(time_t *t);
clock_t clock(void);
#endif
