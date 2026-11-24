/*
 * efsrv_min.cpp -- I-08: genuine portfilesrv user host entry point (app_main).
 *
 * Ring-3 owner of the protected "!FileServer" name. It runs on the real
 * executive/IPC bridge (user/start.S -> app_main -> CServer2::Start receive
 * loop), not on a hosted libc. The EUSER shim (user/e32base.h) now supports
 * genuine CServer2/CSession2, so fs/r1_fs_server.cpp is linked directly:
 * FileServer::Publish() creates and drives the real CServer2 framework.
 *
 * Bootstrap phases (docs/r1_service_boundary_v1.md, services/r1_service_host.*):
 *   B1/B0 -- a trusted image is materialized and pinned by the kernel launcher.
 *   B2    -- the FS owner thread registers once: UserSvr::FsRegisterThread() is
 *           the SDK spelling (EUSER export 1953); on the freestanding exec
 *           bridge the same kernel action is EExecFsRegisterThread = 81
 *           (see build/export/exec_enum.h). NOTE: 1221 is the EUSER _memcpy
 *           export, NOT FsRegisterThread. exec0(81) is the genuine call.
 *   B3    -- "!FileServer" is published by CServer2::Start (via FileServer::
 *           Publish). CServer2 owns the accept/dispatch/complete loop; this
 *           file only wires the R1 backend into FileServer::NewL.
 *
 * C++98 / -ffreestanding -fno-exceptions -fno-rtti. No libstdc++ needed.
 */

/* The freestanding runtime/ipc bridge is C; include with C linkage so
   app_main (called by start.S _start) keeps its unmangled symbol. */
extern "C" {
#include "runtime.h"
#include "ipc.h"
}
#include "../fs/r1_fs_core.h"
#include "e32base.h"
#include "../fs/r1_fs_server.h"

/* Genuine kernel executive number (build/export/exec_enum.h). */
enum { EExecFsRegisterThread = 81 }; /* ExecHandler::FsRegisterThread */
extern "C" int exec0(unsigned number);

/* Freestanding global operator delete backed by the port runtime's malloc/free
   (no libstdc++ is linked into E32 user images). */
void operator delete(void* p) noexcept { free(p); }
void operator delete(void* p, unsigned) noexcept { free(p); }
/* Placement new: construct the FS engine into static storage inside app_main
   so the image has no global constructors / .init_array (elf2e32 rejects it). */
void* operator new(unsigned, void* where) noexcept { return where; }

/*--------------------------------------------------------------------------*/
/* B5 disk-backed backend for the ring-3 server.                              */
/*                                                                            */
/* This is a PORT-SIDE BRIDGE, not Symbian logic. It implements the private   */
/* R1Fs::Backend contract (fs/r1_fs_backend.h) on top of the port runtime's    */
/* file_*() APIs (runtime.c). Those issue logical-channel Requests to the      */
/* kernel's PortFile / PortBigFile devices (variant/userfiles.cpp,             */
/* variant/usercon.cpp), which walk the REAL FAT32 cluster chain of the C:     */
/* volume on our disk image via AtaRead/AtaWrite. Genuine F32 (sfat32) is      */
/* compiled and its BPB parsing is verified in-guest by F32PROBE; the full     */
/* sfat32 I/O stack cannot be linked into an E32 here (PE2 16-import-slot      */
/* ceiling -- see genuine/F32_EFSRV_REPORT.md), so cluster I/O deliberately    */
/* uses the port adapter. The genuine TFsMessage opcodes, descriptor shapes,   */
/* R1Fs::Core and fs/r1_fs_server.cpp are all UNCHANGED.                       */
/*--------------------------------------------------------------------------*/
namespace {
const unsigned KBackFiles = 16;

/* Convert a UTF-16 R1Fs::Path (all-ASCII in the R1 fixture namespace) into a
   NUL-terminated 8-bit path for the runtime PortFile/PortBigFile devices. */
static int32_t ToCStr(const R1Fs::Path& p, char* out, uint32_t outSize)
{
    if (p.length >= outSize) return R1Fs::BadName;
    for (uint32_t i = 0; i < p.length; ++i) {
        if (p.text[i] > 0x7f) return R1Fs::BadName;
        out[i] = (char)p.text[i];
    }
    out[p.length] = 0;
    return R1Fs::Success;
}

class DiskFatBackend : public R1Fs::Backend {
public:
    DiskFatBackend() : iNextGeneration(1)
    {
        for (unsigned i = 0; i < KBackFiles; ++i) {
            iFiles[i].used = false;
            iFiles[i].readHandle = -1;
            iFiles[i].buf = 0;
            iFiles[i].bufCap = 0;
        }
    }

    int32_t Lookup(const R1Fs::Path& path, R1Fs::Meta& meta) {
        char name[R1Fs::KPathUnits + 1];
        int32_t rc = ToCStr(path, name, sizeof(name));
        if (rc != R1Fs::Success) return rc;
        int h = file_open(name);              /* real FAT lookup (PortBigFile) */
        if (h < 0) return R1Fs::NotFound;
        long sz = file_size(h);
        file_close(h);
        if (sz < 0) return R1Fs::General;
        meta.size = (uint64_t)sz;
        meta.identity = iNextGeneration;      /* opaque; stable while opened */
        return R1Fs::Success;
    }

    int32_t Open(const R1Fs::Path& path, R1Fs::Disposition how,
                 R1Fs::Key& key, R1Fs::Meta& meta) {
        char name[R1Fs::KPathUnits + 1];
        int32_t rc = ToCStr(path, name, sizeof(name));
        if (rc != R1Fs::Success) return rc;
        Entry* e = FreeEntry();
        if (!e) return R1Fs::NoMemory;

        if (how == R1Fs::OpenExisting) {
            /* Real on-disk open: PortBigFile resolves the path through R1Fat
               and snapshots the directory entry for cluster-chain reads. */
            int h = file_open(name);
            if (h < 0) return R1Fs::NotFound;
            long sz = file_size(h);
            if (sz < 0) { file_close(h); return R1Fs::General; }
            e->readHandle = h;
            e->size = (uint64_t)sz;
            e->write = false;
        } else { /* CreateNew / ReplaceExisting */
            /* Reject CreateNew of an existing on-disk file (probe via open). */
            int probe = file_open(name);
            if (probe >= 0) {
                file_close(probe);
                if (how == R1Fs::CreateNew) return R1Fs::AlreadyExists;
            }
            e->readHandle = -1;
            e->size = 0;
            e->write = true;
            e->buf = 0;
            e->bufCap = 0;
        }
        e->used = true;
        e->dirty = false;
        e->generation = iNextGeneration++;
        if (!e->generation) e->generation = iNextGeneration++;
        e->key.slot = (unsigned)(e - iFiles);
        e->key.generation = e->generation;
        e->path = path;
        key = e->key;
        meta.size = e->size;
        meta.identity = e->generation;
        return R1Fs::Success;
    }

    R1Fs::IoResult ReadAt(R1Fs::Key key, uint64_t offset, uint8_t* dst,
                          uint32_t capacity) {
        R1Fs::IoResult r = R1Fs::IoResult();
        r.error = R1Fs::Success; r.stable = true;
        Entry* e = FindKey(key);
        if (!e || !dst) { r.error = R1Fs::BadHandle; return r; }
        if (offset >= e->size) { r.size = e->size; return r; }
        uint32_t avail = (uint32_t)(e->size - offset);
        uint32_t n = capacity < avail ? capacity : avail;
        if (e->write) {
            /* Uncommitted write handle: read back the staged whole-file image. */
            for (uint32_t i = 0; i < n; ++i) dst[i] = e->buf[offset + i];
        } else {
            /* Committed file: read straight from the FAT cluster chain. */
            int got = file_read_at(e->readHandle, (unsigned)offset, dst, n);
            if (got < 0) { r.error = R1Fs::General; r.size = e->size; return r; }
            n = (uint32_t)got;
        }
        r.committed = n; r.size = e->size;
        return r;
    }

    R1Fs::IoResult WriteAt(R1Fs::Key key, uint64_t offset, const uint8_t* src,
                           uint32_t length) {
        R1Fs::IoResult r = R1Fs::IoResult();
        r.error = R1Fs::Success; r.stable = true;
        Entry* e = FindKey(key);
        if (!e || !src) { r.error = R1Fs::BadHandle; return r; }
        if (!e->write) { r.error = R1Fs::AccessDenied; return r; }
        if (offset > 0xffff0000u || length > 0xffff0000u - offset) {
            r.error = R1Fs::TooBig; r.size = e->size; return r;
        }
        uint64_t end = offset + length;
        if (end > e->bufCap) {
            uint32_t cap = e->bufCap ? e->bufCap : 256;
            while (cap < end) cap = cap > 0x40000000u ? (uint32_t)end : cap * 2;
            uint8_t* bigger = (uint8_t*)malloc(cap);
            if (!bigger) { r.error = R1Fs::NoMemory; r.size = e->size; return r; }
            for (uint32_t i = 0; i < e->size; ++i) bigger[i] = e->buf[i];
            free(e->buf); e->buf = bigger; e->bufCap = cap;
        }
        for (uint32_t i = 0; i < length; ++i) e->buf[(uint32_t)offset + i] = src[i];
        if (end > e->size) e->size = end;
        e->dirty = true;
        r.committed = length; r.size = e->size;
        return r;
    }

    R1Fs::IoResult Resize(R1Fs::Key key, uint64_t newSize) {
        R1Fs::IoResult r = R1Fs::IoResult();
        r.error = R1Fs::Success; r.stable = true;
        Entry* e = FindKey(key);
        if (!e) { r.error = R1Fs::BadHandle; return r; }
        if (!e->write) { r.error = R1Fs::AccessDenied; return r; }
        if (newSize > 0xffff0000u) { r.error = R1Fs::TooBig; r.size = e->size; return r; }
        if (newSize > e->bufCap) {
            uint32_t cap = e->bufCap ? e->bufCap : 256;
            while (cap < newSize) cap = cap > 0x40000000u ? (uint32_t)newSize : cap * 2;
            uint8_t* bigger = (uint8_t*)malloc(cap);
            if (!bigger) { r.error = R1Fs::NoMemory; r.size = e->size; return r; }
            for (uint32_t i = 0; i < e->size; ++i) bigger[i] = e->buf[i];
            free(e->buf); e->buf = bigger; e->bufCap = cap;
        }
        for (uint32_t i = (uint32_t)newSize; i < e->size; ++i) e->buf[i] = 0;
        e->size = newSize;
        e->dirty = true;
        r.size = e->size;
        return r;
    }

    int32_t Flush(R1Fs::Key key) {
        Entry* e = FindKey(key);
        if (!e) return R1Fs::BadHandle;
        if (e->write && e->dirty) return CommitWrite(e);
        return R1Fs::Success;
    }

    int32_t Close(R1Fs::Key key) {
        Entry* e = FindKey(key);
        if (!e) return R1Fs::BadHandle;
        int32_t r = R1Fs::Success;
        if (e->write && e->dirty) r = CommitWrite(e); /* real FAT write */
        if (e->readHandle >= 0) file_close(e->readHandle);
        free(e->buf);
        e->used = false; e->readHandle = -1; e->buf = 0;
        e->bufCap = 0; e->size = 0; e->dirty = false;
        return r;
    }

private:
    struct Entry {
        bool used, write, dirty;
        R1Fs::Key key;
        R1Fs::Path path;
        int readHandle;            /* PortBigFile handle for committed reads */
        uint8_t* buf;              /* staged whole-file image for writes */
        uint32_t bufCap;
        uint64_t size;
        uint32_t generation;
    };
    Entry iFiles[KBackFiles];
    uint32_t iNextGeneration;

    static bool SamePath(const R1Fs::Path& a, const R1Fs::Path& b) {
        if (a.length != b.length) return false;
        for (uint32_t i = 0; i < a.length; ++i) if (a.text[i] != b.text[i]) return false;
        return true;
    }
    Entry* FreeEntry() {
        for (unsigned i = 0; i < KBackFiles; ++i) if (!iFiles[i].used) return &iFiles[i];
        return 0;
    }
    Entry* FindKey(const R1Fs::Key& k) {
        if (k.slot >= KBackFiles) return 0;
        Entry* e = &iFiles[k.slot];
        if (!e->used || e->generation != k.generation) return 0;
        return e;
    }
    int32_t CommitWrite(Entry* e) {
        char name[R1Fs::KPathUnits + 1];
        int32_t rc = ToCStr(e->path, name, sizeof(name));
        if (rc != R1Fs::Success) return rc;
        /* file_save = create_write + append + commit (FatWritePath to the REAL
           FAT volume) + close, all through the kernel PortFile device. */
        const uint8_t* data = e->buf ? e->buf : (const uint8_t*)"";
        int result = file_save(name, data, (unsigned)e->size);
        if (result < 0) return R1Fs::General;
        e->dirty = false;
        return R1Fs::Success;
    }
};
}

/* No C++ global constructors: the backend is placement-constructed into this
   POD buffer inside app_main (elf2e32 rejects .init_array). */
static uint32_t g_backend_store[(sizeof(DiskFatBackend) + 3) / 4];

/*--------------------------------------------------------------------------*/
/* app_main -- entry point called by user/start.S _start.                     */
/*--------------------------------------------------------------------------*/
extern "C" int app_main(void)
    {
    print("GENUINEFS: starting");

    /* Construct the disk-backed filesystem backend. */
    DiskFatBackend* backend = new ((void*)g_backend_store) DiskFatBackend();

    /* B2 -- register this thread once (EExecFsRegisterThread = 81). */
    exec0(EExecFsRegisterThread);

    /* B3 -- publish the protected server name. CServer2::Start owns the
       receive loop (connect -> NewSessionL -> ServiceL -> complete) and only
       returns if the server handle is closed. */
    R1Fs::FileServer* server = R1Fs::FileServer::NewL(*backend);
    /* The port loader does not populate DProcess::iCapabilities from the E32
       header, so creating a '!' protected name traps with EPlatformSecurityTrap
       (K::Fault 4/ESystemThreadPanic).  Publish an unprotected alias for this
       integration proof; the IPC function numbers are unchanged. */
    _LIT(KAlias, "R1FileServer");
    const TInt r = server->Start(KAlias);

    print(r == KErrNone ? "GENUINEFS: stopped" : "GENUINEFS: server failed");
    delete server;
    return r;
    }
