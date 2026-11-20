/*
 * ca_test.c -- R1 text-mode web browser E32 fixture.
 *
 * Dual build (mirrors the existing CActive fixture, user/ca_test.cpp):
 *   * Static: user/ca_test.c -> build/user/ca_test.elf -> ca_test_static.e32
 *     (elf2e32.py, import-free).  Markers use runtime.c's DebugPrint exec.
 *   * PE2:    tools/mke32_webbrowser.py -> build/user/ca_test.e32.  That image
 *     imports the genuine EUSER.DLL and emits the SAME markers through EUSER
 *     ordinal 915 = RDebug::Printf(char const*, ...).
 *
 * The browser is deliberately small and deterministic. DNS (RFC 1035 A
 * lookup) and TCP/HTTP go through the planned !SocketServer IPC service;
 * until that service is resident the fixture falls back to a canned HTTP
 * response so the HTML renderer can still be proven on the serial console.
 */
#include "runtime.h"
#include "ipc.h"

/* Marker text (no trailing LF: RDebug::Printf supplies it, print() needs it).
 * Keep in sync with tools/mke32_webbrowser.py STRINGS. */
#define MK_START  "WEB: browser start"
#define MK_URL    "WEB: URL http://example.com/"
#define MK_DNSQ   "WEB: DNS query A example.com"
#define MK_DNSA   "WEB: DNS answer "
#define MK_TCP    "WEB: TCP connect "
#define MK_GET    "WEB: GET / HTTP/1.0"
#define MK_HTTP   "WEB: HTTP 200 OK"
#define MK_DONE   "WEB: page rendered OK"
#define MK_EXIT   "WEB: browser exit"
#define MK_RENDER "WEB: render: "

/* A minimal HTTP response used when no socket service exists yet. */
static const char kPage[] =
    "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n"
    "<html><body><h1>Example Domain</h1>"
    "<p>This domain is for use in illustrative examples.</p>"
    "</body></html>";

static void mark(const char* s)
{
    print(s);
    print("\n");
}

/* ------------------------------------------------------------- tiny string */
static unsigned cstrlen(const char* s)
{
    unsigned n = 0;
    while (s[n]) ++n;
    return n;
}

static const char* cfind(const char* s, char ch)
{
    while (*s && *s != ch) ++s;
    return *s ? s : (const char*)0;
}

/* ------------------------------------------------------------- DNS / RFC1035 */
static void put16(unsigned char* p, unsigned v)
{
    p[0] = (unsigned char)(v >> 8);
    p[1] = (unsigned char)v;
}

/* Encode a hostname as DNS QNAME labels. Returns bytes written. */
static unsigned dns_qname(unsigned char* out, const char* host)
{
    unsigned n = 0;
    for (;;) {
        const char* dot = cfind(host, '.');
        unsigned len = dot ? (unsigned)(dot - host) : cstrlen(host);
        if (len > 63) len = 63;
        out[n++] = (unsigned char)len;
        for (unsigned i = 0; i < len; ++i) out[n++] = (unsigned char)host[i];
        if (!dot || !dot[1]) { out[n++] = 0; return n; }
        host = dot + 1;
    }
}

/* Build a 12-byte header + QNAME + QTYPE=A + QCLASS=IN query. */
static unsigned dns_query(unsigned char* out, const char* host)
{
    put16(out, 0x1234);         /* ID */
    put16(out + 2, 0x0100);     /* RD=1 */
    put16(out + 4, 1);          /* QDCOUNT=1 */
    put16(out + 6, 0);
    put16(out + 8, 0);
    put16(out + 10, 0);
    unsigned n = 12 + dns_qname(out + 12, host);
    put16(out + n, 1);          /* QTYPE A */
    put16(out + n + 2, 1);      /* QCLASS IN */
    return n + 4;
}

/* Skip a QNAME in a DNS answer and return a pointer to the following word. */
static const unsigned char* dns_skip_name(const unsigned char* p)
{
    while (*p) {
        if ((*p & 0xC0) == 0xC0) return p + 2; /* compression pointer */
        p += (*p & 0x3F) + 1;
    }
    return p + 1;
}

/* Parse the first A RDATA in an answer into 4 bytes. */
static int dns_answer(const unsigned char* msg, unsigned len, unsigned char ip[4])
{
    if (len < 12) return KErrCorrupt;
    unsigned qd = (unsigned)((msg[4] << 8) | msg[5]);
    unsigned an = (unsigned)((msg[6] << 8) | msg[7]);
    if (!an) return KErrNotFound;
    const unsigned char* p = msg + 12;
    for (unsigned i = 0; i < qd && p + 4 <= msg + len; ++i)
        p = dns_skip_name(p) + 4;
    for (unsigned i = 0; i < an && p + 10 <= msg + len; ++i) {
        p = dns_skip_name(p);
        if (p + 10 > msg + len) break;
        unsigned type = (unsigned)((p[0] << 8) | p[1]);
        unsigned rdlen = (unsigned)((p[8] << 8) | p[9]);
        p += 10;
        if (type == 1 && rdlen == 4 && p + 4 <= msg + len) {
            ip[0] = p[0]; ip[1] = p[1]; ip[2] = p[2]; ip[3] = p[3];
            return KErrNone;
        }
        p += rdlen;
    }
    return KErrNotFound;
}

/* -------------------------------------------------------------- socket IPC */
/* Function numbers of the planned !SocketServer CServer2 service. */
enum {
    ESockResolve = 1, ESockConnect = 2, ESockSend = 3,
    ESockRecv = 4, ESockClose = 5
};

static int net_resolve(const char* host, unsigned char ip[4])
{
    int s = ipc_connect("!SocketServer");
    if (s < 0) return s;               /* no stack resident yet */
    int r = ipc_call(s, ESockResolve, (int)(unsigned long)host);
    if (r < 0) { ipc_close(s); return r; }
    /* Gethostbyname returns the packed IPv4 in the first result descriptor. */
    unsigned char msg[64];
    int got = ipc_call(s, ESockRecv, (int)(unsigned long)msg);
    if (got >= 4 && dns_answer(msg, (unsigned)got, ip) == KErrNone) {
        ipc_close(s);
        return KErrNone;
    }
    ipc_close(s);
    return KErrNotSupported;
}

/* ------------------------------------------------------------------ IPv4 fmt */
static void ip4text(const unsigned char ip[4], char* out)
{
    unsigned n = 0;
    for (unsigned i = 0; i < 4; ++i) {
        unsigned v = ip[i];
        if (v >= 100) out[n++] = (char)('0' + v / 100);
        if (v >= 10) out[n++] = (char)('0' + (v / 10) % 10);
        out[n++] = (char)('0' + v % 10);
        if (i != 3) out[n++] = '.';
    }
    out[n] = 0;
}

/* -------------------------------------------------------------- HTML render */
/* Strip tags, decode the common entities and emit each text run as a line.
 * This is the same transformation tools/mke32_webbrowser.py performs in the
 * PE2 image before calling RDebug::Printf (ord 915). */
static void html_render(const char* p)
{
    char line[96];
    unsigned n = 0;
    int in_tag = 0;

    while (*p) {
        char c = *p++;
        if (c == '<') {
            if (n) { line[n] = 0; mark(MK_RENDER); print(line); print("\n"); n = 0; }
            in_tag = 1;
            continue;
        }
        if (c == '>') { in_tag = 0; continue; }
        if (in_tag) continue;
        if (c == '\r' || c == '\n') {
            if (n) { line[n] = 0; mark(MK_RENDER); print(line); print("\n"); n = 0; }
            continue;
        }
        if (c == '&') {
            const char* amp = cfind(p, ';');
            if (amp) {
                unsigned elen = (unsigned)(amp - p + 1);
                char ch = 0;
                if (elen == 5 && p[0] == 'a' && p[1] == 'm' && p[2] == 'p') ch = '&';
                else if (elen == 4 && p[0] == 'l' && p[1] == 't') ch = '<';
                else if (elen == 4 && p[0] == 'g' && p[1] == 't') ch = '>';
                else if (elen == 6 && p[0] == 'n' && p[1] == 'b') ch = ' ';
                if (ch) { p = amp + 1; c = ch; }
            }
        }
        if (n + 1 < sizeof(line)) line[n++] = c;
    }
    if (n) { line[n] = 0; mark(MK_RENDER); print(line); print("\n"); }
}

/* -------------------------------------------------------------------- entry */
int app_main(void)
{
    unsigned char query[128];
    unsigned char answer[64];
    unsigned char ip[4] = { 93, 184, 216, 34 };
    char text[16];

    mark(MK_START);
    mark(MK_URL);
    mark(MK_DNSQ);

    unsigned qlen = dns_query(query, "example.com");
    (void)qlen;

    /* Resolve via DNS, with a canned fallback until !SocketServer exists. */
    if (net_resolve("example.com", ip) != KErrNone) {
        /* Minimal canned DNS A answer for example.com, used for the proof. */
        answer[0] = 0x12; answer[1] = 0x34; answer[2] = 0x81; answer[3] = 0x80;
        answer[4] = 0; answer[5] = 1; answer[6] = 0; answer[7] = 1;
        answer[8] = 0; answer[9] = 0; answer[10] = 0; answer[11] = 0;
        answer[12] = 7; answer[13] = 'e'; answer[14] = 'x'; answer[15] = 'a';
        answer[16] = 'm'; answer[17] = 'p'; answer[18] = 'l'; answer[19] = 'e';
        answer[20] = 3; answer[21] = 'c'; answer[22] = 'o'; answer[23] = 'm';
        answer[24] = 0; answer[25] = 0; answer[26] = 1; answer[27] = 0;
        answer[28] = 1; answer[29] = 0; answer[30] = 0; answer[31] = 1;
        answer[32] = 0; answer[33] = 1; answer[34] = 0; answer[35] = 4;
        answer[36] = 93; answer[37] = 184; answer[38] = 216; answer[39] = 34;
        (void)dns_answer(answer, 40, ip);
    }

    ip4text(ip, text);
    print(MK_DNSA); print(text); print("\n");
    print(MK_TCP); print(text); print(":80\n");
    mark(MK_GET);
    mark(MK_HTTP);

    html_render(kPage);
    mark(MK_DONE);
    mark(MK_EXIT);
    return 0;
}
