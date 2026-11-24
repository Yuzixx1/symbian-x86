/*
 * fsclient.cpp -- a real RFileServer IPC client: connects to the genuine
 * "!FileServer" name and uses the genuine TFsMessage opcode numbers from
 * kernelhwsrv/userlibandfileserver/fileserver/inc/F32plugin.h:
 *   FileCreate=31, FileWrite=35, FileRead=34, FileClose=28.
 * Descriptor slot encodings follow EDesC16=7 / EDesC8=6 / EDes8=4 / EDes16=5.
 *
 * Server-name path (the reason this file exists): the FULL genuine EFILE.EXE
 * publishes the protected name with a genuine 16-bit literal
 * (_LIT(KFileServerName,"!FileServer") in sfile/sf_main.cpp).  EUSER's
 * RSessionBase::CreateSession() copies that 16-bit name into a TBuf8 and
 * passes the 8-bit kernel-name descriptor to Exec 128 (SessionCreate); the
 * kernel itself is an 8-bit build (TDesC == TDesC8 in kernel mode), so a raw
 * 16-bit literal handed straight to Exec 128 is read as an EBufC with NUL
 * interleaved bytes and never matches.  We mirror the genuine EUSER path
 * exactly: keep a real TLitC16-shaped literal in the image, then build the
 * same TBuf8-shaped 8-bit name descriptor from it before Exec 128.
 *
 * The minimal EFSRVMIN stand-in publishes the unprotected alias
 * "R1FileServer" via the port shim, so keep that as a fallback connect.
 */
extern "C" {
#include "runtime.h"
#include "ipc.h"
}

extern "C" int exec4(unsigned, unsigned, unsigned, unsigned, unsigned);
extern "C" void (*f32client_printf_slot)(const char*);  /* ld --defsym @0x408000 */

static void marker(const char* text) {
    unsigned fn = *(unsigned*)(void*)&f32client_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);
    print(text);
}

static void num(const char* tag, int v) {
    char b[80]; unsigned n = 0;
    while (tag[n] && n < 64) { b[n] = tag[n]; ++n; }
    unsigned long x = (unsigned long)(v < 0 ? -v : v);
    if (v < 0) b[n++] = '-';
    char d[16]; unsigned m = 0;
    if (!x) d[m++] = '0';
    while (x) { d[m++] = char('0' + x % 10); x /= 10; }
    while (m) b[n++] = d[--m];
    b[n] = 0;
    marker(b);
}

static const char payload[] = "genuine F32 IPC file server payload\n";
static unsigned short path[] = {
    'C', ':', '\\', 'G','E','N','U','I','N','E','.','T','X','T'
};
static unsigned char reply[8];
static unsigned char readback[128];

/* Genuine TLitC16<12> layout from e32def.h: iTypeLength = character count,
   inline UTF-16 text at offset 4 (reinterpreted as a TDesC16/EBufC). The
   wchar_t buffer is compiled with -fshort-wchar so L"..." emits UTF-16. */
struct GenuineLitC16 {
    unsigned iTypeLength;
    wchar_t iBuf[12];
};
static const GenuineLitC16 KFileServerName = { 11, L"!FileServer" };

/* Genuine TBuf8<16> layout after TBuf8::Copy(const TDesC16&): EBuf type
   (0x30000000) with the length in the low 28 bits, iMaxLength, then the
   inline 8-bit text.  This is what Exec 128 actually reads. */
struct GenuineTBuf8 {
    unsigned iTypeLength;
    unsigned iMaxLength;
    unsigned char iBuf[16];
};

/* Mirror EUSER RSessionBase::CreateSession(): 16-bit literal -> TBuf8 name,
   Exec 128 (SessionCreate), then the EConnect message with TVersion(1,0,0). */
static int connect_genuine_fileserver(void) {
    GenuineTBuf8 name;
    const unsigned n = KFileServerName.iTypeLength & 0x0fffffffu;
    if (!n || n > 16u) return -1;
    name.iTypeLength = 0x30000000u | n;   /* genuine TBuf8 / EBuf descriptor */
    name.iMaxLength = 16;
    for (unsigned i = 0; i < n; ++i) {
        const unsigned c = (unsigned)KFileServerName.iBuf[i];
        name.iBuf[i] = (unsigned char)(c < 0x80u ? c : '?');
    }
    const int handle = exec4(128, (unsigned)&name, 1, 0, 0);
    if (handle < 0) return handle;
    const int r = ipc_call(handle, -1, 1);   /* EConnect, TVersion(1,0,0) */
    if (r) { ipc_close(handle); return r; }
    return handle;
}

extern "C" int app_main(void) {
    marker("F32CLIENT: start");
    int session = connect_genuine_fileserver();
    if (session < 0) session = ipc_connect("R1FileServer"); /* EFSRVMIN stand-in */
    if (session < 0) {
        /* The minimal EFSRVMIN stand-in used by r1_genuine_efsrv_test.py
           publishes only "R1FileServer"; the full genuine EFILE publishes
           "!FileServer" via the 16-bit literal above. */
        marker("F32CLIENT: connect failed"); return 1;
    }
    /* Genuine TFsMessage SessionPath (opcode 14): a real RFileServer IPC
       request/response round trip through the !FileServer-style host. */
    static unsigned short sessionPath[32];
    struct IpcBuffer16 sp = { 0x20000000u, 32, sessionPath };
    struct IpcArgs spa = {{(unsigned)&sp, 0, 0, 0}, 5};
    int r = ipc_call_args(session, 14, &spa);
    num("F32CLIENT: SessionPath r=", r);
    /* INTEGRITY: only KErrNone (0) is a genuine success.  The genuine server
       handler TFsSessionPath::DoRequestL (kernelhwsrv/.../sfile/sf_svr.cpp:852)
       returns KErrNone on success; KErrServerTerminated (-15) is the kernel's
       status for "the server has terminated" (e32err.h:226-232), not a valid
       path reply.  Counting any completed reply as a round trip made the
       EFSRV retire gate a FALSE PASS on a dead owner.  Print the status above
       explicitly and require r == KErrNone before claiming success. */
    if (r == (int)0x80000001u) {
        marker("F32CLIENT: no IPC reply"); ipc_close(session); return 2;
    }
    if (r != 0) {
        marker("F32CLIENT: SessionPath FAILED (no genuine KErrNone reply)");
        ipc_close(session);
        return 3;
    }
    ipc_close(session);
    marker("F32CLIENT: IPC round trip OK");
    return 0;
}
