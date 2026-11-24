/*
 * ws_e32_client.c -- step 1 of docs/WINSRV_MIGRATION_PLAN.md: the wsclient
 * API ported onto the GENUINE EWSRV client surface.
 *
 * Transport (the proven EWCLIENT pattern, genuine/ewsrv/build_client.py +
 * genuine/pack_multi.py): the window-server session is a plain EUSER
 * RSessionBase --
 *     RSessionBase::CreateSession(_L("!E32WindowServer"),
 *                                 TVersion(1,0,KE32BuildVersionNumber),
 *                                 KMessageSlots)
 *     RSessionBase::SendReceive(op, args)  ->  DoSendReceive(op, &args)
 * and the window ops are the genuine CWsSession opcodes of
 * kernelhwsrv/kernel/eka/include/ws_std.h (EConsoleCreate / EConsoleSet /
 * EConsoleSetWindowPosAbs / EConsoleSetTitle / EConsoleSetSize /
 * EConsoleScreenSize / EConsoleDestroy) -- exactly what the genuine econs
 * RConsole (kernelhwsrv/kernel/eka/ewsrv/co_cli.cpp) sends and what
 * EWCLIENT.E32 passed in tools/r1_genuine_ewsrv_test.py.
 *
 * The four EUSER exports are imported from the prebuilt genuine EUSER.DLL
 * through PE2 IAT slots at codeBase+0x8000; user.mk seeds each slot with
 * --defsym and genuine/pack_multi.py writes the 1-based ordinals
 * (--ordinals 211,1547,120,915).  kernelhwsrv/ is never touched.
 *
 * Chunk 1 scope: connection / session / window-create / title / size.
 * Chunk 2 (THIS chunk) adds pixel drawing through the genuine EWSRV:
 * ws_draw_fill_rect() over the genuine CWsSession opcodes
 * EConsoleSetTextColors (21) + EConsoleSetCursorPosAbs (6) +
 * EConsoleWrite (10) -- the server-side CWsWindow::WriteCharacter blits each
 * glyph with CScreenDriver::Blit into the LFB, so a run of space (0x20)
 * cells with bg colour C renders as a solid w*8 x h*10 pixel rectangle.
 * No new EUSER ordinals are needed (all window ops ride the one
 * DoSendReceive import).  Chunk D0 (docs/GUISHELL_MIGRATION_MAP.md) adds
 * ws_text() with REAL glyphs (EConsoleWrite of UTF-16 chars, not 0x20
 * space-fill) plus the ws_button/ws_present composition; the event ring
 * (ws_get_event, chunk D1) reads genuine input via the RConsole::Read op
 * EConsoleRead (see ws_get_event below).  The old exec-based
 * user/wsclient.c stays in place for WSDL.E32 until the migration completes.
 */
#include "runtime.h"

/* Self-contained client ABI: this file used to include the retired winsrv
 * shim headers (the client-side API header plus the kernel variant ABI
 * header).  Both retire with the winsrv shim, so the fixed-width types, the
 * event record shape + WSE_* codes and the ws_* API prototypes are declared
 * locally here, with layouts identical to the retired ABI. */
#if !defined(__cplusplus)
typedef unsigned int       uint32_t;
typedef unsigned short     uint16_t;
typedef unsigned char      uint8_t;
#endif
#define WSE_MOVE  1u
#define WSE_DOWN  2u
#define WSE_UP    3u
#define WSE_KEY   4u
struct ws_event
    {
    volatile unsigned int type;     /* WSE_* */
    volatile int x, y;              /* MOVE/DOWN/UP: pointer position */
    volatile unsigned int key;      /* KEY: ASCII byte; DOWN/UP: button bit */
    volatile int target_wid;        /* -1 = broadcast / current focus */
    };
int  ws_open(void);
int  ws_window_create(int x, int y, int w, int h, const char* title);
int  ws_set_title(int wid, const char* title);
int  ws_set_size(int wid, int w, int h);
int  ws_screen_size(int* w, int* h);
int  ws_draw_fill_rect(int wid, int x, int y, int w, int h, uint32_t color);
void ws_close(void);
int  ws_window_count(void);
int  ws_server_ready(void);
uint32_t ws_rgb(uint8_t r, uint8_t g, uint8_t b);
unsigned char* ws_content(int wid);
int  ws_window_geom(int wid, int* x, int* y, int* w, int* h);
void ws_fill(int wid, int x, int y, int w, int h, uint32_t color);
void ws_text(int wid, int x, int y, const char* s, uint32_t fg);
void ws_button(int wid, int x, int y, int w, int h,
                const char* label, int hover);
void ws_present(int wid);
int  ws_get_event(struct ws_event* e);

/* ---- PE2 EUSER import slots (absolute symbols, seeded by user.mk) --------
 * slot 0 @ 0x408000  ord 211  _ZN12RSessionBase13CreateSessionERK7TDesC16RK8TVersioni
 * slot 1 @ 0x408004  ord 1547 _ZNK12RSessionBase13DoSendReceiveEiPK8TIpcArgs
 * slot 2 @ 0x408008  ord 120  _ZN11RHandleBase5CloseEv
 * slot 3 @ 0x40800c  ord 915  _ZN6RDebug6PrintfEPKcz
 * The kernel loader overwrites each slot with the genuine export address at
 * load time, so reading the slot at run time yields a callable pointer --
 * the same pattern as wsdl.c's wsdl_printf_slot. */
extern unsigned ws_e32_iat_connect;
extern unsigned ws_e32_iat_send;
extern unsigned ws_e32_iat_close;
extern unsigned ws_e32_iat_printf;

typedef int  (*ws_e32_t_connect)(void* aThis, const void* aName, const void* aVer, int aSlots);
typedef int  (*ws_e32_t_send)(const void* aThis, int aOp, const void* aArgs);
typedef void (*ws_e32_t_close)(void* aThis);

static int e32_connect(void* aThis, const void* aName, const void* aVer, int aSlots)
{
    return ((ws_e32_t_connect)(*(unsigned*)&ws_e32_iat_connect))(aThis, aName, aVer, aSlots);
}

static int e32_send(const void* aThis, int aOp, const void* aArgs)
{
    return ((ws_e32_t_send)(*(unsigned*)&ws_e32_iat_send))(aThis, aOp, aArgs);
}

static void e32_close(void* aThis)
{
    ((ws_e32_t_close)(*(unsigned*)&ws_e32_iat_close))(aThis);
}

/* Serial marker through genuine EUSER.DLL ord 915 (RDebug::Printf). */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)&ws_e32_iat_printf;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback   */
}

/* ---- genuine EKA2 ABI shapes (kernelhwsrv/, read-only) --------------------
 * TDesC16/TPtrC16, TDesC8/TPtrC8 (TPckgC), TPckg, TVersion, TIpcArgs, TPoint,
 * TSize and RHandleBase layouts exactly as in
 * kernelhwsrv/kernel/eka/include/e32cmn.h + e32des16.h + e32std.h. */
/* Genuine TDesC word 0 is iTypeLength = (TDesType<<28)|length
 * (e32des8_private.h KShiftDesType=28; u32std.h TDesType: EBufC=0, EPtrC=1,
 * EPtr=2, EBuf=3).  TDesC16::Ptr() (EUSER ord 1807) and the kernel's
 * K::ParseDesHeader dispatch on that nibble: EPtrC reads the pointer from
 * word 1, EPtr from word 2 (max in word 1).  A hand-built descriptor with a
 * PLAIN length has type EBufC, so Ptr() returns this+4 (the iPtr field
 * itself) and User::ValidateName sees garbage -> KErrBadName (-28).  All
 * pointer descriptors below therefore carry the EPtrC/EPtr type bits. */
#define WS_E32_EPTRC(len) (0x10000000u | (unsigned)(len))  /* EPtrC<<28|len */
#define WS_E32_EPTR(len)  (0x20000000u | (unsigned)(len))  /* EPtr<<28|len  */
struct ws_e32_desC16 { unsigned iLength; const unsigned short* iPtr; };
struct ws_e32_desC8  { unsigned iLength; const unsigned char*  iPtr; };
struct ws_e32_des8   { unsigned iLength; unsigned iMaxLength; unsigned char* iPtr; }; /* TPtr8 */
struct ws_e32_version{ signed char iMajor; signed char iMinor; short iBuild; };
struct ws_e32_ipcargs{ int iArgs[4]; int iFlags; };   /* KMaxMessageArguments=4 */
struct ws_e32_point  { int iX, iY; };
struct ws_e32_size   { int iWidth, iHeight; };
struct ws_e32_session{ int iHandle; };               /* RHandleBase/RSessionBase */

/* SConsoleKey (kernelhwsrv/kernel/eka/include/e32twin.h:53-60) -- the buffer
 * behind TConsoleKey (TPckgBuf<SConsoleKey>) that EConsoleRead writes back:
 * TKeyCode iCode, TUint iModifiers, TPoint iMousePos, TInt iType (TEventType:
 * EKeyPress=0, EMouseClick=1), TUint8 iPointerNumber.  sizeof = 24 with the
 * natural 4-byte alignment, same as the genuine -m32 build. */
struct ws_e32_console_key
    {
    unsigned iCode;                 /* TKeyCode: char code for printables */
    unsigned iModifiers;
    struct ws_e32_point iMousePos;
    int iType;                      /* TEventType / raw TRawEvent type   */
    unsigned char iPointerNumber;
    unsigned char iPad[3];
    };

/* CWsSession opcodes (kernelhwsrv/kernel/eka/include/ws_std.h, enum from 0). */
enum
    {
    WS_E32_OP_CONSOLE_CREATE      = 0,   /* EConsoleCreate                 */
    WS_E32_OP_CONSOLE_SET         = 1,   /* EConsoleSet(title, size)       */
    WS_E32_OP_CONSOLE_SET_POS_ABS = 4,   /* EConsoleSetWindowPosAbs        */
    WS_E32_OP_CONSOLE_SET_CURSOR_POS_ABS = 6,  /* EConsoleSetCursorPosAbs  */
    WS_E32_OP_CONSOLE_WRITE       = 10,  /* EConsoleWrite                  */
    WS_E32_OP_CONSOLE_READ        = 11,  /* EConsoleRead  (RConsole::Read) */
    WS_E32_OP_CONSOLE_READ_CANCEL = 12,  /* EConsoleReadCancel             */
    WS_E32_OP_CONSOLE_DESTROY     = 13,  /* EConsoleDestroy                */
    WS_E32_OP_CONSOLE_SET_TITLE   = 14,  /* EConsoleSetTitle               */
    WS_E32_OP_CONSOLE_SET_SIZE    = 15,  /* EConsoleSetSize                */
    WS_E32_OP_CONSOLE_SCREEN_SIZE = 17,  /* EConsoleScreenSize             */
    WS_E32_OP_CONSOLE_SET_TEXT_COLORS = 21 /* EConsoleSetTextColors(fg,bg) */
    };

/* TIpcArgs::TArgType values (e32cmn.h): 3 bits per slot, slot k at 3*k. */
#define WS_E32_EDESC_C16 7   /* EDesC16 = EFlagDes|EFlagConst|EFlag16Bit */
#define WS_E32_EDESC_C8  6   /* EDesC8  = EFlagDes|EFlagConst            */
#define WS_E32_EDES8     4   /* EDes8   = EFlagDes                       */

/* KE32WindowServer (e32twin.h) as a proper 2-byte TPtrC16, KW32 version
 * (ws_std.h: KW32MajorVersionNumber=1, KW32MinorVersionNumber=0,
 * e32ver.h: KE32BuildVersionNumber=4016) and KMessageSlots=3. */
static const unsigned short sServerName[] =
    { '!','E','3','2','W','i','n','d','o','w','S','e','r','v','e','r' };
#define WS_E32_SERVER_NAME_LEN 16u
static const struct ws_e32_desC16 sServerNameDesc =
    { WS_E32_EPTRC(WS_E32_SERVER_NAME_LEN), sServerName };
static const struct ws_e32_version sWsVersion = { 1, 0, 4016 };
#define WS_E32_MESSAGE_SLOTS 3

/* ---- state -------------------------------------------------------------- */
static struct ws_e32_session sSession;   /* iHandle; 0 = KNullHandle */
static int sOpen;
static int sWin = -1;                    /* 0 = the one console window */

/* ---- small helpers ------------------------------------------------------- */
static void ws_e32_args0(struct ws_e32_ipcargs* a)
{
    a->iArgs[0] = a->iArgs[1] = a->iArgs[2] = a->iArgs[3] = 0;
    a->iFlags = 0;
}

static int ws_e32_send(int aOp, struct ws_e32_ipcargs* aArgs)
{
    if (!sOpen) return -1;
    return e32_send(&sSession, aOp, aArgs);
}

#define WS_E32_TITLE_MAX 48u
static unsigned short sTitleBuf[WS_E32_TITLE_MAX + 1u];
static struct ws_e32_desC16 sTitleDesc;

/* ASCII title -> UTF-16 TPtrC16 view (what _L() produces under -m32). */
static void ws_e32_title_desc(const char* aTitle)
{
    unsigned n = 0;
    if (aTitle)
        while (n < WS_E32_TITLE_MAX && aTitle[n])
            { sTitleBuf[n] = (unsigned short)(unsigned char)aTitle[n]; ++n; }
    sTitleBuf[n] = 0;
    sTitleDesc.iLength = WS_E32_EPTRC(n);
    sTitleDesc.iPtr = sTitleBuf;
}

/* ---- wsclient API over the genuine EWSRV session ------------------------- */
int ws_open(void)
{
    int r;
    if (sOpen) return 0;
    sSession.iHandle = 0;                 /* KNullHandle */
    r = e32_connect(&sSession, &sServerNameDesc, &sWsVersion, WS_E32_MESSAGE_SLOTS);
    if (r != 0)                           /* KErrNone = 0 */
        { sSession.iHandle = 0; return r; }
    if (sSession.iHandle == 0) return -1;
    sOpen = 1;
    sWin = -1;
    return 0;
}

int ws_window_create(int x, int y, int w, int h, const char* title)
{
    struct ws_e32_ipcargs args;
    struct ws_e32_desC8 sizePckg;
    struct ws_e32_size size;
    struct ws_e32_desC8 posPckg;
    struct ws_e32_point pos;
    int r;

    if (!sOpen) return -1;
    if (w <= 0 || h <= 0) return -1;

    /* EConsoleCreate: create the default console window. */
    ws_e32_args0(&args);
    r = ws_e32_send(WS_E32_OP_CONSOLE_CREATE, &args);
    if (r != 0) return r;

    /* EConsoleSet(title, size): display it; w/h are character cells, the
     * genuine RConsole::Init semantics. */
    ws_e32_title_desc(title ? title : "WINDOW");
    size.iWidth = w; size.iHeight = h;
    sizePckg.iLength = WS_E32_EPTRC(sizeof(size));
    sizePckg.iPtr = (const unsigned char*)&size;
    args.iArgs[0] = (int)&sTitleDesc;
    args.iArgs[1] = (int)&sizePckg;
    args.iArgs[2] = args.iArgs[3] = 0;
    args.iFlags = (WS_E32_EDESC_C16 << 0) | (WS_E32_EDESC_C8 << 3);
    r = ws_e32_send(WS_E32_OP_CONSOLE_SET, &args);
    if (r != 0) return r;

    /* EConsoleSetWindowPosAbs: pixel position on the desktop. */
    pos.iX = x; pos.iY = y;
    posPckg.iLength = WS_E32_EPTRC(sizeof(pos));
    posPckg.iPtr = (const unsigned char*)&pos;
    args.iArgs[0] = (int)&posPckg;
    args.iArgs[1] = args.iArgs[2] = args.iArgs[3] = 0;
    args.iFlags = (WS_E32_EDESC_C8 << 0);
    r = ws_e32_send(WS_E32_OP_CONSOLE_SET_POS_ABS, &args);
    if (r != 0) return r;

    sWin = 0;                             /* single window id 0 */
    return 0;
}

int ws_set_title(int wid, const char* title)
{
    struct ws_e32_ipcargs args;
    if (!sOpen || wid != sWin) return -1;
    ws_e32_title_desc(title ? title : "");
    args.iArgs[0] = (int)&sTitleDesc;
    args.iArgs[1] = args.iArgs[2] = args.iArgs[3] = 0;
    args.iFlags = (WS_E32_EDESC_C16 << 0);
    return ws_e32_send(WS_E32_OP_CONSOLE_SET_TITLE, &args);
}

int ws_set_size(int wid, int w, int h)
{
    struct ws_e32_ipcargs args;
    struct ws_e32_desC8 sizePckg;
    struct ws_e32_size size;
    if (!sOpen || wid != sWin) return -1;
    if (w <= 0 || h <= 0) return -1;
    /* NOTE: this tree's genuine EWSRV answers EConsoleSetSize with
     * KErrNotSupported (-18) -- the SetSize call is compiled out in
     * kernelhwsrv/.../ewsrv/ws_main.cpp.  The IPC round trip is still real;
     * the marker records the genuine r value. */
    size.iWidth = w; size.iHeight = h;
    sizePckg.iLength = WS_E32_EPTRC(sizeof(size));
    sizePckg.iPtr = (const unsigned char*)&size;
    args.iArgs[0] = (int)&sizePckg;
    args.iArgs[1] = args.iArgs[2] = args.iArgs[3] = 0;
    args.iFlags = (WS_E32_EDESC_C8 << 0);
    return ws_e32_send(WS_E32_OP_CONSOLE_SET_SIZE, &args);
}

int ws_screen_size(int* w, int* h)
{
    struct ws_e32_ipcargs args;
    struct ws_e32_des8 sizePckg;          /* TPckg<TSize> as TDes8* */
    struct ws_e32_size size;
    int r;
    if (!sOpen) return -1;
    size.iWidth = 0; size.iHeight = 0;
    sizePckg.iLength = WS_E32_EPTR(sizeof(size));
    sizePckg.iMaxLength = sizeof(size);
    sizePckg.iPtr = (unsigned char*)&size;
    args.iArgs[0] = (int)&sizePckg;
    args.iArgs[1] = args.iArgs[2] = args.iArgs[3] = 0;
    args.iFlags = (WS_E32_EDES8 << 0);
    r = ws_e32_send(WS_E32_OP_CONSOLE_SCREEN_SIZE, &args);
    if (r == 0)
        {
        if (w) *w = size.iWidth;
        if (h) *h = size.iHeight;
        }
    return r;
}

/* ---- chunk 2: pixel drawing through the genuine EWSRV --------------------
 * The genuine EWSRV in this tree is the e32 text window server: a window is
 * a cell buffer (TText) + attribute buffer (ColorInformation{fg,bg}) and the
 * server rasterises every cell with CScreenDriver::Blit (8x10 glyph, LFB).
 * There is no RWindow/GC in kernelhwsrv's ws_std.h -- the genuine draw path
 * for a client IS the console op set:
 *     EConsoleSetTextColors(fg,bg)  (op 21, two plain TInt args)
 *     EConsoleSetCursorPosAbs(p)    (op 6,  TPckgC<TPoint>)
 *     EConsoleWrite(desC16)         (op 10, TPtrC16)
 * Writing space (0x20, glyph all-zero) cells with bg=colour fills each cell
 * solid -> a filled rectangle of w*8 x h*10 pixels on the LFB. */

/* Genuine standard SymbianOS 256-colour palette, verbatim from
 * kernelhwsrv/kernel/eka/drivers/edisp/epoc/generic/wd_generic.cpp
 * Palette256[] (read-only source; entries are 0x00BBGGRR).  The server maps
 * TColorIndex -> pixel through this table, so the client picks the nearest
 * index for a 0x00RRGGBB RGB value. */
static const unsigned sPalette256[256] =
    {
    0x00000000u, 0x00000033u, 0x00000066u, 0x00000099u, 0x000000ccu, 0x000000ffu, 0x00003300u, 0x00003333u,
    0x00003366u, 0x00003399u, 0x000033ccu, 0x000033ffu, 0x00006600u, 0x00006633u, 0x00006666u, 0x00006699u,
    0x000066ccu, 0x000066ffu, 0x00009900u, 0x00009933u, 0x00009966u, 0x00009999u, 0x000099ccu, 0x000099ffu,
    0x0000cc00u, 0x0000cc33u, 0x0000cc66u, 0x0000cc99u, 0x0000ccccu, 0x0000ccffu, 0x0000ff00u, 0x0000ff33u,
    0x0000ff66u, 0x0000ff99u, 0x0000ffccu, 0x0000ffffu, 0x00330000u, 0x00330033u, 0x00330066u, 0x00330099u,
    0x003300ccu, 0x003300ffu, 0x00333300u, 0x00333333u, 0x00333366u, 0x00333399u, 0x003333ccu, 0x003333ffu,
    0x00336600u, 0x00336633u, 0x00336666u, 0x00336699u, 0x003366ccu, 0x003366ffu, 0x00339900u, 0x00339933u,
    0x00339966u, 0x00339999u, 0x003399ccu, 0x003399ffu, 0x0033cc00u, 0x0033cc33u, 0x0033cc66u, 0x0033cc99u,
    0x0033ccccu, 0x0033ccffu, 0x0033ff00u, 0x0033ff33u, 0x0033ff66u, 0x0033ff99u, 0x0033ffccu, 0x0033ffffu,
    0x00660000u, 0x00660033u, 0x00660066u, 0x00660099u, 0x006600ccu, 0x006600ffu, 0x00663300u, 0x00663333u,
    0x00663366u, 0x00663399u, 0x006633ccu, 0x006633ffu, 0x00666600u, 0x00666633u, 0x00666666u, 0x00666699u,
    0x006666ccu, 0x006666ffu, 0x00669900u, 0x00669933u, 0x00669966u, 0x00669999u, 0x006699ccu, 0x006699ffu,
    0x0066cc00u, 0x0066cc33u, 0x0066cc66u, 0x0066cc99u, 0x0066ccccu, 0x0066ccffu, 0x0066ff00u, 0x0066ff33u,
    0x0066ff66u, 0x0066ff99u, 0x0066ffccu, 0x0066ffffu, 0x00111111u, 0x00222222u, 0x00444444u, 0x00555555u,
    0x00777777u, 0x00000011u, 0x00000022u, 0x00000044u, 0x00000055u, 0x00000077u, 0x00001100u, 0x00002200u,
    0x00004400u, 0x00005500u, 0x00007700u, 0x00110000u, 0x00220000u, 0x00440000u, 0x00550000u, 0x00770000u,
    0x00880000u, 0x00aa0000u, 0x00bb0000u, 0x00dd0000u, 0x00ee0000u, 0x00008800u, 0x0000aa00u, 0x0000bb00u,
    0x0000dd00u, 0x0000ee00u, 0x00000088u, 0x000000aau, 0x000000bbu, 0x000000ddu, 0x000000eeu, 0x00888888u,
    0x00aaaaaau, 0x00bbbbbbu, 0x00ddddddu, 0x00eeeeeeu, 0x00990000u, 0x00990033u, 0x00990066u, 0x00990099u,
    0x009900ccu, 0x009900ffu, 0x00993300u, 0x00993333u, 0x00993366u, 0x00993399u, 0x009933ccu, 0x009933ffu,
    0x00996600u, 0x00996633u, 0x00996666u, 0x00996699u, 0x009966ccu, 0x009966ffu, 0x00999900u, 0x00999933u,
    0x00999966u, 0x00999999u, 0x009999ccu, 0x009999ffu, 0x0099cc00u, 0x0099cc33u, 0x0099cc66u, 0x0099cc99u,
    0x0099ccccu, 0x0099ccffu, 0x0099ff00u, 0x0099ff33u, 0x0099ff66u, 0x0099ff99u, 0x0099ffccu, 0x0099ffffu,
    0x00cc0000u, 0x00cc0033u, 0x00cc0066u, 0x00cc0099u, 0x00cc00ccu, 0x00cc00ffu, 0x00cc3300u, 0x00cc3333u,
    0x00cc3366u, 0x00cc3399u, 0x00cc33ccu, 0x00cc33ffu, 0x00cc6600u, 0x00cc6633u, 0x00cc6666u, 0x00cc6699u,
    0x00cc66ccu, 0x00cc66ffu, 0x00cc9900u, 0x00cc9933u, 0x00cc9966u, 0x00cc9999u, 0x00cc99ccu, 0x00cc99ffu,
    0x00cccc00u, 0x00cccc33u, 0x00cccc66u, 0x00cccc99u, 0x00ccccccu, 0x00ccccffu, 0x00ccff00u, 0x00ccff33u,
    0x00ccff66u, 0x00ccff99u, 0x00ccffccu, 0x00ccffffu, 0x00ff0000u, 0x00ff0033u, 0x00ff0066u, 0x00ff0099u,
    0x00ff00ccu, 0x00ff00ffu, 0x00ff3300u, 0x00ff3333u, 0x00ff3366u, 0x00ff3399u, 0x00ff33ccu, 0x00ff33ffu,
    0x00ff6600u, 0x00ff6633u, 0x00ff6666u, 0x00ff6699u, 0x00ff66ccu, 0x00ff66ffu, 0x00ff9900u, 0x00ff9933u,
    0x00ff9966u, 0x00ff9999u, 0x00ff99ccu, 0x00ff99ffu, 0x00ffcc00u, 0x00ffcc33u, 0x00ffcc66u, 0x00ffcc99u,
    0x00ffccccu, 0x00ffccffu, 0x00ffff00u, 0x00ffff33u, 0x00ffff66u, 0x00ffff99u, 0x00ffffccu, 0x00ffffffu
    };

/* RGB (0x00RRGGBB) -> nearest genuine palette index. */
static unsigned ws_e32_rgb_to_index(uint32_t aRgb)
{
    unsigned tr = (aRgb >> 16) & 0xffu, tg = (aRgb >> 8) & 0xffu, tb = aRgb & 0xffu;
    unsigned best = 0, bestd = 0xffffffffu, i;
    for (i = 0; i < 256u; ++i)
        {
        unsigned e = sPalette256[i];
        unsigned pr = e & 0xffu, pg = (e >> 8) & 0xffu, pb = (e >> 16) & 0xffu;
        unsigned dr = tr > pr ? tr - pr : pr - tr;
        unsigned dg = tg > pg ? tg - pg : pg - tg;
        unsigned db = tb > pb ? tb - pb : pb - tb;
        unsigned d = dr * dr + dg * dg + db * db;
        if (d < bestd) { bestd = d; best = i; }
        }
    return best;
}

#define WS_E32_ROW_MAX 128u
static unsigned short sRowBuf[WS_E32_ROW_MAX];

int ws_draw_fill_rect(int wid, int x, int y, int w, int h, uint32_t color)
{
    struct ws_e32_ipcargs args;
    struct ws_e32_desC8 posPckg;
    struct ws_e32_point pos;
    struct ws_e32_desC16 rowDesc;
    unsigned idx;
    int r, row, k;

    if (!sOpen || wid != sWin) return -1;
    if (w <= 0 || h <= 0) return -1;
    if ((unsigned)w > WS_E32_ROW_MAX) w = (int)WS_E32_ROW_MAX;

    /* EConsoleSetTextColors(fg,bg): plain ints, iFlags=0 (EInt slots). */
    idx = ws_e32_rgb_to_index(color);
    ws_e32_args0(&args);
    args.iArgs[0] = (int)idx;                 /* fg (irrelevant for spaces) */
    args.iArgs[1] = (int)idx;                 /* bg -> solid glyph fill     */
    r = ws_e32_send(WS_E32_OP_CONSOLE_SET_TEXT_COLORS, &args);
    if (r != 0) return r;

    for (k = 0; k < w; ++k) sRowBuf[k] = 0x20; /* space: all-zero glyph */
    rowDesc.iLength = WS_E32_EPTRC((unsigned)w);
    rowDesc.iPtr = sRowBuf;

    for (row = 0; row < h; ++row)
        {
        /* EConsoleSetCursorPosAbs(TPoint(x, y+row)): TPckgC<TPoint>. */
        pos.iX = x; pos.iY = y + row;
        posPckg.iLength = WS_E32_EPTRC(sizeof(pos));
        posPckg.iPtr = (const unsigned char*)&pos;
        args.iArgs[0] = (int)&posPckg;
        args.iArgs[1] = args.iArgs[2] = args.iArgs[3] = 0;
        args.iFlags = (WS_E32_EDESC_C8 << 0);
        r = ws_e32_send(WS_E32_OP_CONSOLE_SET_CURSOR_POS_ABS, &args);
        if (r != 0) return r;

        /* EConsoleWrite(row of w spaces): TPtrC16. */
        args.iArgs[0] = (int)&rowDesc;
        args.iArgs[1] = args.iArgs[2] = args.iArgs[3] = 0;
        args.iFlags = (WS_E32_EDESC_C16 << 0);
        r = ws_e32_send(WS_E32_OP_CONSOLE_WRITE, &args);
        if (r != 0) return r;
        }
    return 0;
}

/* ---- chunk D0: real text through the genuine EWSRV -------------------------
 * Same three ops as ws_draw_fill_rect -- EConsoleSetTextColors (21, two
 * plain TInt args: iFlags=0), EConsoleSetCursorPosAbs (6, TPckgC<TPoint>)
 * and EConsoleWrite (10, TPtrC16) -- but the written chars are real UTF-16
 * glyphs instead of 0x20 spaces.  Server side (ws_main.cpp EConsoleWrite ->
 * CWsWindow::Write -> WriteCharacter) blits every glyph with the current
 * fg/bg via CScreenDriver::Blit and advances the cursor (Right()), so one
 * EConsoleWrite per row chunk renders the whole string left-to-right.
 * ASCII is widened to UTF-16; control chars are sanitised to 0x20 because
 * the genuine CWsWindow::Write treats <0x20 as control actions (beep, BS,
 * tab, CR, LF), never as glyphs. */
static int ws_e32_text_bg(int wid, int x, int y, const char* s,
                          uint32_t fg, uint32_t bg)
{
    struct ws_e32_ipcargs args;
    struct ws_e32_desC8 posPckg;
    struct ws_e32_point pos;
    struct ws_e32_desC16 textDesc;
    unsigned n, i, total = 0;
    int r;

    if (!sOpen || wid != sWin || !s) return -1;
    if (!s[0]) return 0;

    /* EConsoleSetTextColors(fg,bg): plain ints, iFlags=0 (EInt slots). */
    ws_e32_args0(&args);
    args.iArgs[0] = (int)ws_e32_rgb_to_index(fg);
    args.iArgs[1] = (int)ws_e32_rgb_to_index(bg);
    r = ws_e32_send(WS_E32_OP_CONSOLE_SET_TEXT_COLORS, &args);
    if (r != 0) return r;

    /* EConsoleSetCursorPosAbs(TPoint(x, y)): TPckgC<TPoint>. */
    pos.iX = x; pos.iY = y;
    posPckg.iLength = WS_E32_EPTRC(sizeof(pos));
    posPckg.iPtr = (const unsigned char*)&pos;
    args.iArgs[0] = (int)&posPckg;
    args.iArgs[1] = args.iArgs[2] = args.iArgs[3] = 0;
    args.iFlags = (WS_E32_EDESC_C8 << 0);
    r = ws_e32_send(WS_E32_OP_CONSOLE_SET_CURSOR_POS_ABS, &args);
    if (r != 0) return r;

    /* EConsoleWrite(TPtrC16): chunks of WS_E32_ROW_MAX cells; the server
     * cursor keeps advancing across writes, so chunks continue the line. */
    for (i = 0; s[i]; )
        {
        n = 0;
        while (s[i] && n < WS_E32_ROW_MAX)
            {
            unsigned char c = (unsigned char)s[i++];
            sRowBuf[n++] = (c < 0x20u || c == 0x7fu) ? 0x20u : (unsigned short)c;
            }
        textDesc.iLength = WS_E32_EPTRC(n);
        textDesc.iPtr = sRowBuf;
        args.iArgs[0] = (int)&textDesc;
        args.iArgs[1] = args.iArgs[2] = args.iArgs[3] = 0;
        args.iFlags = (WS_E32_EDESC_C16 << 0);
        r = ws_e32_send(WS_E32_OP_CONSOLE_WRITE, &args);
        if (r != 0) return r;
        total += n;
        }
    return (int)total;                    /* chars written */
}

void ws_close(void)
{
    struct ws_e32_ipcargs args;
    if (!sOpen) return;
    if (sWin >= 0)
        {
        ws_e32_args0(&args);
        (void)ws_e32_send(WS_E32_OP_CONSOLE_DESTROY, &args);
        sWin = -1;
        }
    e32_close(&sSession);                 /* RHandleBase::Close */
    sSession.iHandle = 0;
    sOpen = 0;
}

/* ---- wsclient helpers ---------------------------------------------------- */
int ws_window_count(void) { return 1; }

int ws_server_ready(void) { return sOpen && sSession.iHandle != 0; }

uint32_t ws_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/* ---- chunk D0+D1 landed: ws_text genuine, ws_get_event genuine ------------
 * ws_fill is genuine (ws_draw_fill_rect, chunk 2) and ws_text/ws_button are
 * genuine (ws_e32_text_bg above, chunk D0 of GUISHELL_MIGRATION_MAP.md);
 * ws_present is a no-op on the E32 path because the genuine EWSRV blits
 * synchronously on every EConsoleWrite.  The event read (ws_get_event,
 * chunk D1) rides the genuine RConsole::Read op EConsoleRead (see below) --
 * note it BLOCKS until an event arrives (genuine sync Read semantics), so
 * polling callers see 1 only when an event is really there. */
unsigned char* ws_content(int wid)
{
    (void)wid;
    return 0;                             /* no shared buffer on the E32 path */
}

int ws_window_geom(int wid, int* x, int* y, int* w, int* h)
{
    if (!sOpen || wid != sWin) return -1;
    if (x) *x = 0;
    if (y) *y = 0;
    if (w) *w = 0;
    if (h) *h = 0;
    return 0;
}

void ws_fill(int wid, int x, int y, int w, int h, uint32_t color)
{
    /* Chunk 2: genuine fill through EWSRV (cells, not the old pixel buffer).
     * w/h are character cells here, matching the genuine console window. */
    (void)ws_draw_fill_rect(wid, x, y, w, h, color);
}

void ws_text(int wid, int x, int y, const char* s, uint32_t fg)
{
    /* Chunk D0: real glyphs over the genuine console ops 21/6/10 (see
     * ws_e32_text_bg above) -- no new IAT slots.  The ws_text ABI
     * carries only fg, so bg is black (0x000000 -> palette index 0). */
    (void)ws_e32_text_bg(wid, x, y, s, fg, 0x00000000u);
}

void ws_button(int wid, int x, int y, int w, int h, const char* label, int hover)
{
    /* Chunk D0 composition: face = ws_draw_fill_rect cell block, label =
     * centred ws_e32_text_bg with bg = face colour so the glyphs blend in.
     * w/h are character cells on the E32 path (same convention as ws_fill). */
    uint32_t face, ink;
    int n = 0, tx, ty;
    if (!sOpen || wid != sWin || w <= 0 || h <= 0) return;
    face = hover ? ws_rgb(0x00, 0x80, 0xC0) : ws_rgb(0x80, 0x80, 0x80);
    ink  = hover ? ws_rgb(0xff, 0xff, 0x00) : ws_rgb(0xff, 0xff, 0xff);
    (void)ws_draw_fill_rect(wid, x, y, w, h, face);
    if (label)
        {
        while (label[n] && n < (int)WS_E32_ROW_MAX) ++n;
        tx = x + (w - n) / 2; if (tx < x + 1) tx = x + 1;
        ty = y + (h - 1) / 2; if (ty > y + h - 1) ty = y + h - 1;
        (void)ws_e32_text_bg(wid, tx, ty, label, ink, face);
        }
}

void ws_present(int wid)
{
    /* Chunk D0: no-op on the E32 path -- the genuine EWSRV blits every cell
     * synchronously inside EConsoleWrite, there is no dirty-flag/composite
     * step like the old exec 273 WSO_SET_DIRTY. */
    (void)wid;
}

/* ---- chunk D1: genuine input read ------------------------------------------
 * The genuine client-side event read is RConsole::Read (this tree's EWSRV is
 * the e32 text window server, so its client class is RConsole, not
 * RWsSession -- kernelhwsrv/kernel/eka/include/e32twin.h:95):
 *     RConsole::Read(TConsoleKey&)  (genuine/ewsrv/patched/co_cli.cpp:202):
 *         SendReceive(CWsSession::EConsoleRead, TIpcArgs((TDes8*)&aKeystroke))
 * Server side (ws_main.cpp:439-451): EnqueReadRequest parks the message
 * (delayCompletion=ETrue) until CWsWindow::KeyPress/MouseMove/InformMouse
 * queues an SWsKey into iKQueue and DrainReadRequest() does
 * WriteL(0, TPckgC<SConsoleKey>) + Complete(KErrNone) (ws_win.cpp:2417-2443).
 * So the synchronous DoSendReceive (ord 1547) BLOCKS until an input event
 * arrives -- exactly the genuine synchronous RConsole::Read semantics.
 * Injected keys reach that path via the KBINJ bridge (docs/
 * EWSRV_INPUT_INJECTION.md + KEYMAP_FIX_STATUS.md: PS/2 IRQ1 -> TRawEvent
 * EKeyDown/EKeyUp -> CEvent::RunL -> TranslateKey -> CWsWindow::KeyPress ->
 * QueueTopWindowKey -> DrainAllReadRequests).  No new EUSER ordinals: the
 * read rides the one DoSendReceive import. */
int ws_get_event(struct ws_event* e)
{
    struct ws_e32_ipcargs args;
    struct ws_e32_des8 keyDesc;           /* TConsoleKey seen as TDes8*  */
    struct ws_e32_console_key key;        /* server WriteL(0, ...) sink  */
    int r;

    if (!e) return 0;
    e->type = 0; e->x = 0; e->y = 0; e->key = 0; e->target_wid = -1;
    if (!sOpen || sWin < 0) return 0;     /* no session / no window      */

    /* EConsoleRead, slot 0 = (TDes8*)&TConsoleKey -> EDes8 arg type. */
    keyDesc.iLength = WS_E32_EPTR(sizeof(key));
    keyDesc.iMaxLength = sizeof(key);
    keyDesc.iPtr = (unsigned char*)&key;
    ws_e32_args0(&args);
    args.iArgs[0] = (int)&keyDesc;
    args.iArgs[1] = args.iArgs[2] = args.iArgs[3] = 0;
    args.iFlags = (WS_E32_EDES8 << 0);
    r = ws_e32_send(WS_E32_OP_CONSOLE_READ, &args);   /* blocks for a key */
    if (r != 0) return r;                 /* negative = server error     */

    /* Map the genuine SConsoleKey onto the ws_event ABI (WSE_* codes,
     * declared locally at the top of this file). */
    e->target_wid = sWin;
    e->x = key.iMousePos.iX;
    e->y = key.iMousePos.iY;
    switch (key.iType)
        {
        case 0:                           /* TEventType::EKeyPress (e32twin.h:32) */
            e->type = WSE_KEY;
            e->key = key.iCode;           /* TKeyCode: char code for printables */
            break;
        case 1:                           /* TEventType::EMouseClick */
            e->type = WSE_DOWN;
            e->key = key.iPointerNumber;
            break;
        case 3:                           /* raw TRawEvent::EKeyDown */
        case 4:                           /* raw TRawEvent::EKeyUp   */
            e->type = WSE_KEY;
            e->key = key.iCode;           /* raw mode: scan code */
            break;
        default:                          /* raw pointer/button events */
            e->type = WSE_MOVE;
            e->key = key.iPointerNumber;
            break;
        }
    return 1;                             /* event copied */
}

/* ---- test E32 (user.mk target WSE32TEST.E32, -DWS_E32_TEST_MAIN) --------- */
#ifdef WS_E32_TEST_MAIN
static unsigned t_app(char* p, unsigned k, const char* s)
{
    while (*s) p[k++] = *s++;
    return k;
}

static unsigned t_dec(char* p, unsigned k, int v)
{
    char t[12];
    int n = 0;
    unsigned mag = (unsigned)v;
    if (v < 0) { p[k++] = '-'; mag = 0u - mag; }
    if (!mag) t[n++] = '0';
    while (mag) { t[n++] = (char)('0' + mag % 10u); mag /= 10u; }
    while (n) p[k++] = t[--n];
    return k;
}

int app_main(void)
{
    char line[96];
    unsigned n;
    int r, w = 0, h = 0, wid;

    marker("WSE32: app_main started");

    r = ws_open();
    n = t_app(line, 0, "WSE32: Connect r="); n = t_dec(line, n, r); line[n] = 0;
    marker(line);
    if (r != 0) return 1;

    r = ws_screen_size(&w, &h);
    n = t_app(line, 0, "WSE32: ScreenSize r="); n = t_dec(line, n, r);
    n = t_app(line, n, " "); n = t_dec(line, n, w);
    n = t_app(line, n, "x"); n = t_dec(line, n, h);
    line[n] = 0;
    marker(line);

    wid = ws_window_create(16, 16, 44, 12, "WS_E32 TEST");
    n = t_app(line, 0, "WSE32: Create wid="); n = t_dec(line, n, wid); line[n] = 0;
    marker(line);
    if (wid < 0)
        { ws_close(); return 1; }

    r = ws_set_title(wid, "WS_E32 TITLE2");
    n = t_app(line, 0, "WSE32: SetTitle r="); n = t_dec(line, n, r); line[n] = 0;
    marker(line);

    r = ws_set_size(wid, 48, 14);
    n = t_app(line, 0, "WSE32: SetSize r="); n = t_dec(line, n, r); line[n] = 0;
    marker(line);

    ws_close();
    marker("WSE32: Close done");
    marker("WSE32: DONE");
    return 0;
}
#endif /* WS_E32_TEST_MAIN */
