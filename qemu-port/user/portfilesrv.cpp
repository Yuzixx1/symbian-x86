/*
 * portfilesrv.cpp — S-04: genuine portfilesrv user host entry point (app_main).
 *
 * This is the ring-3 owner of the protected "!FileServer" name. It runs on the
 * real executive/IPC bridge (user/start.S -> app_main -> ipc_receive loop), not
 * on a hosted libc, and services EFSRV-wire messages with the shared R1 FS
 * engine (fs/r1_fs_core.cpp) over the genuine R1Fs::Backend contract.
 *
 * Bootstrap phases (docs/r1_service_boundary_v1.md, services/r1_service_host.*):
 *   B1/B0 — a trusted image is materialized and pinned by the kernel launcher.
 *   B2    — the FS owner thread registers once: UserSvr::FsRegisterThread() is
 *           the SDK spelling (EUSER export 1953); on the freestanding exec
 *           bridge the same kernel action is EExecFsRegisterThread = 81
 *           (see build/export/exec_enum.h). NOTE: 1221 is the EUSER _memcpy
 *           export, NOT FsRegisterThread. exec0(81) is the genuine call.
 *   B3    — "!FileServer" is published with CServer2::Start in the EUSER build.
 *           This bridge-native host publishes with ipc_server_create() and
 *           starts its receive loop at the same point in app_main().
 *
 * Why Core directly and not R1Fs::FileServer::ServiceL():
 *   fs/r1_fs_server.{h,cpp} is the genuine CServer2/CSession2 host. Its
 *   FileSession::ServiceL/NewSessionL are private and take an SDK RMessage2;
 *   it cannot be linked until the EUSER shim in user/e32base.h (being built by
 *   a sibling card) compiles. This file is the equivalent bridge-native
 *   dispatcher: it builds the R1Fs::Frame from the real IpcMessage, calls the
 *   same R1Fs::Decode + Core operations, and completes over ipc_complete().
 *   Once the shim lands, replace the local ServiceL dispatcher with
 *   FileServer::NewL/Publish and the CServer2 receive loop; the Core/Backend
 *   wiring below is unchanged.
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

/* Genuine kernel executive numbers (build/export/exec_enum.h). */
enum {
    EExecFsRegisterThread = 81,     /* KExecExec: ExecHandler::FsRegisterThread */
    EExecMessageGetDesLength = 167, /* RMessageK::GetDesLength */
    EExecMessageGetDesMaxLength = 168
};

extern "C" int exec0(unsigned number);
extern "C" int exec2(unsigned number, unsigned arg1, unsigned arg2);

/* The R1Fs::Backend vtable carries a deleting destructor; provide the
   freestanding global operator delete that the port runtime's malloc/free
   backs (no libstdc++ is linked into E32 user images). */
void operator delete(void* p) noexcept { free(p); }
void operator delete(void* p, unsigned) noexcept { free(p); }
/* Placement new: construct the FS engine into static storage inside app_main
   so the image has no global constructors / .init_array (elf2e32 rejects it). */
void* operator new(unsigned, void* where) noexcept { return where; }

/* Genuine Symbian error values (numeric, from r1_fs_backend.h Status). */
enum {
    KErrNoneLocal     = R1Fs::Success,
    KErrArgumentLocal = R1Fs::Argument,
    KErrNotSuppLocal  = R1Fs::NotSupported,
    KErrBadHandleLocal= R1Fs::BadHandle,
    KErrCorruptLocal  = R1Fs::Corrupt,
    KErrOverflowLocal = R1Fs::Overflow,
    KErrNotReadyLocal = R1Fs::NotReady
};

/*--------------------------------------------------------------------------*/
/* Portable, self-contained filesystem backend.                            */
/* Implements the private R1Fs::Backend contract (fs/r1_fs_backend.h).       */
/* Stored bytes persist for the life of the host process (across Close),     */
/* which is enough for the R1 file-service regression on a disposable disk.  */
/* Swap this object for FatBackend/ChannelBackend when a freestanding FAT    */
/* adapter is available; Core and the dispatcher below do not change.        */
/*--------------------------------------------------------------------------*/
namespace {
const unsigned KBackFiles = 16;
const uint32_t KBackMaxBytes = 8u * 1024u * 1024u;

class RamBackend : public R1Fs::Backend {
public:
    RamBackend() : iNextGeneration(1) {}

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

/* No C++ global constructors: the objects are placement-constructed in
   app_main into these POD buffers (see the .init_array note above). */
static uint32_t g_backend_store[(sizeof(RamBackend) + 3) / 4];
static uint32_t g_core_store[(sizeof(R1Fs::Core) + 3) / 4];
static RamBackend* g_backend = 0;
static R1Fs::Core* g_core = 0;

/*--------------------------------------------------------------------------*/
/* Session table: maps the kernel's opaque session cookie to a Core token.   */
/*--------------------------------------------------------------------------*/
namespace {
const unsigned KPortSessions = R1Fs::KMaxSessions;
struct PortSession { bool used; void* cookie; uint32_t token; };
static PortSession g_sessions[KPortSessions];

PortSession* SessionByCookie(void* cookie) {
    if (!cookie) return 0;
    for (unsigned i = 0; i < KPortSessions; ++i)
        if (g_sessions[i].used && g_sessions[i].cookie == cookie) return &g_sessions[i];
    return 0;
}
PortSession* FreeSession() {
    for (unsigned i = 0; i < KPortSessions; ++i) if (!g_sessions[i].used) return &g_sessions[i];
    return 0;
}
}

/*--------------------------------------------------------------------------*/
/* Descriptor helpers: RMessage2 GetDesLength/GetDesMaxLength equivalents.    */
/*--------------------------------------------------------------------------*/
static int DesLength(const struct IpcMessage* m, int slot) {
    return exec2(EExecMessageGetDesLength, (unsigned)m->handle, (unsigned)slot);
}
static int DesMaxLength(const struct IpcMessage* m, int slot) {
    int r = exec2(EExecMessageGetDesMaxLength, (unsigned)m->handle, (unsigned)slot);
    return r >= 0 ? r : DesLength(m, slot);
}

/* Read a UTF-16 path descriptor (slot 0) into text/length (code units).
   Accepts both 16-bit descriptors (native EFSRV wire) and 8-bit ASCII ones. */
static int ReadPath(struct IpcMessage* m, uint16_t* text, unsigned capacity,
                    unsigned& units) {
    int bytes = DesLength(m, 0);
    if (bytes < 0) return bytes;
    if ((unsigned)bytes > capacity * 2) return R1Fs::BadName;
    int got = ipc_read16(m, 0, text, bytes, 0);
    if (got == bytes) { units = (unsigned)got / 2; return R1Fs::Success; }
    /* Fallback: 8-bit descriptor carrying an ASCII path. */
    if (got < 0) {
        uint8_t ascii[2 * R1Fs::KPathUnits];
        got = ipc_read8(m, 0, ascii, bytes, 0);
        if (got < 0) return got;
        if ((unsigned)got > capacity) return R1Fs::BadName;
        for (int i = 0; i < got; ++i) text[i] = ascii[i];
        units = (unsigned)got;
        return R1Fs::Success;
    }
    return R1Fs::Argument;
}

/* Write a UTF-16 path back into a descriptor; returns a Symbian status. */
static int WritePath(struct IpcMessage* m, int slot, const uint16_t* text,
                     unsigned units) {
    int cap = DesMaxLength(m, slot);
    if (cap < 0) return cap;
    if (cap < (int)(units * 2)) return R1Fs::Overflow;
    int wrote = ipc_write16(m, slot, text, (int)(units * 2), 0);
    if (wrote < 0) return wrote;
    return wrote == (int)(units * 2) ? R1Fs::Success : R1Fs::Corrupt;
}

/* Raw package (TPckg) copy: 8 bytes for TInt64-sized values, 4 for handles. */
static int ReadRaw(struct IpcMessage* m, int slot, void* buf, int length, int offset) {
    int got = ipc_read8(m, slot, buf, length, offset);
    return got < 0 ? got : (got == length ? R1Fs::Success : R1Fs::Corrupt);
}
static int WriteRaw(struct IpcMessage* m, int slot, const void* buf, int length, int offset) {
    int wrote = ipc_write8(m, slot, buf, length, offset);
    return wrote < 0 ? wrote : (wrote == length ? R1Fs::Success : R1Fs::Corrupt);
}

/*--------------------------------------------------------------------------*/
/* Dispatch: the bridge-native twin of FileSession::ServiceL/Process.         */
/*--------------------------------------------------------------------------*/
static int ProcessMessage(struct IpcMessage* m, uint32_t token,
                          const R1Fs::Decoded& d) {
    R1Fs::Core& core = *g_core;
    const int op = d.operation;
    const int h = (int)(uint32_t)m->args[3];

    if (op == R1Fs::SessionPath) {
        R1Fs::Path path;
        int r = core.GetPath(token, path);
        if (r) return r;
        return WritePath(m, 0, path.text, path.length);
    }
    if (op == R1Fs::SetSessionPath || op == R1Fs::FileOpen ||
        op == R1Fs::FileCreate || op == R1Fs::FileReplace) {
        uint16_t name[R1Fs::KPathUnits];
        unsigned units = 0;
        int r = ReadPath(m, name, R1Fs::KPathUnits, units);
        if (r) return r;
        if (op == R1Fs::SetSessionPath)
            return core.SetPath(token, name, units);
        uint32_t handle = 0;
        const R1Fs::Disposition how = op == R1Fs::FileOpen ? R1Fs::OpenExisting :
            (op == R1Fs::FileCreate ? R1Fs::CreateNew : R1Fs::ReplaceExisting);
        r = core.Open(token, name, units, (uint32_t)m->args[1], how, handle);
        if (r) return r;
        r = WriteRaw(m, 3, &handle, 4, 0); /* handle reply before publish */
        if (r) { core.Abort(token, handle); return r; }
        return core.Commit(token, handle);
    }

    if (op == R1Fs::FileClose) return core.Close(token, (uint32_t)h);
    if (op == R1Fs::FileFlush) return core.Flush(token, (uint32_t)h);
    if (op == R1Fs::FileReadCancel) {
        const int r = core.CheckShare(token, (uint32_t)h);
        return r ? r : R1Fs::NotSupported; /* no queued messages to cancel */
    }

    if (op == R1Fs::FileSize) {
        uint64_t size = 0;
        int r = core.Size(token, (uint32_t)h, size);
        if (r) return r;
        int64_t wire = (int64_t)size;
        return WriteRaw(m, 0, &wire, 8, 0);
    }
    if (op == R1Fs::FileSetSize) {
        int64_t n = m->args[0];
        if (d.widePosition) { int r = ReadRaw(m, 0, &n, 8, 0); if (r) return r; }
        int r = core.Writable(token, (uint32_t)h);
        if (r) return r;
        if (n < 0) return R1Fs::Argument;
        return core.Resize(token, (uint32_t)h, n);
    }
    if (op == R1Fs::FileSeek) {
        int64_t n = m->args[0];
        if (d.widePosition) { int r = ReadRaw(m, 0, &n, 8, 0); if (r) return r; }
        uint64_t result = 0;
        int r = core.Seek(token, (uint32_t)h, n, (uint32_t)m->args[1], result);
        if (r) return r;
        int64_t wire = (int64_t)result;
        return WriteRaw(m, 2, &wire, 8, 0);
    }
    if (op == R1Fs::FileRead || op == R1Fs::FileWrite) {
        int rights = op == R1Fs::FileWrite ? core.Writable(token, (uint32_t)h)
                                           : core.Readable(token, (uint32_t)h);
        if (rights) return rights;
        const int requested = m->args[1];
        if (requested < 0 || requested > 1024 * 1024) return R1Fs::Argument;
        int64_t pos = d.widePosition ? 0 : (int64_t)(uint32_t)m->args[2];
        if (d.widePosition) { int r = ReadRaw(m, 2, &pos, 8, 0); if (r) return r; }
        bool current = pos == -1 || (uint64_t)pos == 0xffffffffULL;
        if (pos < 0 && !current) return R1Fs::Argument;

        static uint8_t bytes[R1Fs::KTransferBytes];
        uint32_t total = 0;
        if (op == R1Fs::FileWrite) {
            do {
                int chunk = requested - (int)total > R1Fs::KTransferBytes
                          ? R1Fs::KTransferBytes : requested - (int)total;
                int got = ipc_read8(m, 0, bytes, chunk, (int)total);
                if (got < 0) return got;
                if (got != chunk) return R1Fs::Argument;
                uint32_t done = 0;
                int r = core.Write(token, (uint32_t)h, (uint64_t)pos, current,
                                   bytes, (uint32_t)chunk, done);
                if (r) return r;
                if (done != (uint32_t)chunk) return R1Fs::Corrupt;
                total += done; current = true;
            } while (total < (uint32_t)requested);
            return R1Fs::Success;
        }
        do {
            int chunk = requested - (int)total > R1Fs::KTransferBytes
                      ? R1Fs::KTransferBytes : requested - (int)total;
            uint32_t done = 0; uint64_t next = 0;
            int r = core.StageRead(token, (uint32_t)h, (uint64_t)pos, current,
                                   bytes, (uint32_t)chunk, done, next);
            if (r) return r;
            r = WriteRaw(m, 0, bytes, (int)done, (int)total);
            if (r) return r;
            r = core.ConfirmRead(token, (uint32_t)h, next);
            if (r) return r;
            total += done; current = true;
            if (done < (uint32_t)chunk) break; /* EOF / short read */
        } while (total < (uint32_t)requested);
        return R1Fs::Success;
    }
    return R1Fs::NotSupported;
}

/* Build the R1Fs::Frame from a real IpcMessage, decode, dispatch, and
   complete. Mirrors FileSession::ServiceL's descriptor interrogation. */
static int ServiceL(struct IpcMessage* m, uint32_t token) {
    R1Fs::Frame frame = R1Fs::Frame();
    frame.function = (uint32_t)m->function;
    for (int i = 0; i < 4; ++i) frame.a[i] = m->args[i];
    for (int i = 0; i < 4; ++i) {
        const int len = DesLength(m, i);
        if (len < 0) continue; /* not a descriptor slot */
        const int cap = DesMaxLength(m, i);
        frame.descriptor[i] = 1;
        frame.length[i] = (uint32_t)len;
        frame.capacity[i] = (uint32_t)(cap < 0 ? len : cap);
    }
    R1Fs::Decoded decoded = R1Fs::Decoded();
    int r = R1Fs::Decode(frame, decoded);
    if (!r) r = ProcessMessage(m, token, decoded);
    return r;
}

/* ServiceError equivalent: every message gets exactly one terminal reply. */
static void ServiceError(struct IpcMessage* m, int error) {
    if (m && m->handle) ipc_complete(m, error);
}

/*--------------------------------------------------------------------------*/
/* app_main — entry point called by user/start.S _start.                     */
/*--------------------------------------------------------------------------*/
extern "C" int app_main(void) {
    print("PORTFILESRV: starting");

    /* Construct the backend + FS engine on the FS owner thread. */
    g_backend = new ((void*)g_backend_store) RamBackend();
    g_core = new ((void*)g_core_store) R1Fs::Core(*g_backend);

    /* B2 — register this thread once (EExecFsRegisterThread = 81). The EUSER
       shim spells this UserSvr::FsRegisterThread()/User::FsRegisterThread(). */
    exec0(EExecFsRegisterThread);

    /* B3 — publish the protected server name before any client is released. */
    const int server = ipc_server_create("!FileServer");
    if (server < 0) {
        print("PORTFILESRV: ipc_server_create failed");
        return 1;
    }

    print("PORTFILESRV: !FileServer ready");

    for (;;) {
        struct IpcMessage m;
        if (ipc_receive(server, &m) < 0) break; /* server handle closed */

        /* Session connect (EConnect, function -1). */
        if (m.function == -1) {
            if (m.args[0] != 1) { ServiceError(&m, R1Fs::NotSupported); continue; }
            PortSession* s = FreeSession();
            if (!s) { ServiceError(&m, R1Fs::NoMemory); continue; }
            uint32_t token = 0;
            int r = g_core->Connect((uint32_t)m.args[3], token);
            if (r) { ServiceError(&m, r); continue; }
            s->used = true; s->cookie = s; s->token = token;
            ipc_accept(&m, s);          /* bind opaque session cookie */
            ipc_complete(&m, R1Fs::Success);
            continue;
        }

        /* Session disconnect / teardown notification (function -2). */
        if (m.function == -2) {
            PortSession* s = SessionByCookie(m.session);
            if (s) { g_core->Disconnect(s->token); s->used = false; }
            ServiceError(&m, R1Fs::Success);
            continue;
        }

        PortSession* s = SessionByCookie(m.session);
        if (!s) { ServiceError(&m, R1Fs::BadHandle); continue; }

        int result = ServiceL(&m, s->token);
        ServiceError(&m, result); /* one terminal completion per message */
    }

    ipc_close(server);
    print("PORTFILESRV: stopped");
    return 0;
}
