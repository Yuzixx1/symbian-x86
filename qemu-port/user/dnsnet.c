/*
 * dnsnet.c -- PE2 E32 REAL DNS client over the kernel<->userland e1000 bridge.
 *
 * Unlike dnsresolv.c (loopback responder / !SocketServer), this program puts
 * genuine Ethernet frames on QEMU SLIRP through the shared-memory netbridge:
 *   ARP for the 10.0.2.2 gateway -> learn its MAC,
 *   build Ethernet/IPv4/UDP/DNS A query for "example.com",
 *   post it to the TX ring, then read the real reply from the RX ring.
 *
 * The kernel `netbridge` shell command is the pump: it drains TX into e1000_tx
 * and places e1000_recv frames into RX.  E32Run returns immediately, so the
 * test harness starts `netbridge` while this process is waiting on the rings.
 *
 * Serial markers use genuine EUSER.DLL ordinal 915 (RDebug::Printf), bound by
 * E32Run from the PE2 IAT at 0x408000 (symbol dnsnet_printf_slot).
 */
#include "runtime.h"
#include "netbridge.h"

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern unsigned int dnsnet_printf_slot;
typedef void (*DnsnetPrintf)(const char*, ...);

static void marker(const char* text)
{
    unsigned int fn = *(volatile unsigned int*)&dnsnet_printf_slot;
    if (fn != 915u && fn != 0u) ((DnsnetPrintf)fn)(text);
    else print(text);                         /* no EUSER.DLL bound */
}

/* ---------------------------------------------------------------- tiny helpers */
static void scopy(void* dst, const void* src, unsigned n)
{
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
}
static void put16(unsigned char* p, unsigned v)
    { p[0] = (unsigned char)(v >> 8); p[1] = (unsigned char)v; }
static unsigned get16(const unsigned char* p)
    { return ((unsigned)p[0] << 8) | p[1]; }

static unsigned append(char* p, unsigned k, const char* s)
    { while (*s) p[k++] = *s++; return k; }
static unsigned dec(char* p, unsigned k, unsigned v)
{
    char t[12];
    int n = 0;
    if (!v) t[n++] = '0';
    while (v) { t[n++] = (char)('0' + v % 10u); v /= 10u; }
    while (n) p[k++] = t[--n];
    return k;
}
static unsigned fmt_ip(char* p, unsigned k, const unsigned char ip[4])
{
    for (unsigned i = 0; i < 4; ++i)
        {
        if (i) p[k++] = '.';
        k = dec(p, k, ip[i]);
        }
    return k;
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
    if (!*name) return -6;
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
            return (pos + 1 < len) ? pos + 2 : 0u;   /* 2-byte NAME pointer */
        if (!c) return pos + 1;
        pos += c + 1;
        }
}

/* Parse a response: matching ID, QR=1, RCODE=0, first A/IN answer -> ip[4]. */
static int dns_parse_answer(const unsigned char* r, unsigned len, unsigned id,
                            unsigned char ip[4])
{
    if (len < DNS_HEADER) return -20;
    if (get16(r) != id) return -1;
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
        pos += rdlen;
        }
    return -1;
}

/* ----------------------------------------------------- IPv4 / UDP checksums */
static unsigned checksum(const unsigned char* p, unsigned len, unsigned sum)
{
    while (len > 1) { sum += get16(p); p += 2; len -= 2; }
    if (len) sum += (unsigned)p[0] << 8;
    while (sum >> 16) sum = (sum & 0xffffu) + (sum >> 16);
    return sum;
}

/* -------------------------------------------------------- frame construction */
static const unsigned char kSrcIp[4] = { 10, 0, 2, 15 };   /* guest   */
static const unsigned char kDnsIp[4] = { 10, 0, 2, 3 };    /* SLIRP DNS proxy */
static const unsigned char kGwIp[4]  = { 10, 0, 2, 2 };    /* SLIRP gateway   */

/* ARP request: who-has <ip> tell 10.0.2.15 */
static int build_arp(unsigned ip0, unsigned char* f, const unsigned char* mac)
{
    unsigned i;
    for (i = 0; i < 6; ++i) { f[i] = 0xff; f[6 + i] = mac[i]; }
    f[12] = 0x08; f[13] = 0x06;
    f[14] = 0; f[15] = 1; f[16] = 0x08; f[17] = 0;
    f[18] = 6; f[19] = 4; f[20] = 0; f[21] = 1;   /* Ethernet/IPv4 request */
    for (i = 0; i < 6; ++i) f[22 + i] = mac[i];
    scopy(f + 28, kSrcIp, 4);
    for (i = 0; i < 6; ++i) f[32 + i] = 0;
    f[38] = 10; f[39] = 0; f[40] = 2; f[41] = (unsigned char)ip0;
    return 42;
}

/* Ethernet(14) + IPv4(20) + UDP(8) + DNS.  IPv4 dst = SLIRP DNS proxy 10.0.2.3
 * routed through the gateway MAC learned by ARP; IPv4 src is always 10.0.2.15. */
static int build_dns_frame(const unsigned char* dns, unsigned dlen,
                           const unsigned char gwmac[6],
                           const unsigned char* srcmac,
                           unsigned char* f, unsigned cap)
{
    unsigned ulen = 8 + dlen;
    unsigned iplen = 20 + ulen;
    unsigned flen = 14 + iplen;
    if (flen > cap) return -1;

    scopy(f, gwmac, 6);                         /* Ethernet dst = gateway MAC */
    scopy(f + 6, srcmac, 6);
    f[12] = 0x08; f[13] = 0x00;                 /* EtherType IPv4 */

    unsigned char* ip = f + 14;
    ip[0] = 0x45; ip[1] = 0;
    put16(ip + 2, (unsigned short)iplen);
    put16(ip + 4, 0x1234); put16(ip + 6, 0);     /* ID, flags/frag */
    ip[8] = 64; ip[9] = 17; put16(ip + 10, 0);   /* TTL, proto UDP, csum=0 */
    scopy(ip + 12, kSrcIp, 4);
    scopy(ip + 16, kDnsIp, 4);
    put16(ip + 10, (unsigned short)~checksum(ip, 20, 0));

    unsigned char* udp = ip + 20;
    put16(udp + 0, UDP_SPORT);
    put16(udp + 2, DNS_PORT);
    put16(udp + 4, (unsigned short)ulen);
    put16(udp + 6, 0);                          /* checksum=0 while computing */
    scopy(udp + 8, dns, dlen);
    unsigned sum = checksum(kSrcIp, 4, 0);
    sum = checksum(kDnsIp, 4, sum);
    sum += 17u;                                 /* zero + protocol (UDP) */
    sum += ulen;                                /* UDP length */
    sum = checksum(udp, ulen, sum);             /* UDP header + payload */
    unsigned csum = ~sum & 0xffffu;
    put16(udp + 6, (unsigned short)(csum ? csum : 0xffffu));
    return (int)flen;
}

/* Return DNS payload length for a genuine reply to our UDP socket, or 0.
 * Fills srcip with the IPv4 source address of the reply. */
static unsigned extract_dns(const unsigned char* f, unsigned len,
                            unsigned char srcip[4], unsigned* dns_off)
{
    if (len < 14u + 20u + 8u) return 0;
    if (f[12] != 0x08 || f[13] != 0x00) return 0;
    const unsigned char* ip = f + 14;
    if ((ip[0] >> 4) != 4 || ip[9] != 17) return 0;
    unsigned ihl = (unsigned)(ip[0] & 0x0f) * 4;
    if (ihl < 20) return 0;
    unsigned totlen = get16(ip + 2);
    if (totlen < 20u + 8u || (unsigned)len < 14u + totlen) return 0;
    const unsigned char* udp = ip + ihl;
    /* Reply: UDP src = DNS_PORT(53), dst = our ephemeral port (49152). */
    if (get16(udp + 0) != DNS_PORT || get16(udp + 2) != UDP_SPORT) return 0;
    unsigned ulen = get16(udp + 4);
    if (ulen < 8u || 14u + ihl + ulen > len) return 0;
    scopy(srcip, ip + 12, 4);
    *dns_off = 14u + ihl + 8u;
    return ulen - 8u;
}

/* ---------------------------------------------------------------- bridge rx/tx */
static struct nb_shared* gSh;
static unsigned char gFrame[NB_FRAME_SIZE];

static void tx_post(const unsigned char* f, unsigned len)
{
    struct nb_ring* tx = &gSh->tx;
    const unsigned ti = tx->head;
    struct nb_slot* ts = &tx->slot[ti];
    if (ts->len) return;                        /* ring full: drop */
    scopy((void*)ts->data, f, len);
    ts->len = len;
    __asm__ __volatile__("" : : : "memory");
    tx->head = (ti + 1u) % NB_RING_SLOTS;
}

/* Consume the next ready RX frame (all pending slots), return its length. */
static unsigned rx_take(void)
{
    struct nb_ring* rx = &gSh->rx;
    for (unsigned n = 0; n < NB_RING_SLOTS; ++n)
        {
        const unsigned ri = rx->tail;
        struct nb_slot* rs = &rx->slot[ri];
        const unsigned len = rs->len;
        if (!len) break;
        unsigned take = len > NB_FRAME_SIZE ? NB_FRAME_SIZE : len;
        scopy(gFrame, (const void*)rs->data, take);
        rs->len = 0;
        __asm__ __volatile__("" : : : "memory");
        rx->tail = (ri + 1u) % NB_RING_SLOTS;
        return take;
        }
    return 0;
}

static int is_arp_reply_from_gw(const unsigned char* f, unsigned len)
{
    return len >= 42 && f[12] == 0x08 && f[13] == 0x06 &&
           f[20] == 0x00 && f[21] == 0x02 &&
           f[28] == 10 && f[29] == 0 && f[30] == 2 && f[31] == 2;
}

/* ------------------------------------------------------------------------ main */
int app_main(void)
{
    static const char* kName = "example.com";
    char line[128];
    unsigned k;

    marker("DNSNET: DNS client over netbridge start (EUSER ord 915)");

    int base = exec2(QE_EXEC_NETBRIDGE_BASE, 0, 0);
    int size = exec2(QE_EXEC_NETBRIDGE_SIZE, 0, 0);
    if (base <= 0 || size < (int)sizeof(struct nb_shared))
        { marker("DNSNET: FAIL bridge discovery (execs 247/248)"); return 1; }
    gSh = (struct nb_shared*)base;
    if (gSh->magic != NB_MAGIC || gSh->version != NB_VERSION)
        { marker("DNSNET: FAIL bridge magic"); return 1; }

    /* ---- step 1: ARP for the gateway 10.0.2.2 ---- */
    unsigned char gwmac[6];
    int arp_ok = 0;
    int flen = build_arp(kGwIp[3], gFrame, (const unsigned char*)gSh->mac);
    tx_post(gFrame, (unsigned)flen);
    marker("DNSNET: ARP request for gateway 10.0.2.2 posted");
    for (int poll = 0; poll < 250; ++poll)
        {
        sleep_ms(20);
        unsigned len = rx_take();
        if (is_arp_reply_from_gw(gFrame, len))
            {
            scopy(gwmac, gFrame + 22, 6);        /* sender hardware = gateway */
            arp_ok = 1;
            k = append(line, 0, "DNSNET: gateway MAC learned ");
            const char* h = "0123456789abcdef";
            for (unsigned i = 0; i < 6; ++i)
                {
                if (i) line[k++] = ':';
                line[k++] = h[(gwmac[i] >> 4) & 0xf];
                line[k++] = h[gwmac[i] & 0xf];
                }
            line[k] = 0;
            marker(line);
            break;
            }
        }
    if (!arp_ok)
        {
        /* Known QEMU SLIRP gateway MAC from the netbridge ARP proof. */
        static const unsigned char kFallback[6] =
            { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };
        scopy(gwmac, kFallback, 6);
        marker("DNSNET: gateway MAC fallback 52:55:0a:00:02:02");
        }

    /* ---- step 2: build and post the real A/IN DNS query ---- */
    unsigned char query[512], answer[512];
    unsigned id = 0;
    int qlen = dns_build_query(kName, query, sizeof(query), &id);
    if (qlen < 0) { marker("DNSNET: FAIL query encode"); gSh->done = 1; return 1; }
    flen = build_dns_frame(query, (unsigned)qlen, gwmac,
                           (const unsigned char*)gSh->mac,
                           gFrame, sizeof(gFrame));
    if (flen < 0) { marker("DNSNET: FAIL frame build"); gSh->done = 1; return 1; }
    tx_post(gFrame, (unsigned)flen);
    k = append(line, 0, "DNSNET: query sent ");
    k = append(line, k, kName);
    line[k] = 0;
    marker(line);

    /* ---- step 3: wait for the genuine DNS reply in the RX ring ---- */
    int found = 0;
    for (int poll = 0; poll < 500; ++poll)
        {
        sleep_ms(20);
        unsigned len = rx_take();
        if (!len) continue;
        unsigned char srcip[4], rip[4];
        unsigned dns_off = 0;
        unsigned dlen = extract_dns(gFrame, len, srcip, &dns_off);
        if (!dlen || dlen > sizeof(answer)) continue;
        scopy(answer, gFrame + dns_off, dlen);
        if (dns_parse_answer(answer, dlen, id, rip) != 0) continue;

        k = append(line, 0, "DNSNET: reply from ");
        k = fmt_ip(line, k, srcip);
        line[k] = 0;
        marker(line);

        k = append(line, 0, "DNSNET: ");
        k = append(line, k, kName);
        k = append(line, k, " -> ");
        k = fmt_ip(line, k, rip);
        line[k] = 0;
        marker(line);
        found = 1;
        break;
        }

    if (!found) marker("DNSNET: no reply (timeout)");
    marker("DNSNET: DONE");
    gSh->done = 1;                              /* stop the kernel poll loop */
    return 0;
}
