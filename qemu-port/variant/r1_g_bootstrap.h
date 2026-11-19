#ifndef R1_G_BOOTSTRAP_H
#define R1_G_BOOTSTRAP_H
#include "r1_g_objects.h"
namespace R1Kernel {
struct ManifestEntry {
    R1G::ImageName absoluteName;
    E32G::Bytes bytes;
};
// All views are pre-authenticated, immutable kernel-owned staging. Never accept
// writable disk/user pointers as a manifest. Storage must outlive this call.
struct BootRequest {
    const ManifestEntry* manifest; TUint count, root;
    R1G::ImageName arguments; // arguments ONLY, UTF-16, excludes executable name
    R1G::ImageId required; TUint requiredSid, minStack;
};
struct BootResult {
    TInt handle; // EOwnerThread handle in launching kernel thread, NOT user client
    DProcess* process; // borrowed while handle remains open; suspended
    R1G::Diagnostic diagnostic;
};
// Thread context, interrupts enabled, kernel unlocked, no fast/code mutex held.
// Enters/leaves CS internally. Serialized bootstrap calls only. Heap storage.
// Does not set up TSS, change effective security flags, Resume or run diagnostics.
TInt LoadSuspended(const BootRequest&,BootResult&);
} // namespace R1Kernel
#endif
