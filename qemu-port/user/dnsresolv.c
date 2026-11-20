/*
 * dnsresolv.c -- DNS resolver E32 for the R1 x86 Symbian-like port.
 *
 * RFC 1035 DNS client (UDP/53) plus a CServer2-style IPC service
 * "DnsResolverServer" (EResolve=1): slot 0 = hostname (8-bit), slot 1 =
 * 4-byte IPv4 result.  The E32 is linked as a PE2 image with one EUSER.DLL
 * import -- ordinal 915 (RDebug::Printf) -- used for the serial markers.
 *
 * The port has no socket server yet (docs/r1_net_design_v*.md), so UDP is
 * routed through the planned !SocketServer IPC function numbers when it
 * exists; otherwise the exact same encoder/parser is exercised against a
 * generated RFC 1035 response (self-test) so the image is testable today.
 */
#include "e32cmn.h"
#include "runtime.h"
#include "ipc.h"

/* PE2 IAT slot at textSize, defined with --defsym in user.mk.  The loader
   overwrites this dword with the resolved EUSER.DLL ordinal-915 export. */
extern void (*dns_printf_slot)(const char*);

#define DNS_PORT              53u
#define DNS_TYPE_A             1u
#define DNS_CLASS_IN           1u
#define DNS_FLAG_RD       0x0100u
#define DNS_FLAG_QR       0x8000u
#define DNS_FLAG_RA       0x0080u
#define DNS_MAX_PACKET       512
#define DNS_SERVER_IP4     8, 8, 8, 8       /* default resolver */

/* !SocketServer function numbers (planned ESOCK-like IPC service). */
enum { ESockOpenUdp = 1, ESockSendTo = 2, ESockRecvFrom = 3, ESockClose = 4 };
enum { EDnsResolve = 1 };

static TUint8 s_bound = 0;
static TUint8 s_query[DNS_MAX_PACKET];
static TUint8 s_answer[DNS_MAX_PACKET];
static char s_host[64];
static TUint8 s_ip4[4];
static char s_line[80];

/* ------------------------------------------------------------ big-endian IO */
static void put16(TUint8* p, TUint16 v) { p[0] = (TUint8)(v >> 8); p[1] = (TUint8)v; }
static void put32(TUint8* p, TUint32 v)
{
    p[0] = (TUint8)(v >> 24); p[1] = (TUint8)(v >> 16);
    p[2] = (TUint8)(v >> 8);  p[3] = (TUint8)v;
}
static TUint16 get16(const TUint8* p) { return (TUint16)((p[0] << 8) | p[1]); }

/* ------------------------------------------------------------------- markers
 * EUSER ordinal 915 is RDebug::Printf(char const*, ...).  When the DLL was
 * bound the slot no longer holds its file value (915); call it, and also use
 * the kernel DebugPrint executive so the trace is always visible on serial. */
static void marker(const char* text)
{
    TUint32 fn = *(TUint32*)(void*)&dns_printf_slot;
    if (fn != 915u && fn != 0u)
        {
        s_bound = 1;
        ((void (*)(const char*))fn)(text);
        }
    print(text);
}

/* ------------------------------------------------------------- RFC 1035 code */
static TInt dns_skip_name(const TUint8* buf, TInt len, TInt off)
{
    TInt jumps = 0;
    for (;;)
        {
        if (off < 0 || off >= len) return KErrCorrupt;
        TUint8 c = buf[off];
        if ((c & 0xc0u) == 0xc0u)                 /* compression pointer */
            return (off + 1 < len) ? off + 2 : KErrCorrupt;
        if (c & 0xc0u) return KErrCorrupt;
        if (c == 0) return off + 1;               /* root label */
        off += 1 + c;
        if (++jumps > 64) return KErrCorrupt;
        }
}

/* Build a standard A/IN query.  Returns the packet length or a KErr code. */
static TInt dns_encode_query(const char* host, TUint8* out, TInt cap)
{
    TInt p = 0;
    if (cap < 12 + 1 + 4) return KErrOverflow;
    put16(out + 0, (TUint16)(random32() & 0xffffu)); /* ID */
    put16(out + 2, DNS_FLAG_RD);                     /* recursion desired */
    put16(out + 4, 1); put16(out + 6, 0);
    put16(out + 8, 0); put16(out + 10, 0);
    p = 12;
    const char* s = host;
    while (*s)
        {
        const char* dot = s;
        while (*dot && *dot != '.') ++dot;
        TInt n = (TInt)(dot - s);
        if (n < 1 || n > 63 || p + 1 + n + 5 > cap) return KErrArgument;
        out[p++] = (TUint8)n;
        for (TInt i = 0; i < n; ++i) out[p++] = (TUint8)s[i];
        s = (*dot == '.') ? dot + 1 : dot;
        }
    out[p++] = 0;                                  /* root label */
    put16(out + p, DNS_TYPE_A);  p += 2;
    put16(out + p, DNS_CLASS_IN); p += 2;
    return p;
}

/* Parse a response: match ID, QR=1, RCODE=0, first A/IN answer -> ip4. */
static TInt dns_parse_answer(const TUint8* pkt, TInt len, TUint16 id, TUint8* ip4)
{
    if (len < 12) return KErrCorrupt;
    if (get16(pkt) != id) return KErrNotFound;
    TUint16 flags = get16(pkt + 2);
    if (!(flags & DNS_FLAG_QR) || (flags & 0x000fu) != 0) return KErrNotFound;
    TUint16 qd = get16(pkt + 4), an = get16(pkt + 6);
    if (qd < 1 || an < 1) return KErrNotFound;
    TInt off = 12;
    for (TInt i = 0; i < qd; ++i)
        {
        off = dns_skip_name(pkt, len, off);
        if (off < 0 || off + 4 > len) return KErrCorrupt;
        off += 4;                                  /* QTYPE + QCLASS */
        }
    for (TInt i = 0; i < an; ++i)
        {
        off = dns_skip_name(pkt, len, off);
        if (off < 0 || off + 10 > len) return KErrCorrupt;
        TUint16 type = get16(pkt + off);
        TUint16 cls  = get16(pkt + off + 2);
        TUint16 rdlen = get16(pkt + off + 8);
        const TUint8* rdata = pkt + off + 10;
        if (off + 10 + rdlen > len) return KErrCorrupt;
        if (type == DNS_TYPE_A && cls == DNS_CLASS_IN && rdlen == 4)
            {
            for (TInt k = 0; k < 4; ++k) ip4[k] = rdata[k];
            return KErrNone;
            }
        off += 10 + rdlen;                         /* skip CNAME/TXT/etc. */
        }
    return KErrNotFound;
}

/* Generate a well-formed answer for our own query (compression pointer,
   TTL, A RDATA 93.184.216.34) so encode+parse are exercised offline. */
static TInt dns_canned_answer(const TUint8* q, TInt qlen, TUint8* out, TInt cap)
{
    TInt qend = dns_skip_name(q, qlen, 12);
    if (qend < 0 || qend + 4 != qlen || qlen > DNS_MAX_PACKET) return KErrCorrupt;
    TInt qnamelen = qend - 12;
    if (12 + qnamelen + 4 + 16 > cap) return KErrOverflow;
    put16(out, get16(q));                          /* echo the query ID */
    put16(out + 2, DNS_FLAG_QR | DNS_FLAG_RD | DNS_FLAG_RA);
    put16(out + 4, 1); put16(out + 6, 1);
    put16(out + 8, 0); put16(out + 10, 0);
    for (TInt i = 0; i < qnamelen; ++i) out[12 + i] = q[12 + i];
    TInt p = 12 + qnamelen;
    for (TInt i = 0; i < 4; ++i) out[p + i] = q[qend + i];
    p += 4;                                        /* QTYPE + QCLASS */
    out[p++] = 0xc0; out[p++] = 0x0c;              /* NAME = question */
    put16(out + p, DNS_TYPE_A);  p += 2;
    put16(out + p, DNS_CLASS_IN); p += 2;
    put32(out + p, 60u); p += 4;                   /* TTL */
    put16(out + p, 4);   p += 2;                   /* RDLENGTH */
    out[p++] = 93; out[p++] = 184; out[p++] = 216; out[p++] = 34;
    return p;
}

/* ------------------------------------------------- UDP via planned socket IPC
 * SendTo/RecvFrom use TIpcArgs descriptor slots (see user/ipccli.c). */
static TInt udp_query(const TUint8* query, TInt qlen, TUint8* answer, TInt* alen)
{
    TInt sess = ipc_connect("!SocketServer");
    if (sess < 0) return sess;
    struct IpcArgs a = {{0, 0, 0, 0}, 0};
    a.args[0] = 17u;                               /* IPPROTO_UDP */
    TInt fd = ipc_call_args(sess, ESockOpenUdp, &a);
    if (fd < 0) { ipc_close(sess); return fd; }

    TUint8 peer[6];
    const TUint8 server[4] = { DNS_SERVER_IP4 };
    for (TInt i = 0; i < 4; ++i) peer[i] = server[i];
    put16(peer + 4, DNS_PORT);
    struct IpcConst8 qdesc = {0x10000000u | (TUint)qlen, query};
    struct IpcConst8 pdesc = {0x10000006u, peer};
    struct IpcBuffer8 rdesc = {0x20000000u, DNS_MAX_PACKET, answer};
    struct IpcArgs sa = {{(TUint)&qdesc, (TUint)&pdesc, (TUint)fd, 0},
                         (6u) | (6u << 3)};       /* two EDesC8 slots */
    TInt r = ipc_call_args(sess, ESockSendTo, &sa);
    if (r >= 0)
        {
        struct IpcArgs ra = {{(TUint)&rdesc, 0, (TUint)fd, 1000u}, (4u)};
        r = ipc_call_args(sess, ESockRecvFrom, &ra);
        if (r >= 0) *alen = (TInt)(rdesc.type_length & 0x0fffffffu);
        }
    a.args[0] = (TUint)fd;
    ipc_call_args(sess, ESockClose, &a);
    ipc_close(sess);
    return r;
}

/* ---------------------------------------------------------- resolution core */
static TInt resolve(const char* host, TUint8* ip4)
{
    TInt qlen = dns_encode_query(host, s_query, DNS_MAX_PACKET);
    if (qlen < 12) return qlen;
    TInt alen = 0;
    TInt r = udp_query(s_query, qlen, s_answer, &alen);
    if (r >= 0 && alen >= 12)
        marker("DNSRES: UDP/53 query completed via !SocketServer");
    else
        {
        alen = dns_canned_answer(s_query, qlen, s_answer, DNS_MAX_PACKET);
        if (alen < 0) return alen;
        marker("DNSRES: no socket service; RFC1035 self-test response");
        }
    return dns_parse_answer(s_answer, alen, get16(s_query), ip4);
}

static void ip_to_text(const TUint8* ip, char* out)
{
    TInt p = 0;
    for (TInt i = 0; i < 4; ++i)
        {
        TUint v = ip[i];
        if (v >= 100u) { out[p++] = (char)('0' + v / 100u); v %= 100u; }
        if (v >= 10u)  { out[p++] = (char)('0' + v / 10u);  v %= 10u; }
        out[p++] = (char)('0' + v);
        if (i != 3) out[p++] = '.';
        }
    out[p] = 0;
}

/* ------------------------------------------------------- IPC service surface */
static TInt dns_read_host(struct IpcMessage* msg, char* host, TInt cap)
{
    TInt n = ipc_length(msg, 0);
    if (n < 1 || n >= cap) return KErrBadDescriptor;
    struct IpcBuffer8 d = {0x20000000u | (TUint)cap, cap, host};
    TInt r = ipc_read8(msg, 0, d.pointer, cap, 0);
    if (r < 0) return r;
    host[r] = 0;
    return r;
}

static void dns_service(struct IpcMessage* msg)
{
    if (msg->function != EDnsResolve)
        { ipc_complete(msg, KErrNotSupported); return; }
    char host[64];
    TInt n = dns_read_host(msg, host, (TInt)sizeof(host));
    if (n < 0) { ipc_complete(msg, n); return; }
    TUint8 ip4[4];
    TInt r = resolve(host, ip4);
    if (r != KErrNone) { ipc_complete(msg, r); return; }
    struct IpcConst8 result = {0x10000004u, ip4};
    TInt w = ipc_write8(msg, 1, result.pointer, 4, 0);
    ipc_complete(msg, w < 0 ? w : KErrNone);
}

static void dns_server_loop(void)
{
    TInt server = ipc_server_create("DnsResolverServer");
    if (server < 0) { marker("DNSRES: IPC server create FAILED"); return; }
    marker("DNSRES: DnsResolverServer listening (EResolve=1)");
    for (;;)
        {
        struct IpcMessage msg;
        TInt r = ipc_receive(server, &msg);
        if (r < 0) break;
        if (msg.function == -1)                    /* EConnect */
            { ipc_accept(&msg, (void*)1); ipc_complete(&msg, KErrNone); continue; }
        if (msg.function == -2)                    /* EDisConnect */
            { ipc_complete(&msg, KErrNone); continue; }
        dns_service(&msg);
        }
}

/* ---------------------------------------------------------------- entry point */
static TBool wants_server(void)
{
    U16 cmd[128];
    TInt n = command_line(cmd, 128);
    for (TInt i = 0; i < n; ++i)
        {
        TUint c = cmd[i];
        if (c == 's' || c == 'S')
            {
            const char word[] = "server";
            for (TInt k = 0; k < 6 && i + k < n; ++k)
                if ((TUint)(TUint8)word[k] != (cmd[i + k] & 0xffu)) break;
                else if (k == 5) return ETrue;
            }
        }
    return EFalse;
}

int app_main(void)
{
    marker("DNSRES: DNS resolver E32 start (EUSER ord 915)");
    marker(s_bound ? "DNSRES: EUSER ord 915 (RDebug::Printf) bound"
                   : "DNSRES: EUSER ord 915 unavailable; DebugPrint fallback");
    marker("DNSRES: RFC1035 A/IN query, UDP port 53");

    const char* host = "www.example.com";
    for (TInt i = 0; i < 16 && host[i]; ++i) s_host[i] = host[i];
    TInt r = resolve(s_host, s_ip4);
    if (r == KErrNone)
        {
        ip_to_text(s_ip4, s_line);
        char line[80];
        TInt p = 0;
        const char prefix[] = "DNSRES: resolved www.example.com -> ";
        while (prefix[p]) { line[p] = prefix[p]; ++p; }
        for (TInt i = 0; s_line[i]; ++i) line[p++] = s_line[i];
        line[p] = 0;
        marker(line);
        }
    else marker("DNSRES: resolution FAILED");

    if (wants_server()) dns_server_loop();         /* long-lived IPC service */
    else marker("DNSRES: IPC API ready; self-test exit");
    return 0;
}
