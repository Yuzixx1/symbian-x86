/* Minimal freestanding <errno.h> for the Lua 5.4 E32 port. */
#ifndef QPORT_LUA_ERRNO_H
#define QPORT_LUA_ERRNO_H
extern int errno;
#define EDOM    33
#define ERANGE  34
#define EINVAL  22
#define ENOENT  2
#define EACCES  13
#endif
