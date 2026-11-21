/* Freestanding <errno.h> shim. */
#ifndef E32_DOOM_ERRNO_H
#define E32_DOOM_ERRNO_H

extern int errno;

#define EPERM   1
#define ENOENT  2
#define EIO     5
#define ENOMEM  12
#define EACCES  13
#define EEXIST  17
#define ENODEV  19
#define EINVAL  22
#define ENOSPC  28
#define EISDIR  21
#define ERANGE  34
#define EAGAIN  11
#define ENOTDIR 20
#define ETIMEDOUT 60

#endif /* E32_DOOM_ERRNO_H */
