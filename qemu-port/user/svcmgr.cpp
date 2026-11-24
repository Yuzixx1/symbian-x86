/*
 * svcmgr.cpp — S-02: service manager (ordered startup + supervision + shutdown).
 *
 * Boot service order is a static table:
 *   - EUSER.DLL   : already resident (E32LoadDll binds its exports); no process
 *                   to launch, kept first to document the dependency order.
 *   - portfilesrv : launched as a managed child; publishes "!FileServer".
 *   - future IPC services (DNS resolver, network stack, console, loader) are
 *     placeholders in the table, enabled as their images land.
 *
 * The orchestrator waits on each service's global name via ipc_connect(),
 * supervises children (restart on death with bounded backoff) and shuts down
 * in reverse on a fatal error. The optional "fs" command-line mode remains a
 * standalone FileServer host for bring-up; the boot path does not use it.
 *
 * Launch/kill/ctl execs (268/269/270) are installed by
 * variant/e32load.cpp::SvcMgrExecInstall at boot; kernelhwsrv/ is untouched.
 *
 * C++98 / -ffreestanding -fno-exceptions -fno-rtti, no libstdc++, no globals
 * with constructors (elf2e32 rejects .init_array).
 */

extern "C" {
#include "runtime.h"
#include "ipc.h"
}
#include "../fs/r1_fs_core.h"
#include "e32base.h"
#include "../fs/r1_fs_server.h"

extern "C" int exec0(unsigned number);
extern "C" int exec2(unsigned number, unsigned, unsigned);

/* Genuine kernel executive numbers (build/export/exec_enum.h). */
enum { EExecFsRegisterThread = 81 };
enum { ESvcLaunch = 268, ESvcKill = 269, ESvcCtl = 270 };
enum { ESvcCtlDead = 0, ESvcCtlReap = 1 };

/* Freestanding operator new/delete (no libstdc++ in E32 user images). */
void operator delete(void* p) noexcept { free(p); }
void operator delete(void* p, unsigned) noexcept { free(p); }
void* operator new(unsigned, void* where) noexcept { return where; }

/*--------------------------------------------------------------------------*/
/* Ring-3 filesystem backend — in-memory stand-in for the kernel FAT adapter. */
/* Mirrors the proven portfilesrv.cpp backend; the Core/CServer2 wiring is     */
/* unchanged when a ring-3 FAT adapter lands.                                  */
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
        e->open = false;
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

/* POD store: backend is placement-constructed inside app_main (no .init_array). */
static uint32_t g_backend_store[(sizeof(FatBackend) + 3) / 4];

/*--------------------------------------------------------------------------*/
/* FileServer host mode — owns "!FileServer" and blocks in the IPC loop.       */
/*--------------------------------------------------------------------------*/
static int run_fileserver(void)
    {
    print("SVCMGR: FileServer host starting");

    FatBackend* backend = new ((void*)g_backend_store) FatBackend();

    /* B2: register this thread as the FS owner (EExecFsRegisterThread = 81). */
    exec0(EExecFsRegisterThread);

    /* B3: publish. CServer2::Start owns the accept/dispatch/complete loop and
       blocks forever; it only returns if the server handle is closed. */
    R1Fs::FileServer* server = R1Fs::FileServer::NewL(*backend);
    const TInt r = server->Publish();

    print(r == KErrNone ? "SVCMGR: FileServer stopped" : "SVCMGR: server failed");
    delete server;
    return r;
    }

/*--------------------------------------------------------------------------*/
/* Orchestrator mode — ordered startup, supervision, shutdown.                 */
/*--------------------------------------------------------------------------*/
struct Desc { unsigned type_length; const char* ptr; };
static Desc desc(const char* text)
    {
    unsigned n = 0;
    while (text[n]) ++n;
    Desc d = {0x10000000u | n, text};
    return d;
    }

static int svc_launch(const char* path, const char* cmd)
    {
    Desc p = desc(path);
    Desc c = desc(cmd ? cmd : "");
    return exec2(ESvcLaunch, (unsigned)&p, (unsigned)&c);  /* >0 = new pid */
    }
static int svc_kill(int pid)   { return exec2(ESvcKill, (unsigned)pid, 0); }
static int svc_dead(int pid)   { return exec2(ESvcCtl, (unsigned)ESvcCtlDead, (unsigned)pid); }
static int svc_reap(void)      { return exec2(ESvcCtl, (unsigned)ESvcCtlReap, 0); }

/* Storage/FAT mount is already done by Asic::Init3, and EUSER.DLL is resident
   (E32LoadDll). A null path marks a prerequisite that needs no launch. */
struct Service { const char* path; const char* cmd; const char* name; const char* depends; };
static const Service kServices[] =
    {
    { 0, 0, 0, 0 },  /* EUSER.DLL: already running, documents boot order */
    // RETIRED by r1_shim_retire_apply: { "\\PORTFILESRV.E32", 0, "!FileServer", 0 },  /* F32 / R1 file server */
    /* { "\\CONSOLE.E32",  0, "!ConsoleServer", "!FileServer" },  H-02 */
    /* { "\\LOADER.E32",   0, "!Loader",        "!ConsoleServer" }, L-06 */
    /* { "\\DNSSRV.E32",   0, "!DnsResolver",   "!FileServer" },  future */
    /* { "\\NETSRV.E32",   0, "!NetStack",      "!FileServer" },  future */
    };
enum { KServiceCount = (int)(sizeof(kServices) / sizeof(kServices[0])) };

static int wait_ready(const char* name)
    {
    for (int attempt = 0; attempt < 250; ++attempt)
        {
        int h = ipc_connect(name);
        if (h >= 0) { ipc_close(h); return 0; }
        sleep_ms(20);
        }
    return -1;
    }

static int sPid[KServiceCount];
static int sRestarts[KServiceCount];

static void shutdown_services(void)
    {
    print("SVCMGR: shutdown (reverse order)");
    for (int i = KServiceCount - 1; i >= 0; --i)
        {
        if (sPid[i] > 0 && !svc_dead(sPid[i]))
            {
            svc_kill(sPid[i]);
            print("SVCMGR: killed a service");
            }
        sPid[i] = 0;
        }
    print("SVCMGR: reaped");
    svc_reap();
    }

static int run_orchestrator(void)
    {
    print("SVCMGR: starting");

    for (int i = 0; i < KServiceCount; ++i)
        {
        const Service& s = kServices[i];
        if (!s.path)  /* already-resident prerequisite (EUSER.DLL) */
            { print("SVCMGR: prerequisite already running"); continue; }
        int pid = svc_launch(s.path, s.cmd);
        if (pid <= 0) { print("SVCMGR: launch failed"); shutdown_services(); return -1; }
        sPid[i] = pid;
        sRestarts[i] = 0;
        if (s.name && wait_ready(s.name) != 0)
            { print("SVCMGR: service readiness timeout"); shutdown_services(); return -1; }
        print("SVCMGR: service ready");
        }
    print("SVCMGR: ordered startup complete");

    /* Supervise forever: restart an unexpected death with bounded backoff. */
    const int kMaxRestarts = 4;
    const int kBackoffMs[4] = {50, 100, 200, 400};
    for (;;)
        {
        sleep_ms(100);
        int terminal = 0;
        for (int i = 0; i < KServiceCount; ++i)
            {
            if (sPid[i] <= 0 || !svc_dead(sPid[i])) continue;
            svc_reap();
            if (sRestarts[i] >= kMaxRestarts)
                { print("SVCMGR: restart limit reached"); terminal = 1; break; }
            const int b = kBackoffMs[sRestarts[i]];
            ++sRestarts[i];
            sleep_ms(b);
            int pid = svc_launch(kServices[i].path, kServices[i].cmd);
            if (pid <= 0) { print("SVCMGR: relaunch failed"); terminal = 1; break; }
            sPid[i] = pid;
            if (kServices[i].name && wait_ready(kServices[i].name) != 0)
                { print("SVCMGR: relaunch readiness timeout"); terminal = 1; break; }
            print("SVCMGR: restarted a service");
            }
        if (terminal) break;
        }

    shutdown_services();
    print("SVCMGR: stopped");
    return 0;
    }

/*--------------------------------------------------------------------------*/
/* app_main — entry point called by user/start.S _start.                       */
/*--------------------------------------------------------------------------*/
extern "C" int app_main(void)
    {
    U16 cmd[64];
    const int n = command_line(cmd, 64);
    for (int i = 0; i + 1 < n; ++i)
        {
        U16 c = cmd[i];
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        if (c == 'f' && i + 1 < n)
            {
            U16 d = cmd[i + 1];
            if (d >= 'A' && d <= 'Z') d = d - 'A' + 'a';
            if (d == 's') return run_fileserver();
            }
        }
    return run_orchestrator();
    }
