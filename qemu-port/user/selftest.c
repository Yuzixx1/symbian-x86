/*
 * selftest.c -- single comprehensive OS self-test E32.
 *
 * One program that exercises the core OS services from userland and reports
 * a single PASS/FAIL verdict.  Boot the ISO, at the shell prompt run:
 *
 *     portfilesrv      (start the file server, once)
 *     selftest         (run this program)
 *
 * It prints one marker per service and ends with RESULT PASS / RESULT FAIL.
 *
 * Tests:
 *   1 euser   - EUSER.DLL ord 915 (RDebug::Printf) is bound (all markers flow
 *               through it, so if you see them, it works).
 *   2 malloc  - chunk-backed user heap allocates + frees.
 *   3 chunk   - RChunk create/open/base/size round trip.
 *   4 file    - PortFile save + read round trip.
 *   5 ipc     - server create + client connect + close round trip (no blocking).
 *   6 netbridge - kernel<->userland e1000 frame bridge (execs 247/248); reports
 *               FAIL only if the bridge was never initialised (run the `netbridge`
 *               shell command first).
 *
 * Build: user.mk appends `SELFTEST.E32` to `userland` with
 *        elf2e32.py --euser-import 915 (genuine PE2 import of EUSER.DLL).
 */
#include "runtime.h"
#include "ipc.h"
#include "netbridge.h"

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern unsigned int selftest_printf_slot;
typedef void (*StPrintf)(const char*, ...);

static int g_failures = 0;

static void marker(const char* text)
{
    unsigned int fn = *(volatile unsigned int*)&selftest_printf_slot;
    if (fn != 915u && fn != 0u) ((StPrintf)fn)(text);
    else print(text);
}

/* ---- tiny formatters (no libc) ---- */
static unsigned append(char* p, unsigned k, const char* s)
{
    while (*s) p[k++] = *s++;
    return k;
}
static unsigned dec(char* p, unsigned k, unsigned v)
{
    char t[12];
    int n = 0;
    if (!v) t[n++] = '0';
    while (v) { t[n++] = (char)('0' + v % 10u); v /= 10u; }
    while (n) p[k++] = t[--n];
    return k;
}
static void result(const char* name, int ok)
{
    char line[128];
    unsigned k = append(line, 0, "ST: ");
    k = append(line, k, name);
    k = append(line, k, ok ? " OK" : " FAIL");
    line[k] = 0;
    marker(line);
    if (!ok) ++g_failures;
}

/* ---- tiny memcmp (no libc) ---- */
static int memcmp(const void* a, const void* b, unsigned n)
{
    const unsigned char* pa = (const unsigned char*)a;
    const unsigned char* pb = (const unsigned char*)b;
    while (n--)
        {
        if (*pa != *pb) return 1;
        ++pa; ++pb;
        }
    return 0;
}

/* ---- test 1: EUSER ord 915 binding ---- */
static void test_euser(void)
{
    /* After E32Run binds the import, the IAT slot holds the resolved
     * RDebug::Printf address (not the ordinal 915, not 0). */
    unsigned int fn = *(volatile unsigned int*)&selftest_printf_slot;
    result("euser", (fn != 0u) && (fn != 915u));
}

/* ---- test 2: malloc/free user heap ---- */
static void test_malloc(void)
{
    void* a = malloc(64);
    void* b = malloc(256);
    int ok = (a != 0) && (b != 0) && (a != b);
    if (ok)
        {
        char* p = (char*)a;
        p[0] = 'A'; p[63] = 'Z';
        ok = (p[0] == 'A') && (p[63] == 'Z');
        free(a); free(b);
        }
    result("malloc", ok);
}

/* ---- test 3: RChunk create/open/base/size ---- */
static void test_chunk(void)
{
    int h = chunk_create("STEST", 4096, 4096);
    int ok = (h >= 0);
    if (ok)
        {
        void* base = chunk_base(h);
        unsigned sz = chunk_size(h);
        ok = (base != 0) && (sz == 4096);
        if (ok)
            {
            int h2 = chunk_open("STEST");
            ok = (h2 >= 0) && (chunk_size(h2) == 4096);
            }
        }
    result("chunk", ok);
}

/* ---- test 4: file save + read ---- */
static void test_file(void)
{
    const char* text = "SELFTEST file round trip OK";
    unsigned len = 0;
    while (text[len]) ++len;
    int ok = (file_save("STEST.TXT", text, len) == 0);
    if (ok)
        {
        int h = file_open("STEST.TXT");
        ok = (h >= 0);
        if (ok)
            {
            char buf[128];
            int r = file_read(h, buf, sizeof(buf) - 1);
            file_close(h);
            ok = (r == (int)len) && (memcmp(buf, text, len) == 0);
            }
        }
    result("file", ok);
}

/* ---- test 5: IPC server create + connect + close (non-blocking) ---- */
static void test_ipc(void)
{
    int srv = ipc_server_create("STEST");
    int ok = (srv >= 0);
    if (ok)
        {
        int sess = ipc_connect("STEST");
        ok = ok && (sess >= 0);
        if (sess >= 0) ipc_close(sess);
        ipc_close(srv);
        }
    result("ipc", ok);
}

/* ---- test 6: netbridge discovery (execs 247/248) ---- */
static void test_netbridge(void)
{
    int base = exec2(QE_EXEC_NETBRIDGE_BASE, 0, 0);
    int size = exec2(QE_EXEC_NETBRIDGE_SIZE, 0, 0);
    int ok = (base > 0) && (size >= (int)sizeof(struct nb_shared));
    if (ok)
        {
        struct nb_shared* sh = (struct nb_shared*)base;
        ok = (sh->magic == NB_MAGIC) && (sh->version == NB_VERSION);
        if (!ok)
            marker("ST: netbridge note: run the `netbridge` shell command first");
        }
    result("netbridge", ok);
}

int app_main(void)
{
    marker("ST: SELFTEST START - EKA2 PC OS core services");
    test_euser();
    test_malloc();
    test_chunk();
    test_file();
    test_ipc();
    test_netbridge();
    char line[128];
    unsigned k = append(line, 0, "ST: failures=");
    k = dec(line, k, (unsigned)g_failures);
    line[k] = 0;
    marker(line);
    if (g_failures == 0)
        marker("ST: RESULT PASS - all core services operational");
    else
        marker("ST: RESULT FAIL - one or more services broken");
    process_exit(g_failures == 0 ? 0 : 1);
    return 0; /* unreached */
}
