/* Freestanding <fcntl.h> shim: flags referenced by doomgeneric's input code. */
#ifndef E32_DOOM_FCNTL_H
#define E32_DOOM_FCNTL_H

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2
#define O_CREAT  0100
#define O_TRUNC  01000
#define O_APPEND 02000
#define O_NONBLOCK 04000
#define O_BINARY 0
#define O_TEXT 0

int open(const char *pathname, int flags, ...);
int fcntl(int fd, int cmd, ...);

#endif /* E32_DOOM_FCNTL_H */
