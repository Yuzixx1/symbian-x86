#ifndef R1_KERNEL_FS_CHANNEL_H
#define R1_KERNEL_FS_CHANNEL_H
class DProcess;
// Kernel-only bootstrap call. Takes a reference to the exact process object;
// no SID/name/capability may substitute for the pointer. Call once after the
// trusted host object exists, before channel open; revoke on pre-publication
// abort. Registered host death is a whole-system failure, not restart.
TInt R1FsChannelPin(DProcess* aProcess);
void R1FsChannelRevoke();
TInt R1FsChannelInstall(); // after UserFilesInit, before host resume
#endif
