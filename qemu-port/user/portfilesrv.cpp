/*
 * portfilesrv.cpp — I-08: genuine portfilesrv user host entry point (app_main).
 *
 * Ring-3 owner of the protected "!FileServer" name. It runs on the real
 * executive/IPC bridge (user/start.S -> app_main -> CServer2::Start receive
 * loop), not on a hosted libc. The EUSER shim (user/e32base.h) now supports
 * genuine CServer2/CSession2, so fs/r1_fs_server.cpp is linked directly:
 * FileServer::Publish() creates and drives the real CServer2 framework.
 *
 * Bootstrap phases (docs/r1_service_boundary_v1.md, services/r1_service_host.*):
 *   B1/B0 — a trusted image is materialized and pinned by the kernel launcher.
 *   B2    — the FS owner thread registers once: UserSvr::FsRegisterThread() is
 *           the SDK spelling (EUSER export 1953); on the freestanding exec
 *           bridge the same kernel action is EExecFsRegisterThread = 81
 *           (see build/export/exec_enum.h). NOTE: 1221 is the EUSER _memcpy
 *           export, NOT FsRegisterThread. exec0(81) is the genuine call.
 *   B3    — "!FileServer" is published by CServer2::Start (via FileServer::
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
/* Freestanding backend for the ring-3 server.                              */
/* Implements the private R1Fs::Backend contract (fs/r1_fs_backend.h).       */
/* Named FatBackend here as the in-process stand-in: the genuine fs/r1_fs_fat*/
/* .cpp needs kernel fat32.cpp symbols (FatReadPath/FatWritePath/FatMount)   */
/* that are not exported to E32 user images. The Core/CServer2 wiring is    */
/* unchanged when a ring-3 FAT adapter lands.                                */
/*--------------------------------------------------------------------------*/
namespace {
const unsigned KBackFiles = 16;
const uint32_t KBackMaxBytes = 8u * 1024u * 1024u;

class FatBackend : public R1Fs::Backend {
public:
    FatBackend() : iNextGeneration(1) {}

    int32_t Lookup(const R1Fs::Path& path, R1Fs::Meta& meta) {
        const Entry* e = FindPath(path);
        if (!e) return R1Fs::NotFound;
        meta.size = e->size;
        meta.identity = e->generation;
        return R1Fs::Success;
    }

    int32_t Open(const R1Fs::Path& path, R1Fs::Disposition how,
                 R1Fs::Key& key, R1Fs::Meta& meta) {
        Entry* e = FindPath(path);
        if (how == R1Fs::OpenExisting) {
            if (!e) return R1Fs::NotFound;
        } else if (how == R1Fs::CreateNew) {
            if (e) return R1Fs::AlreadyExists;
            e = FreeEntry();
            if (!e) return R1Fs::NoMemory;
            InitEntry(*e, path);
        } else { /* ReplaceExisting */
            if (e) { e->size = 0; }
            else { e = FreeEntry(); if (!e) return R1Fs::NoMemory; InitEntry(*e, path); }
        }
        if (e->open) return R1Fs::InUse;
        e->open = true;
        key.slot = (unsigned)(e - iFiles);
        key.generation = e->generation;
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
        uint32_t avail = e->size - (uint32_t)offset;
        uint32_t n = capacity < avail ? capacity : avail;
        for (uint32_t i = 0; i < n; ++i) dst[i] = e->data[(uint32_t)offset + i];
        r.committed = n; r.size = e->size;
        return r;
    }

    R1Fs::IoResult WriteAt(R1Fs::Key key, uint64_t offset, const uint8_t* src,
                           uint32_t length) {
        R1Fs::IoResult r = R1Fs::IoResult();
        r.error = R1Fs::Success; r.stable = true;
        Entry* e = FindKey(key);
        if (!e || !src) { r.error = R1Fs::BadHandle; return r; }
        if (offset > KBackMaxBytes || length > KBackMaxBytes - (uint32_t)offset) {
            r.error = R1Fs::TooBig; r.size = e->size; return r;
        }
        uint64_t end = offset + length;
        if (end > e->cap) {
            uint32_t want = end > KBackMaxBytes ? KBackMaxBytes : (uint32_t)end;
            uint32_t cap = e->cap ? e->cap : 256;
            while (cap < want) cap = cap > KBackMaxBytes / 2 ? want : cap * 2;
            uint8_t* bigger = (uint8_t*)malloc(cap);
            if (!bigger) { r.error = R1Fs::NoMemory; r.size = e->size; return r; }
            for (uint32_t i = 0; i < e->size; ++i) bigger[i] = e->data[i];
            free(e->data); e->data = bigger; e->cap = cap;
        }
        for (uint32_t i = 0; i < length; ++i) e->data[(uint32_t)offset + i] = src[i];
        if (end > e->size) e->size = (uint32_t)end;
        r.committed = length; r.size = e->size;
        return r;
    }

    R1Fs::IoResult Resize(R1Fs::Key key, uint64_t newSize) {
        R1Fs::IoResult r = R1Fs::IoResult();
        r.error = R1Fs::Success; r.stable = true;
        Entry* e = FindKey(key);
        if (!e) { r.error = R1Fs::BadHandle; return r; }
        if (newSize > KBackMaxBytes) { r.error = R1Fs::TooBig; r.size = e->size; return r; }
        if (newSize > e->cap) {
            uint32_t cap = e->cap ? e->cap : 256;
            while (cap < newSize) cap = cap > KBackMaxBytes / 2 ? (uint32_t)newSize : cap * 2;
            uint8_t* bigger = (uint8_t*)malloc(cap);
            if (!bigger) { r.error = R1Fs::NoMemory; r.size = e->size; return r; }
            for (uint32_t i = 0; i < e->size; ++i) bigger[i] = e->data[i];
            free(e->data); e->data = bigger; e->cap = cap;
        }
        for (uint32_t i = (uint32_t)newSize; i < e->size; ++i) e->data[i] = 0;
        e->size = (uint32_t)newSize;
        r.size = e->size;
        return r;
    }

    int32_t Flush(R1Fs::Key key) { return FindKey(key) ? R1Fs::Success : R1Fs::BadHandle; }

    int32_t Close(R1Fs::Key key) {
        Entry* e = FindKey(key);
        if (!e) return R1Fs::BadHandle;
        e->open = false; /* keep bytes for later Open */
        return R1Fs::Success;
    }

private:
    struct Entry {
        bool used, open;
        R1Fs::Key key;
        R1Fs::Path path;
        uint8_t* data;
        uint32_t size, cap, generation;
        Entry() : used(false), open(false), data(0), size(0), cap(0), generation(0) {}
    };
    Entry iFiles[KBackFiles];
    uint32_t iNextGeneration;

    static bool SamePath(const R1Fs::Path& a, const R1Fs::Path& b) {
        if (a.length != b.length) return false;
        for (uint32_t i = 0; i < a.length; ++i) if (a.text[i] != b.text[i]) return false;
        return true;
    }
    Entry* FindPath(const R1Fs::Path& p) {
        for (unsigned i = 0; i < KBackFiles; ++i)
            if (iFiles[i].used && SamePath(iFiles[i].path, p)) return &iFiles[i];
        return 0;
    }
    Entry* FreeEntry() {
        for (unsigned i = 0; i < KBackFiles; ++i) if (!iFiles[i].used) return &iFiles[i];
        return 0;
    }
    Entry* FindKey(const R1Fs::Key& k) {
        if (k.slot >= KBackFiles) return 0;
        Entry* e = &iFiles[k.slot];
        if (!e->used || !e->open || e->generation != k.generation) return 0;
        return e;
    }
    void InitEntry(Entry& e, const R1Fs::Path& p) {
        e.used = true; e.open = false; e.path = p;
        e.data = 0; e.size = 0; e.cap = 0;
        e.generation = iNextGeneration++;
        if (!e.generation) e.generation = iNextGeneration++;
    }
};
}

/* No C++ global constructors: the backend is placement-constructed into this
   POD buffer inside app_main (elf2e32 rejects .init_array). */
static uint32_t g_backend_store[(sizeof(FatBackend) + 3) / 4];

/*--------------------------------------------------------------------------*/
/* app_main -- entry point called by user/start.S _start.                     */
/*--------------------------------------------------------------------------*/
extern "C" int app_main(void)
    {
    print("PORTFILESRV: starting");

    /* Construct the filesystem backend on the FS owner thread. */
    FatBackend* backend = new ((void*)g_backend_store) FatBackend();

    /* B2 -- register this thread once (EExecFsRegisterThread = 81). */
    exec0(EExecFsRegisterThread);

    /* B3 -- publish the protected server name. CServer2::Start owns the
       receive loop (connect -> NewSessionL -> ServiceL -> complete) and only
       returns if the server handle is closed. */
    R1Fs::FileServer* server = R1Fs::FileServer::NewL(*backend);
    const TInt r = server->Publish();

    print(r == KErrNone ? "PORTFILESRV: stopped" : "PORTFILESRV: server failed");
    delete server;
    return r;
    }
