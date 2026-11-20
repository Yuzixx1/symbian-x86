/*
 * dnsresolv.c -- DNS resolver E32 (DNSRES.E32) for the x86 Symbian-like port.
 *
 * RFC 1035 A-record resolver with a real UDP/53 datagram path:
 *   DNS query -> IPv4 header -> UDP header (src 49152, dst 53) -> transport.
 * The transport is udp_exchange(): a !SocketServer IPC service carries the
 * framed datagram when present; a built-in loopback DNS responder otherwise
 * closes the loop so encode/parse are exercised end to end in QEMU today.
 *
 * IPC service "DnsResolverServer":
 *   -1 EConnect     -> accept (version in arg0), KErrNone
 *   -2 EDisConnect  -> KErrNone
 *    0 EResolve     -> slot 0 hostname (8-bit), slot 1 4-byte IPv4 result
 *    1 ESelfTest    -> run built-in lookups, KErrNone
 *    2 EStop        -> KErrNone, then stop serving
 *
 * Serial markers use genuine EUSER.DLL ordinal 915 (RDebug::Printf), bound by
 * E32Run from the PE2 IAT at 0x408000; DebugPrint is the unbound fallback.
 */
#include "runtime.h"
#include "ipc.h"

/* ------------------------------------------------------------------ EUSER IAT */
/* Defined with --defsym=dns_printf_slot=0x408000 in user.mk.  elf2e32.py
   --euser-import 915 seeds that dword with the 1-based ordinal; the loader
   replaces it with the resolved RDebug::Printf export address before resume. */
extern unsigned int dns_printf_slot;
typedef void (*DnsPrintf)(const char*, ...);

static void marker(const char* text)
{
    unsigned int fn = *(volatile unsigned int*)&dns_printf_slot;
    if (fn != 915u && fn != 0u) ((DnsPrintf)fn)(text);
    else print(text);                        /* no EUSER.DLL bound */
}

/* ---------------------------------------------------------------- tiny helpers */
static void scopy(void* dst, const void* src, unsigned n)
{
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
}
static int scmp(const char* a, const char* b)
{
    while (*a && *a == *b) { ++a; ++b; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}
static void put16(unsigned char* p, unsigned v)
    { p[0] = (unsigned char)(v >> 8); p[1] = (unsigned char)v; }
static unsigned get16(const unsigned char* p)
    { return ((unsigned)p[0] << 8) | p[1]; }
static void put32(unsigned char* p, unsigned v)
{
    p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);  p[3] = (unsigned char)v;
}

/* ------------------------------------------------------------ DNS wire format */
#define DNS_HEADER   12
#define DNS_TYPE_A   1
#define DNS_CLASS_IN 1
#define DNS_PORT     53u
#define UDP_SPORT    49152u

static int dns_build_query(const char* name, unsigned char* q, unsigned cap,
                           unsigned* id_out)
{
    unsigned pos = DNS_HEADER;
    unsigned id = random32() & 0xffffu;
    const char* p = name;
    if (!*name) return -6;                       /* KErrArgument */
    put16(q + 0, (unsigned short)id);
    put16(q + 2, 0x0100u);                       /* RD: recursion desired */
    put16(q + 4, 1);                             /* QDCOUNT = 1 */
    put16(q + 6, 0); put16(q + 8, 0); put16(q + 10, 0);
    while (*p)
        {
        const char* start = p;
        while (*p && *p != '.') ++p;
        unsigned len = (unsigned)(p - start);
        if (!len || len > 63 || pos + 1 + len + 5 > cap) return -6;
        q[pos++] = (unsigned char)len;
        scopy(q + pos, start, len); pos += len;
        if (*p == '.') ++p;
        }
    q[pos++] = 0;                                /* root label */
    put16(q + pos, DNS_TYPE_A);  pos += 2;
    put16(q + pos, DNS_CLASS_IN); pos += 2;
    *id_out = id;
    return (int)pos;
}

/* Skip a (possibly compressed) domain name; return 0 on malformed input. */
static unsigned skip_name(const unsigned char* r, unsigned len, unsigned pos)
{
    unsigned hops = 0;
    for (;;)
        {
        if (pos >= len || hops++ > 64) return 0;
        unsigned c = r[pos];
        if ((c & 0xc0u) == 0xc0u)
            {
            /* A compression pointer is a complete 2-byte NAME field: skipping
               the name must stop AFTER the pointer, not at the end of the
               name it points to (otherwise TYPE/CLASS/RDATA are misread). */
            return (pos + 1 < len) ? pos + 2 : 0u;
            }
        if (!c) return pos + 1;                  /* root label */
        pos += c + 1;
        }
}

/* Parse a response: matching ID, QR=1, RCODE=0, first A/IN answer -> ip[4]. */
static int dns_parse_answer(const unsigned char* r, unsigned len, unsigned id,
                            unsigned char ip[4])
{
    if (len < DNS_HEADER) return -20;            /* KErrCorrupt */
    if (get16(r) != id) return -1;               /* KErrNotFound (wrong ID) */
    unsigned flags = get16(r + 2);
    if (!(flags & 0x8000u) || (flags & 0x000fu)) return -1;
    unsigned qd = get16(r + 4), an = get16(r + 6), pos = DNS_HEADER;
    for (unsigned i = 0; i < qd; ++i)
        {
        pos = skip_name(r, len, pos);
        if (!pos || pos + 4 > len) return -20;
        pos += 4;                                /* QTYPE + QCLASS */
        }
    for (unsigned i = 0; i < an; ++i)
        {
        pos = skip_name(r, len, pos);
        if (!pos || pos + 10 > len) return -20;
        unsigned type = get16(r + pos), cls = get16(r + pos + 2);
        unsigned rdlen = get16(r + pos + 8);
        pos += 10;
        if (type == DNS_TYPE_A && cls == DNS_CLASS_IN && rdlen >= 4 &&
            pos + 4 <= len)
            {
            scopy(ip, r + pos, 4);
            return 0;
            }
        if (pos + rdlen > len) return -20;
        pos += rdlen;                            /* skip CNAME/TXT/... */
        }
    return -1;
}

/* ------------------------------------------------------- UDP/IPv4 framing (53) */
static unsigned checksum(const unsigned char* p, unsigned len, unsigned sum)
{
    while (len > 1) { sum += get16(p); p += 2; len -= 2; }
    if (len) sum += (unsigned)p[0] << 8;
    while (sum >> 16) sum = (sum & 0xffffu) + (sum >> 16);
    return sum;
}

static int udp_wrap(const unsigned char* dns, unsigned dlen,
                    const unsigned char dst[4], unsigned char* frame, unsigned cap)
{
    static const unsigned char src[4] = { 10, 0, 0, 2 };
    unsigned ulen = 8 + dlen, need = 20 + ulen;
    if (need > cap) return -11;                  /* KErrOverflow */
    frame[0] = 0x45; frame[1] = 0;
    put16(frame + 2, (unsigned short)need);
    put16(frame + 4, 0x1234); put16(frame + 6, 0);
    frame[8] = 64; frame[9] = 17; put16(frame + 10, 0);
    scopy(frame + 12, src, 4); scopy(frame + 16, dst, 4);
    put16(frame + 10, (unsigned short)~checksum(frame, 20, 0));
    put16(frame + 20, (unsigned short)UDP_SPORT);
    put16(frame + 22, (unsigned short)DNS_PORT);
    put16(frame + 24, (unsigned short)ulen); put16(frame + 26, 0);
    scopy(frame + 28, dns, dlen);
    unsigned sum = checksum(src, 4, 0);
    sum = checksum(dst, 4, sum);
    sum = checksum(frame + 9, 1, sum);
    sum = checksum(frame + 20, ulen, sum);
    unsigned csum = ~sum & 0xffffu;
    put16(frame + 26, (unsigned short)(csum ? csum : 0xffffu));
    return (int)need;
}

static int udp_unwrap(const unsigned char* frame, unsigned len,
                      unsigned char* dns, unsigned cap)
{
    if (len < 28 || (frame[0] >> 4) != 4 || frame[9] != 17) return -20;
    unsigned ihl = (unsigned)(frame[0] & 0x0f) * 4, ulen = get16(frame + 24);
    if (ihl < 20 || ulen < 8 || ihl + ulen > len) return -20;
    if (get16(frame + 22) != DNS_PORT) return -20;
    unsigned dlen = ulen - 8;
    if (dlen > cap) return -11;
    scopy(dns, frame + ihl + 8, dlen);
    return (int)dlen;
}

/* ---------------------------------------------------- loopback DNS responder */
struct HostEntry { const char* name; unsigned char ip[4]; };
static const struct HostEntry kHosts[] = {
    { "localhost",        { 127,   0,   0,   1 } },
    { "example.com",      {  93, 184, 216,  34 } },
    { "sybian.local",     {  10,   0,   0,   1 } },
    { "www.sybian.local", {  10,   0,   0,   2 } },
};

static int loopback_answer(const unsigned char* req, unsigned rlen,
                           unsigned char* ans, unsigned cap)
{
    if (rlen < DNS_HEADER + 5 || get16(req + 4) != 1) return -20;
    unsigned pos = skip_name(req, rlen, DNS_HEADER);
    if (!pos || pos + 4 > rlen) return -20;
    char name[128];
    unsigned n = 0, p = DNS_HEADER;
    while (p < pos)
        {
        unsigned l = req[p++];
        if ((l & 0xc0u) == 0xc0u || !l) break;
        for (unsigned i = 0; i < l && n + 1 < sizeof(name); ++i)
            {
            char c = (char)req[p++];
            if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
            name[n++] = c;
            }
        name[n++] = '.';
        }
    if (n) --n;
    name[n] = 0;
    const struct HostEntry* hit = 0;
    for (unsigned i = 0; i < sizeof(kHosts) / sizeof(kHosts[0]); ++i)
        if (!scmp(name, kHosts[i].name)) { hit = &kHosts[i]; break; }
    if (!hit) return -1;
    if (cap < pos + 4 + 16) return -11;
    scopy(ans, req, pos + 4);                    /* header + question */
    put16(ans + 2, 0x8180u);                     /* QR|RD|RA, RCODE=0 */
    put16(ans + 6, 1);                           /* ANCOUNT = 1 */
    put16(ans + 8, 0); put16(ans + 10, 0);
    unsigned o = pos + 4;
    put16(ans + o, 0xc00cu); o += 2;             /* NAME -> question */
    put16(ans + o, DNS_TYPE_A); o += 2;
    put16(ans + o, DNS_CLASS_IN); o += 2;
    put32(ans + o, 60); o += 4;                  /* TTL */
    put16(ans + o, 4); o += 2;                   /* RDLENGTH */
    scopy(ans + o, hit->ip, 4); o += 4;
    return (int)o;
}

/* ------------------------------------------------------------- UDP transport */
static int udp_exchange(const unsigned char* req, unsigned reqlen,
                        unsigned char* rsp, unsigned rcap)
{
    static const unsigned char server[4] = { 8, 8, 8, 8 };
    unsigned char tx[512], rx[512];
    int flen = udp_wrap(req, reqlen, server, tx, sizeof(tx));
    if (flen < 0) return flen;
    marker("DNSRES: UDP 10.0.0.2:49152 -> 8.8.8.8:53");

    /* Real netstack path: !SocketServer UdpSendToRecvFrom (function 12). */
    int sock = ipc_connect("!SocketServer");
    if (sock >= 0)
        {
        struct IpcArgs a = {{ (unsigned)(0x10000000u | (unsigned)flen),
                              (unsigned)tx, (unsigned)sizeof(rx),
                              (unsigned)rx }, 0};
        int r = ipc_call_args(sock, 12, &a);
        ipc_close(sock);
        if (r >= 0 && (unsigned)r <= rcap)
            { scopy(rsp, rx, (unsigned)r); return r; }
        }

    /* Loopback: feed the framed datagram through the responder, then unwrap
       the reply exactly as a NIC receive path would. */
    marker("DNSRES: no SocketServer; loopback UDP responder");
    unsigned char payload[512], answer[512];
    int plen = udp_unwrap(tx, (unsigned)flen, payload, sizeof(payload));
    if (plen < 0) return plen;
    int alen = loopback_answer(payload, (unsigned)plen, answer, sizeof(answer));
    if (alen < 0) return alen;
    int frlen = udp_wrap(answer, (unsigned)alen, server, rx, sizeof(rx));
    if (frlen < 0) return frlen;
    return udp_unwrap(rx, (unsigned)frlen, rsp, rcap);
}

/* ------------------------------------------------------------------ resolver */
static int dns_resolve(const char* name, unsigned char ip[4])
{
    unsigned char query[512], reply[512];
    unsigned id = 0;
    int qlen = dns_build_query(name, query, sizeof(query), &id);
    if (qlen < 0) return qlen;
    marker("DNSRES: built A/IN query (RFC 1035)");
    int rlen = udp_exchange(query, (unsigned)qlen, reply, sizeof(reply));
    if (rlen < 0) { marker("DNSRES: UDP exchange failed"); return rlen; }
    marker("DNSRES: UDP reply received");
    int r = dns_parse_answer(reply, (unsigned)rlen, id, ip);
    if (r < 0) marker("DNSRES: no A record in reply");
    return r;
}

static void fmt_ip(const unsigned char ip[4], char out[16])
{
    unsigned n = 0;
    for (unsigned i = 0; i < 4; ++i)
        {
        unsigned v = ip[i], d = 100;
        while (d > 1 && v < d) d /= 10;
        while (d) { out[n++] = (char)('0' + v / d); v %= d; d /= 10; }
        if (i != 3) out[n++] = '.';
        }
    out[n] = 0;
}

static int dns_selftest(void)
{
    static const char* names[] = { "localhost", "example.com", "sybian.local" };
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        {
        unsigned char ip[4] = { 0, 0, 0, 0 };
        char line[96], iptext[16];
        unsigned n = 0;
        const char pre[] = "DNSRES: resolve ";
        for (unsigned k = 0; pre[k]; ++k) line[n++] = pre[k];
        for (unsigned k = 0; names[i][k]; ++k) line[n++] = names[i][k];
        for (unsigned k = 0; k < 4; ++k) line[n++] = ' ';
        line[n++] = '-'; line[n++] = '>'; line[n++] = ' ';
        int r = dns_resolve(names[i], ip);
        if (r == 0) fmt_ip(ip, iptext);
        else { const char fail[] = "FAIL"; scopy(iptext, fail, sizeof(fail)); }
        for (unsigned k = 0; iptext[k]; ++k) line[n++] = iptext[k];
        line[n] = 0;
        marker(line);
        }
    return 0;
}

/* ---------------------------------------------------------------- IPC service */
int app_main(void)
{
    marker("DNSRES: DNS resolver E32 start (EUSER ord 915)");
    dns_selftest();

    /* Kernel object names are capped (KFNameMax) -- keep it short so the
       server really registers. */
    int server = ipc_server_create("DnsResolver");
    if (server < 0) { marker("DNSRES: IPC server create FAILED"); return 1; }
    marker("DNSRES: DnsResolverServer ready (EResolve=0)");
    static int session_anchor;
    int running = 1;
    while (running)
        {
        struct IpcMessage m;
        if (ipc_receive(server, &m)) break;
        switch (m.function)
            {
            case -1:                             /* EConnect */
                if (m.args[0] != 1) ipc_complete(&m, -6);
                else { ipc_accept(&m, &session_anchor); ipc_complete(&m, 0); }
                break;
            case -2:                             /* EDisConnect */
                ipc_complete(&m, 0);
                break;
            case 0:                              /* EResolve */
                {
                unsigned char name[128], ip[4] = { 0, 0, 0, 0 };
                int length = ipc_length(&m, 0), result = -6;
                if (length > 0 && length < (int)sizeof(name))
                    {
                    int got = ipc_read8(&m, 0, name, (unsigned)length, 0);
                    if (got > 0)
                        {
                        name[got < (int)sizeof(name) ? got : (int)sizeof(name) - 1] = 0;
                        result = dns_resolve((const char*)name, ip);
                        if (result == 0) ipc_write8(&m, 1, ip, 4, 0);
                        }
                    }
                ipc_complete(&m, result);
                break;
                }
            case 1:                              /* ESelfTest */
                dns_selftest();
                ipc_complete(&m, 0);
                break;
            case 2:                              /* EStop */
                ipc_complete(&m, 0);
                running = 0;
                break;
            default:
                ipc_complete(&m, -49);           /* KErrNotSupported */
                break;
            }
        }
    ipc_close(server);
    marker("DNSRES: resolver stopped");
    return 0;
}
