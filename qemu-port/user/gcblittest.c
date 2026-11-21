/*
 * gcblittest.c -- PE2 E32 proof of the real GC_BLIT bulk-display opcode.
 *
 * Allocates a 640x400 RGBA8888 frame in the 4 MiB chunk-backed user heap,
 * fills it with a deterministic gradient (white 4px border), publishes its
 * physical base/size through gui_shared.blit_src_phys/size (low RAM is
 * identity-mapped, so the virtual pointer is the physical address), then
 * submits a ONE-command exec-251 display list carrying GC_BLIT.  The kernel
 * validates the descriptor and writes all 1,024,000 bytes to the VBE LFB in
 * ring 0 (variant/qemu_gui_exec.cpp) -- no new executive slot is used.
 *
 * The blit is submitted a second time after the DONE marker so the console's
 * LFB text mirror cannot smear the gradient before the QEMU screendump.  The
 * program then sleeps forever; the test harness owns the VM lifetime.
 *
 * Serial markers use the genuine EUSER.DLL ordinal 915 (RDebug::Printf),
 * bound by E32Run from the PE2 IAT slot gcblittest_printf_slot at 0x408000.
 */
#include "runtime.h"
#include "guishost.h"

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern unsigned int gcblittest_printf_slot;
typedef void (*NbPrintf)(const char*, ...);

static void marker(const char* text)
{
    unsigned int fn = *(volatile unsigned int*)&gcblittest_printf_slot;
    if (fn != 915u && fn != 0u) ((NbPrintf)fn)(text);  /* EUSER ord 915 */
    else print(text);                                  /* no EUSER bound */
}

/* ---- tiny formatters (no hosted libc) ---- */
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
static unsigned hex32(char* p, unsigned k, unsigned v)
{
    const char* h = "0123456789abcdef";
    k = append(p, k, "0x");
    for (int i = 28; i >= 0; i -= 4) p[k++] = h[(v >> i) & 0xfu];
    return k;
}

/* ---- frame geometry: one DOOM-class 640x400 RGBA frame ---- */
#define BLIT_W        640u
#define BLIT_H        400u
#define BLIT_SIZE     (BLIT_W * BLIT_H * 4u)   /* 1,024,000 bytes */

static struct gui_shared* gSh;

static void put_pixel(volatile unsigned char* p, unsigned x, unsigned y,
                      unsigned r, unsigned g, unsigned b)
{
    unsigned off = (y * BLIT_W + x) * 4u;
    p[off]      = (unsigned char)r;
    p[off + 1u] = (unsigned char)g;
    p[off + 2u] = (unsigned char)b;
    p[off + 3u] = 0xffu;
}

/* Deterministic gradient: R=x&0xff, G=y&0xff, B=(x+y)&0xff, plus a white
 * 4px border.  Centre pixel (320,200) is exactly (64,200,8). */
static void fill_frame(volatile unsigned char* p)
{
    unsigned x, y, t;
    for (y = 0; y < BLIT_H; ++y)
        for (x = 0; x < BLIT_W; ++x)
            put_pixel(p, x, y, x & 0xffu, y & 0xffu, (x + y) & 0xffu);
    for (x = 0; x < BLIT_W; ++x)
        for (t = 0; t < 4u; ++t)
            { put_pixel(p, x, t, 255u,255u,255u);
              put_pixel(p, x, BLIT_H - 1u - t, 255u,255u,255u); }
    for (y = 0; y < BLIT_H; ++y)
        for (t = 0; t < 4u; ++t)
            { put_pixel(p, t, y, 255u,255u,255u);
              put_pixel(p, BLIT_W - 1u - t, y, 255u,255u,255u); }
}

/* One GC_BLIT command at (x,y) covering w*h, source offset 0. */
static void submit_blit(unsigned x, unsigned y, unsigned w, unsigned h)
{
    struct gc_blit_cmd* b;
    b = (struct gc_blit_cmd*)&gSh->cmds[0];
    b->type       = GC_BLIT;
    b->x          = x;
    b->y          = y;
    b->w          = w;
    b->h          = h;
    b->src_offset = 0u;
    exec2(QE_EXEC_GUI_DRAW, 1u, 0u);
}

int app_main(void)
{
    char line[160];
    unsigned k;
    int shb;
    volatile unsigned char* buf;

    marker("GB: BLIT TEST READY");

    buf = (volatile unsigned char*)malloc(BLIT_SIZE);
    if (!buf)
        {
        marker("GB: BLIT FAIL malloc");
        return 1;
        }

    k = append(line, 0, "GB: BLIT buf=");
    k = hex32(line, k, (unsigned)buf);
    k = append(line, k, " size=");
    k = dec(line, k, BLIT_SIZE);
    line[k] = 0;
    marker(line);

    fill_frame(buf);

    shb = exec2(QE_EXEC_GUI_DRAW, 0u, 0u);   /* arg1==0 -> shared buffer */
    if (shb <= 0)
        {
        marker("GB: BLIT FAIL no shared buffer (exec 251)");
        return 1;
        }
    gSh = (struct gui_shared*)shb;
    if (gSh->magic != GUI_MAGIC || gSh->version != GUI_VERSION)
        {
        marker("GB: BLIT FAIL shared magic");
        return 1;
        }

    /* Low RAM is identity-mapped U/S=1 in user PDs, so the virtual pointer is
     * the physical address the kernel must read from (0x1000..0x80000000). */
    gSh->blit_src_phys = (unsigned)buf;
    gSh->blit_src_size = BLIT_SIZE;

    k = append(line, 0, "GB: BLIT src=");
    k = hex32(line, k, gSh->blit_src_phys);
    k = append(line, k, " size=");
    k = dec(line, k, gSh->blit_src_size);
    k = append(line, k, " w=640 h=400");
    line[k] = 0;
    marker(line);

    submit_blit(0u, 0u, BLIT_W, BLIT_H);
    marker("GB: BLIT submitted cmds=1");
    marker("GB: BLIT TEST DONE");

    /* Re-submit as the final action: overwrites any console text the markers
     * above may have mirrored into the LFB, leaving a clean gradient frame. */
    submit_blit(0u, 0u, BLIT_W, BLIT_H);

    for (;;) sleep_ms(100);
    return 0;
}
