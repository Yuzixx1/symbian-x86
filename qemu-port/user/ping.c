/*
 * ping.c -- PE2 E32 real ICMP echo client over the kernel<->userland e1000
 * frame bridge.  This is NOT a loopback self-test: every echo request is a
 * real Ethernet/IPv4/ICMP frame pushed through netbridge's TX ring into QEMU
 * SLIRP, and every reply is a genuine frame read back out of the RX ring.
 *
 * Flow:
 *   1. Discover the bridge via slow execs 247 (base) / 248 (size).
 *   2. Post a raw ARP "who-has 10.0.2.2" request; learn the gateway MAC from
 *      the real ARP reply (fall back to SLIRP's known 52:55:0a:00:02:02 if
 *      ARP somehow fails, but still attempt ICMP).
 *   3. For each sequence number, build Ethernet + IPv4 (10.0.2.15 ->
 *      10.0.2.2, TTL 64, proto 1) + ICMP echo request with correct IP and
 *      ICMP checksums, post to TX, then poll RX for an echo reply with a
 *      matching id/seq and verified checksums.
 *   4. Print PING: markers and set sh->done so the kernel poll loop exits.
 *
 * Serial markers use genuine EUSER.DLL ordinal 915 (RDebug::Printf), bound by
 * E32Run from the PE2 IAT at 0x408000 (ping_printf_slot).
 */
#include "runtime.h"
#include "netbridge.h"

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern unsigned int ping_printf_slot;
typedef void (*NbPrintf)(const char*, ...);

static void marker(const char* text)
{
    unsigned int fn = *(volatile unsigned int*)&ping_printf_slot;
    if (fn != 915u && fn != 0u) ((NbPrintf)fn)(text);
    else print(text);                         /* no EUSER.DLL bound */
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
static unsigned hex2(char* p, unsigned k, unsigned v)
{
    const char* h = "0123456789abcdef";
    p[k++] = h[(v >> 4) & 0xfu];
    p[k++] = h[v & 0xfu];
    return k;
}

/* ---- unaligned-safe big-endian word helpers (reused from dnsresolv.c) ---- */
static void put16(unsigned char* p, unsigned v)
    { p[0] = (unsigned char)(v >> 8); p[1] = (unsigned char)v; }
static unsigned get16(const unsigned char* p)
    { return ((unsigned)p[0] << 8) | p[1]; }
static int same4(const unsigned char* a, const unsigned char* b)
    { return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3]; }
static void copy4(unsigned char* d, const unsigned char* s)
    { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3]; }

/* RFC1071 one's-complement sum, folded to 16 bits (no final complement). */
static unsigned csum(const unsigned char* p, unsigned len, unsigned sum)
{
    while (len > 1) { sum += get16(p); p += 2; len -= 2; }
    if (len) sum += (unsigned)p[0] << 8;
    while (sum >> 16) sum = (sum & 0xffffu) + (sum >> 16);
    return sum;
}

/* ---- addresses / limits ---- */
static const unsigned char GW_IP[4]   = { 10, 0, 2, 2 };
static const unsigned char OUR_IP[4]  = { 10, 0, 2, 15 };
/* QEMU SLIRP's virtual MAC for 10.0.2.2 (seen in the netbridge ARP reply). */
static const unsigned char GW_MAC_FALLBACK[6] =
    { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };

#define PING_ID          0x4b45u   /* ICMP identifier */
#define PING_PAYLOAD     16u       /* "EKA2PING" + 8 pattern bytes */
#define NUM_PINGS        3
#define ARP_POLLS        220
#define ICMP_POLLS       160
#define POLL_SLEEP_MS    20

/* ---- ring helpers ---- */
static int post_frame(struct nb_shared* sh, const unsigned char* f, unsigned len)
{
    struct nb_ring* tx = &sh->tx;
    const unsigned ti = tx->head;
    struct nb_slot* ts = &tx->slot[ti];
    if (ts->len) return -1;                     /* slot still busy */
    for (unsigned i = 0; i < len; ++i) ts->data[i] = f[i];
    ts->len = len;
    __asm__ __volatile__("" : : : "memory");
    tx->head = (ti + 1u) % NB_RING_SLOTS;
    return 0;
}

/* Consume at most one pending RX frame, always releasing the slot. Returns
 * the frame length (0 = nothing pending). */
static unsigned rx_take(struct nb_shared* sh, unsigned char* buf, unsigned cap)
{
    struct nb_ring* rx = &sh->rx;
    const unsigned ri = rx->tail;
    struct nb_slot* rs = &rx->slot[ri];
    const unsigned len = rs->len;
    if (!len) return 0;
    unsigned n = len < cap ? len : cap;
    for (unsigned i = 0; i < n; ++i) buf[i] = rs->data[i];
    rs->len = 0;
    __asm__ __volatile__("" : : : "memory");
    rx->tail = (ri + 1u) % NB_RING_SLOTS;
    return len;
}

/* ---- ARP request / reply ---- */
static void build_arp_request(unsigned char* f, const unsigned char* mac)
{
    unsigned i;
    for (i = 0; i < 6; ++i) { f[i] = 0xff; f[6 + i] = mac[i]; }
    f[12] = 0x08; f[13] = 0x06;                 /* EtherType ARP */
    f[14] = 0x00; f[15] = 0x01;                 /* hardware type Ethernet */
    f[16] = 0x08; f[17] = 0x00;                 /* protocol type IPv4 */
    f[18] = 6;    f[19] = 4;                    /* address lengths */
    f[20] = 0x00; f[21] = 0x01;                 /* ARP op: request */
    for (i = 0; i < 6; ++i) f[22 + i] = mac[i]; /* sender hardware */
    copy4(f + 28, OUR_IP);                      /* sender protocol 10.0.2.15 */
    for (i = 0; i < 6; ++i) f[32 + i] = 0;      /* target hardware (blank) */
    copy4(f + 38, GW_IP);                       /* target protocol 10.0.2.2 */
}

/* ARP reply: EtherType ARP, op=2, sender protocol IP == gateway. */
static int parse_arp_reply(const unsigned char* r, unsigned len,
                           unsigned char mac[6])
{
    unsigned i;
    if (len < 42 || r[12] != 0x08 || r[13] != 0x06) return 0;
    if (r[20] != 0x00 || r[21] != 0x02) return 0;
    if (!same4(r + 28, GW_IP)) return 0;
    for (i = 0; i < 6; ++i) mac[i] = r[22 + i];
    return 1;
}

/* ---- ICMP echo request builder (returns total Ethernet frame length) ---- */
static unsigned build_echo(unsigned char* f, const unsigned char* ourmac,
                           const unsigned char* gwmac, unsigned seq)
{
    unsigned i, off;
    for (i = 0; i < 6; ++i) { f[i] = gwmac[i]; f[6 + i] = ourmac[i]; }
    f[12] = 0x08; f[13] = 0x00;                 /* EtherType IPv4 */

    f[14] = 0x45; f[15] = 0x00;                 /* IPv4, IHL=5, ToS=0 */
    put16(f + 16, 20u + 8u + PING_PAYLOAD);     /* total IP length */
    put16(f + 18, 0x1234);                      /* identification */
    put16(f + 20, 0x4000);                      /* flags: DF, no fragment */
    f[22] = 64; f[23] = 1;                      /* TTL=64, protocol=ICMP */
    put16(f + 24, 0);
    copy4(f + 26, OUR_IP);
    copy4(f + 30, GW_IP);
    put16(f + 24, (unsigned short)~csum(f + 14, 20, 0)); /* IP checksum */

    off = 14 + 20;
    f[off] = 8; f[off + 1] = 0;                 /* echo request, code 0 */
    put16(f + off + 2, 0);                      /* ICMP checksum placeholder */
    put16(f + off + 4, PING_ID);
    put16(f + off + 6, seq);
    {
        static const unsigned char pat[8] =
            { 'E', 'K', 'A', '2', 'P', 'I', 'N', 'G' };
        for (i = 0; i < 8; ++i) f[off + 8 + i] = pat[i];
        for (i = 0; i < 8; ++i) f[off + 16 + i] = (unsigned char)(0x41 + i);
    }
    put16(f + off + 2, (unsigned short)~csum(f + off, 8u + PING_PAYLOAD, 0));
    return 14u + 20u + 8u + PING_PAYLOAD;
}

/* Echo reply: IPv4, proto 1, ICMP type 0/code 0, matching id+seq, valid
 * IP + ICMP checksums, source == gateway.  Returns payload length on match. */
static int parse_echo_reply(const unsigned char* r, unsigned len,
                            unsigned seq, unsigned* plen_out)
{
    unsigned ihl, off, icmplen;
    if (len < 14u + 20u + 8u) return 0;
    if (r[12] != 0x08 || r[13] != 0x00) return 0;       /* EtherType IPv4 */
    if ((r[14] >> 4) != 4) return 0;
    ihl = (unsigned)(r[14] & 0x0fu) * 4u;
    if (ihl < 20 || r[23] != 1) return 0;               /* protocol ICMP */
    if (!same4(r + 14 + 12, GW_IP)) return 0;           /* from 10.0.2.2 */
    if (csum(r + 14, ihl, 0) != 0xffffu) return 0;      /* bad IP checksum */
    off = 14 + ihl;
    if (r[off] != 0 || r[off + 1] != 0) return 0;       /* echo reply */
    if (get16(r + off + 4) != PING_ID) return 0;
    if (get16(r + off + 6) != seq) return 0;
    icmplen = get16(r + 14 + 2);                       /* IP total length */
    if (icmplen < ihl + 8 || icmplen > len - 14) return 0;
    icmplen -= ihl;                                    /* ICMP header + data */
    if (icmplen > len - off) return 0;
    if (csum(r + off, icmplen, 0) != 0xffffu) return 0; /* bad ICMP checksum */
    *plen_out = icmplen - 8;
    return 1;
}

int app_main(void)
{
    char line[192];
    unsigned k, i, seq, replies = 0;
    unsigned char gwmac[6], ourmac[6];
    int have_mac = 0;

    int base = exec2(QE_EXEC_NETBRIDGE_BASE, 0, 0);
    int size = exec2(QE_EXEC_NETBRIDGE_SIZE, 0, 0);
    if (base <= 0 || size < (int)sizeof(struct nb_shared))
        {
        marker("PING: FAIL bridge discovery (execs 247/248)");
        return 1;
        }
    struct nb_shared* sh = (struct nb_shared*)base;
    if (sh->magic != NB_MAGIC || sh->version != NB_VERSION)
        {
        marker("PING: FAIL bridge magic");
        return 1;
        }
    for (i = 0; i < 6; ++i) ourmac[i] = sh->mac[i];

    /* ---- 1. ARP for the gateway, learn its MAC ---- */
    {
        unsigned char f[64];
        for (i = 0; i < sizeof(f); ++i) f[i] = 0;
        build_arp_request(f, ourmac);
        if (post_frame(sh, f, 42) != 0)
            marker("PING: ARP request TX busy (will still try ICMP)");
    }
    {
        unsigned char r[NB_FRAME_SIZE];
        for (int poll = 0; poll < ARP_POLLS && !have_mac; ++poll)
            {
            sleep_ms(POLL_SLEEP_MS);
            unsigned len = rx_take(sh, r, sizeof(r));
            if (parse_arp_reply(r, len, gwmac)) have_mac = 1;
            }
    }
    if (!have_mac)
        for (i = 0; i < 6; ++i) gwmac[i] = GW_MAC_FALLBACK[i];

    k = append(line, 0, "PING: ARP gateway 10.0.2.2 -> ");
    for (i = 0; i < 6; ++i)
        {
        if (i) k = append(line, k, ":");
        k = hex2(line, k, gwmac[i]);
        }
    k = append(line, k, have_mac ? " (learned)" : " (fallback)");
    line[k] = 0;
    marker(line);

    /* ---- 2. ICMP echo requests through the bridge ---- */
    for (seq = 1; seq <= NUM_PINGS; ++seq)
        {
        unsigned char f[NB_FRAME_SIZE];
        unsigned flen = build_echo(f, ourmac, gwmac, seq);
        if (post_frame(sh, f, flen) != 0)
            {
            marker("PING: echo request TX busy");
            continue;
            }
        k = append(line, 0, "PING: echo request seq ");
        k = dec(line, k, seq);
        k = append(line, k, " sent");
        line[k] = 0;
        marker(line);

        unsigned char r[NB_FRAME_SIZE];
        int got = 0;
        for (int poll = 0; poll < ICMP_POLLS; ++poll)
            {
            sleep_ms(POLL_SLEEP_MS);
            unsigned len = rx_take(sh, r, sizeof(r));
            unsigned plen = 0;
            if (parse_echo_reply(r, len, seq, &plen))
                {
                k = append(line, 0, "PING: reply from 10.0.2.2 seq ");
                k = dec(line, k, seq);
                k = append(line, k, " len=");
                k = dec(line, k, 8u + plen);
                line[k] = 0;
                marker(line);

                k = append(line, 0, "PING: RTT-ish seq ");
                k = dec(line, k, seq);
                k = append(line, k, " poll=");
                k = dec(line, k, (unsigned)poll);
                line[k] = 0;
                marker(line);
                ++replies;
                got = 1;
                break;
                }
            }
        if (!got)
            {
            k = append(line, 0, "PING: seq ");
            k = dec(line, k, seq);
            k = append(line, k, " timed out (no echo reply)");
            line[k] = 0;
            marker(line);
            }
        }

    k = append(line, 0, "PING: ");
    k = dec(line, k, replies);
    k = append(line, k, "/");
    k = dec(line, k, (unsigned)NUM_PINGS);
    k = append(line, k, " replies");
    line[k] = 0;
    marker(line);
    marker("PING: DONE");

    sh->done = 1;                               /* stop the kernel poll loop */
    return replies ? 0 : 1;
}
