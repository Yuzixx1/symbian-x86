#ifndef R1_G_LAUNCH_H
#define R1_G_LAUNCH_H
// Serialized shell/kernel-thread entry. Commands: check, heap, sched, reap,
// abandon, reject-manifest, reject-import. No implicit fallback or user IPC.
TInt R1GCommand(const char* mode);
#endif
