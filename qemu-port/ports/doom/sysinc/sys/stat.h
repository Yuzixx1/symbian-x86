#ifndef E32_DOOM_SYS_STAT_H
#define E32_DOOM_SYS_STAT_H

#include <stddef.h>

struct stat {
    unsigned st_mode;
    long st_size;
    long st_mtime;
};
int stat(const char *path, struct stat *buf);
int mkdir(const char *pathname, unsigned mode);

#define S_IFDIR 0040000
#define S_IFREG 0100000
#define S_ISDIR(m) (((m) & 0170000) == S_IFDIR)

#endif /* E32_DOOM_SYS_STAT_H */
