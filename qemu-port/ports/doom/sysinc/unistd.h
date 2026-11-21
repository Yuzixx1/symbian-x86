/* Freestanding <unistd.h> shim: only declarations the core files mention. */
#ifndef E32_DOOM_UNISTD_H
#define E32_DOOM_UNISTD_H

#include <stddef.h>

int access(const char *pathname, int mode);
int close(int fd);
int getpid(void);
long lseek(int fd, long offset, int whence);
int mkdir(const char *pathname, unsigned mode);
int unlink(const char *pathname);
int rmdir(const char *pathname);
unsigned sleep(unsigned seconds);
int usleep(unsigned usec);
long sysconf(int name);
char *getcwd(char *buf, size_t size);

#define R_OK 4
#define W_OK 2
#define X_OK 1
#define F_OK 0
#define _SC_PAGESIZE 1

#endif /* E32_DOOM_UNISTD_H */
