// shell.h — interactive EKA2> shell thread (Phase 6).
#ifndef __QEMU_SHELL_H__
#define __QEMU_SHELL_H__

namespace Shell
    {
    void SpawnAndEnter();   // registers shell+idle threads, switches in (no return)
    }

#endif
