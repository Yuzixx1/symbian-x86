/*
 * dhcpc.c -- PE2 E32 DHCP client that obtains a REAL IP lease over e1000/SLIRP.
 *
 * It uses the kernel<->userland e1000 frame bridge (user/netbridge.h):
 *   - discover the shared bridge via slow execs 247 (base) / 248 (size);
 *   - build Ethernet(14) + IPv4(20) + UDP(8) + DHCP BOOTP DISCOVER, post it
 *     into the TX ring, and let the kernel's `netbridge` poll command hand it
 *     to the e1000;
 *   - read the DHCP OFFER from the RX ring, send a REQUEST for the offered
 *     address, and read the ACK.  QEMU SLIRP runs a genuine DHCP server on
 *     UDP 67 that leases 10.0.2.15 with gateway 10.0.2.2.
 *
 * All framing/checksum code is freestanding (no libc).  Serial markers use
 * the genuine EUSER.DLL ordinal 915 (RDebug::Printf), bound by E32Run from
 * the PE2 IAT at 0x408000 (absolute symbol dhcp_printf_slot).  If EUSER is
 * not bound the markers fall back to the kernel console print().
 */
#include "runtime.h"
#include "netbridge.h"

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern unsigned int dhcp_printf_slot;
typedef void (*DhcpPrintf)(const char*, ...);

static void marker(const char* text)
{
    unsigned int fn = *(volatile unsigned int*)&dhcp_printf_slot;
    if (fn != 915u && fn != 0u) ((DhcpPrintf)fn)(text);
    else print(text);
}

/* ------------------------------------------------------------ tiny helpers */
static void scopy(void* dst, const void* src, unsigned n)
{
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
}
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
static unsigned get16(const unsigned char* p)
    { return ((unsigned)p[0] << 8) | p[1]; }
static unsigned get32(const unsigned char* p)
    { return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) |
             ((unsigned)p[2] << 8) | p[3]; }
static void put16(unsigned char* p, unsigned v)
    { p[0] = (unsigned char)(v >> 8); p[1] = (unsigned char)v; }
static void put32(unsigned char* p, unsigned v)
{
    p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);  p[3] = (unsigned char)v;
}
static unsigned checksum(const unsigned char* p, unsigned len, unsigned sum)
{
    while (len > 1) { sum += get16(p); p += 2; len -= 2; }
    if (len) sum += (unsigned)p[0] << 8;
    while (sum >> 16) sum = (sum & 0xffffu) + (sum >> 16);
    return sum;
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

/* --------------------------------------------------------- frame offsets */
#define ETH_O      0u
#define IP_O      14u
#define UDP_O     34u                 /* IP_O + 20 */
#define BOOTP_O   42u                 /* UDP_O + 8  */
#define COOKIE_O 278u                 /* BOOTP_O + 236 */
#define OPT_O    282u                 /* COOKIE_O + 4 */
#define DHCP_MIN 300u                 /* pad BOOTP+options to >= 300 bytes */

/* --------------------------------------------------- DHCP request builder */
/* msgtype: 1=DISCOVER, 3=REQUEST.  For REQUEST, yiaddr is the offered address
 * and server is the DHCP server identifier from the OFFER. */
static int build_request(unsigned char* f, const unsigned char mac[6],
                         unsigned xid, unsigned char msgtype, int request,
                         const unsigned char yiaddr[4],
                         const unsigned char server[4])
{
    unsigned i, o;
    unsigned dlen, ulen, iplen;
    unsigned char* b = f + BOOTP_O;
    unsigned char* u = f + UDP_O;
    unsigned char* ip = f + IP_O;

    /* zero the whole BOOTP + options region, then fill it */
    for (i = BOOTP_O; i < BOOTP_O + DHCP_MIN; ++i) f[i] = 0;

    /* ---- Ethernet: broadcast dst, guest MAC src, EtherType IPv4 ---- */
    for (i = 0; i < 6; ++i) { f[ETH_O + i] = 0xff; f[ETH_O + 6 + i] = mac[i]; }
    f[ETH_O + 12] = 0x08; f[ETH_O + 13] = 0x00;

    /* ---- BOOTP fixed part ---- */
    b[0] = 1;             /* op = BOOTREQUEST */
    b[1] = 1;             /* htype = Ethernet */
    b[2] = 6;             /* hlen */
    b[3] = 0;             /* hops */
    put32(b + 4, xid);    /* transaction id */
    put16(b + 8, 0);      /* secs */
    put16(b + 10, 0x8000u); /* flags: ask server to broadcast the reply */
    /* ciaddr / yiaddr / siaddr / giaddr (b+12..b+27) stay 0.0.0.0 */
    for (i = 0; i < 6; ++i) b[28 + i] = mac[i];   /* chaddr */
    /* sname (b+44..107) and file (b+108..235) stay zero */
    put32(f + COOKIE_O, 0x63825363u);             /* magic cookie */

    /* ---- DHCP options ---- */
    o = OPT_O;
    f[o++] = 53; f[o++] = 1; f[o++] = (unsigned char)msgtype;
    f[o++] = 55; f[o++] = 4;                       /* parameter request list */
    f[o++] = 1;  f[o++] = 3; f[o++] = 6; f[o++] = 15;
    f[o++] = 61; f[o++] = 7; f[o++] = 1;           /* client ID: type 1 + MAC */
    for (i = 0; i < 6; ++i) f[o++] = mac[i];
    if (request)
        {
        f[o++] = 50; f[o++] = 4;                   /* requested IP address */
        for (i = 0; i < 4; ++i) f[o++] = yiaddr[i];
        f[o++] = 54; f[o++] = 4;                   /* server identifier */
        for (i = 0; i < 4; ++i) f[o++] = server[i];
        }
    f[o++] = 255;                                  /* END */

    dlen = o - BOOTP_O;
    if (dlen < DHCP_MIN) dlen = DHCP_MIN;
    for (i = o; i < BOOTP_O + dlen; ++i) f[i] = 0; /* pad with PAD/zeros */

    /* ---- UDP header: src 68 (client), dst 67 (server) ---- */
    ulen = 8u + dlen;
    put16(u + 0, 68); put16(u + 2, 67);
    put16(u + 4, (unsigned short)ulen);
    put16(u + 6, 0);

    /* ---- IPv4 header ---- */
    iplen = 20u + ulen;
    ip[0] = 0x45; ip[1] = 0;
    put16(ip + 2, (unsigned short)iplen);
    put16(ip + 4, (unsigned short)(xid & 0xffffu)); /* identification */
    put16(ip + 6, 0);                               /* flags / fragment */
    ip[8] = 64; ip[9] = 17; put16(ip + 10, 0);
    for (i = 0; i < 4; ++i) { ip[12 + i] = 0; ip[16 + i] = 0xff; }
    put16(ip + 10, (unsigned short)~checksum(ip, 20, 0));

    /* ---- UDP checksum with the IPv4 pseudo-header ----
     * src = 0.0.0.0, dst = 255.255.255.255, zero, proto = 17, UDP length. */
    {
    unsigned char ph[12];
    unsigned sum, csum;
    for (i = 0; i < 4; ++i) { ph[i] = 0; ph[4 + i] = 0xff; }
    ph[8] = 0; ph[9] = 17; put16(ph + 10, ulen);
    sum = checksum(ph, sizeof(ph), 0);
    sum = checksum(u, ulen, sum);
    csum = ~sum & 0xffffu;
    put16(u + 6, (unsigned short)(csum ? csum : 0xffffu));
    }

    /* IP_O (14) is also the Ethernet header length: total frame = 14 + iplen. */
    return (int)(IP_O + iplen);
}

/* -------------------------------------------------- DHCP reply parser */
static int parse_reply(const unsigned char* f, unsigned len, unsigned xid,
                       unsigned char yiaddr[4], unsigned char server[4],
                       unsigned char mask[4], unsigned char router[4],
                       unsigned char dns[4], unsigned* msgtype)
{
    const unsigned char *ip, *u, *b, *opt, *end;
    unsigned ihl, ulen;

    if (len < ETH_O + 20u + 8u + 240u) return -1;
    if (f[12] != 0x08 || f[13] != 0x00) return -1;            /* IPv4 */
    ip = f + IP_O;
    if ((ip[0] >> 4) != 4) return -1;
    ihl = (unsigned)(ip[0] & 0x0fu) * 4u;
    if (ihl < 20u || ip[9] != 17) return -1;                  /* UDP */
    u = f + IP_O + ihl;
    if (get16(u + 0) != 67 || get16(u + 2) != 68) return -1;  /* 67 -> 68 */
    ulen = get16(u + 4);
    if (ulen < 8u) return -1;
    end = u + ulen;
    if (end > f + len) end = f + len;

    b = u + 8;
    if (b + 240u > end) return -1;
    if (b[0] != 2) return -1;                                 /* BOOTREPLY */
    if (get32(b + 4) != xid) return -1;                       /* xid match */
    scopy(yiaddr, b + 16, 4);                                 /* yiaddr */
    opt = b + 236;
    if (opt + 4u > end || get32(opt) != 0x63825363u) return -1;
    opt += 4;

    *msgtype = 0;
    while (opt < end)
        {
        unsigned code = *opt++;
        unsigned olen;
        if (code == 255) break;                               /* END */
        if (code == 0) continue;                              /* PAD */
        if (opt >= end) break;
        olen = *opt++;
        if (opt + olen > end) break;
        switch (code)
            {
            case 53: if (olen >= 1) *msgtype = *opt; break;   /* msg type */
            case 54: if (olen >= 4) scopy(server, opt, 4); break;
            case 1:  if (olen >= 4) scopy(mask, opt, 4); break;
            case 3:  if (olen >= 4) scopy(router, opt, 4); break;
            case 6:  if (olen >= 4) scopy(dns, opt, 4); break;
            default: break;
            }
        opt += olen;
        }
    return (*msgtype) ? 0 : -1;
}

/* ------------------------------------------------------- bridge helpers */
static int tx_post(struct nb_shared* sh, const unsigned char* frame, int len)
{
    struct nb_ring* tx = &sh->tx;
    const unsigned ti = tx->head;
    struct nb_slot* ts = &tx->slot[ti];
    if (ts->len) return -1;                        /* ring full */
    scopy((void*)ts->data, frame, (unsigned)len);
    ts->len = (unsigned)len;
    __asm__ __volatile__("" : : : "memory");
    tx->head = (ti + 1u) % NB_RING_SLOTS;
    return 0;
}

static int rx_wait(struct nb_shared* sh, unsigned xid, unsigned want,
                   unsigned char yiaddr[4], unsigned char server[4],
                   unsigned char mask[4], unsigned char router[4],
                   unsigned char dns[4], int polls, int interval)
{
    struct nb_ring* rx = &sh->rx;
    for (int p = 0; p < polls; ++p)
        {
        sleep_ms(interval);
        const unsigned ri = rx->tail;
        struct nb_slot* rs = &rx->slot[ri];
        const unsigned len = rs->len;
        if (len)
            {
            unsigned msgtype = 0;
            int r = parse_reply((const unsigned char*)rs->data, len, xid,
                                yiaddr, server, mask, router, dns, &msgtype);
            rs->len = 0;                           /* release slot back */
            __asm__ __volatile__("" : : : "memory");
            rx->tail = (ri + 1u) % NB_RING_SLOTS;
            if (r == 0 && msgtype == want) return 1;
            }
        }
    return 0;
}

/* ------------------------------------------------------------------ main */
int app_main(void)
{
    unsigned char line[192];
    unsigned k;
    int base = exec2(QE_EXEC_NETBRIDGE_BASE, 0, 0);
    int size = exec2(QE_EXEC_NETBRIDGE_SIZE, 0, 0);

    if (base <= 0 || size < (int)sizeof(struct nb_shared))
        { marker("DHCP: FAIL bridge discovery (execs 247/248)"); return 1; }
    struct nb_shared* sh = (struct nb_shared*)base;
    if (sh->magic != NB_MAGIC || sh->version != NB_VERSION)
        { marker("DHCP: FAIL bridge magic"); return 1; }

    k = append((char*)line, 0, "DHCP: bridge base=");
    k = dec((char*)line, k, (unsigned)base);
    k = append((char*)line, k, " size=");
    k = dec((char*)line, k, (unsigned)size);
    line[k] = 0;
    marker((const char*)line);

    const unsigned char* mac = (const unsigned char*)sh->mac;
    unsigned xid = random32();
    unsigned char frame[384];
    unsigned char yiaddr[4] = {0,0,0,0}, server[4] = {0,0,0,0};
    unsigned char mask[4] = {0,0,0,0}, router[4] = {0,0,0,0};
    unsigned char dns[4] = {0,0,0,0};

    /* ---- 1. DISCOVER ---- */
    int flen = build_request(frame, mac, xid, 1, 0, yiaddr, server);
    if (tx_post(sh, frame, flen) != 0)
        { marker("DHCP: FAIL TX ring full (DISCOVER)"); sh->done = 1; return 1; }
    marker("DHCP: DISCOVER sent");

    /* ---- 2. OFFER ---- */
    if (!rx_wait(sh, xid, 2, yiaddr, server, mask, router, dns, 400, 15))
        {
        marker("DHCP: no lease (timeout)");
        marker("DHCP: DONE");
        sh->done = 1;
        return 0;
        }
    char sip[16], yip[16];
    fmt_ip(server, sip); fmt_ip(yiaddr, yip);
    k = append((char*)line, 0, "DHCP: OFFER from ");
    k = append((char*)line, k, sip);
    k = append((char*)line, k, " yiaddr=");
    k = append((char*)line, k, yip);
    line[k] = 0;
    marker((const char*)line);

    /* ---- 3. REQUEST ---- */
    flen = build_request(frame, mac, xid, 3, 1, yiaddr, server);
    if (tx_post(sh, frame, flen) != 0)
        { marker("DHCP: FAIL TX ring full (REQUEST)"); sh->done = 1; return 1; }
    marker("DHCP: REQUEST sent");

    /* ---- 4. ACK ---- */
    if (!rx_wait(sh, xid, 5, yiaddr, server, mask, router, dns, 400, 15))
        {
        marker("DHCP: no lease (timeout)");
        marker("DHCP: DONE");
        sh->done = 1;
        return 0;
        }
    marker("DHCP: ACK received");

    char mip[16], gip[16], dip[16];
    fmt_ip(yiaddr, yip); fmt_ip(mask, mip);
    fmt_ip(router, gip); fmt_ip(dns, dip);
    k = append((char*)line, 0, "DHCP: IP=");
    k = append((char*)line, k, yip);
    k = append((char*)line, k, " MASK=");
    k = append((char*)line, k, mip);
    k = append((char*)line, k, " GW=");
    k = append((char*)line, k, gip);
    k = append((char*)line, k, " DNS=");
    k = append((char*)line, k, dip);
    line[k] = 0;
    marker((const char*)line);
    marker("DHCP: DONE");

    sh->done = 1;                                  /* stop the poll loop */
    return 0;
}
