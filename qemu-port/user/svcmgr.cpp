/*
 * svcmgr.cpp — S-02: ordered system-server startup + supervision + shutdown.
 *
 * User-mode E32 service manager (app_main via user/start.S). It owns a static
 * service table {image path, publish name, depends-on}, starts each service in
 * dependency order through a trusted launch exec (kernel-side E32Run variant),
 * waits for each to publish its global name, supervises children (restart on
 * death with bounded exponential backoff), and shuts down in reverse order
 * (E32Kill each child, then E32Reap) before returning to process_exit.
 *
 * The launch/kill/ctl execs (268/269/270) are installed over three test-only
 * slow-exec slots by variant/e32load.cpp::SvcMgrExecInstall() at boot. The
 * genuine kernel source tree (kernelhwsrv/) is not modified.
 *
 * C++98 / -ffreestanding -fno-exceptions -fno-rtti, no libstdc++, no globals
 * with constructors. Mirrors docs/r1_service_manager_design_v1.md.
 */

extern "C" {
#include "runtime.h"
#include "ipc.h"
}

extern "C" int exec2(unsigned number, unsigned, unsigned);

/* Trusted service-manager exec numbers (see SvcMgrExecInstall). */
enum { ESvcLaunch = 268, ESvcKill = 269, ESvcCtl = 270 };
enum { ESvcCtlDead = 0, ESvcCtlReap = 1 };

/* TDesC8-shaped descriptor {type|length, ptr}, as used by ipc.c. */
struct Desc { unsigned type_length; const char* ptr; };
static Desc desc(const char* text)
    {
    unsigned n = 0;
    while (text[n]) ++n;
    Desc d = {0x10000000u | n, text};
    return d;
    }

static int svc_launch(const char* path)
    {
    Desc d = desc(path);
    return exec2(ESvcLaunch, (unsigned)&d, 0);  /* >0 = new process id */
    }
static int svc_kill(int pid)   { return exec2(ESvcKill, (unsigned)pid, 0); }
static int svc_dead(int pid)   { return exec2(ESvcCtl, (unsigned)ESvcCtlDead, (unsigned)pid); }
static int svc_reap(void)      { return exec2(ESvcCtl, (unsigned)ESvcCtlReap, 0); }

/* Static service table. Storage/FAT mount is already done by Asic::Init3;
   F32 is portfilesrv owning "!FileServer". Console (!ConsoleServer) and loader
   (!Loader) are stubbed until H-02/L-06 land (design doc step 3). */
struct Service { const char* path; const char* name; const char* depends; };
static const Service kServices[] =
    {
    { "\\PORTFILESRV.E32", "!FileServer", 0 },  /* storage -> F32 */
    /* { "\\CONSOLE.E32",   "!ConsoleServer", "!FileServer" },  H-02 */
    /* { "\\LOADER.E32",    "!Loader",        "!ConsoleServer"}, L-06 */
    };
enum { KServiceCount = (int)(sizeof(kServices) / sizeof(kServices[0])) };

/* Bounded readiness probe: connect to the published global name. ipc_connect
   returns a negative error until CServer2::Start publishes the name. */
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

/* Shutdown: clients before servers (reverse table order), E32Kill each live
   child, withdraw, then final E32Reap. */
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

extern "C" int app_main(void)
    {
    print("SVCMGR: starting");

    /* Ordered startup: each service starts only after its dependency's global
       name is connectable. depends-on is honoured by table order today. */
    for (int i = 0; i < KServiceCount; ++i)
        {
        const Service& s = kServices[i];
        int pid = svc_launch(s.path);
        if (pid <= 0) { print("SVCMGR: launch failed"); shutdown_services(); return -1; }
        sPid[i] = pid;
        sRestarts[i] = 0;
        if (s.name && wait_ready(s.name) != 0)
            { print("SVCMGR: service readiness timeout"); shutdown_services(); return -1; }
        print("SVCMGR: service ready");
        }
    print("SVCMGR: ordered startup complete");

    /* Supervision: poll each child; restart an unexpected death with bounded
       exponential backoff. A deliberate kill (our own shutdown) is not polled
       because shutdown sets sPid to 0 before returning. */
    const int kMaxRestarts = 4;
    const int kBackoffMs[4] = {50, 100, 200, 400};
    for (int pass = 0; pass < 100; ++pass)  /* run budget; stop IPC is future */
        {
        sleep_ms(100);
        int terminal = 0;
        for (int i = 0; i < KServiceCount; ++i)
            {
            if (sPid[i] <= 0 || !svc_dead(sPid[i])) continue;
            svc_reap();  /* free the dead child's exec/reaper record */
            if (sRestarts[i] >= kMaxRestarts)
                { print("SVCMGR: restart limit reached"); terminal = 1; break; }
            const int b = kBackoffMs[sRestarts[i]];
            ++sRestarts[i];
            sleep_ms(b);
            int pid = svc_launch(kServices[i].path);
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
