/*
 * ntpc.c -- PE2 E32 REAL NTP time-sync client over the kernel<->userland
 * e1000 frame bridge (user/netbridge.h).
 *
 * This is a genuine network application: it puts real Ethernet frames on
 * QEMU SLIRP through the shared-memory netbridge, just like ping/dnsnet/dhcpc:
 *   - discover the bridge via slow execs 247 (base) / 248 (size);
 *   - ARP for the 10.0.2.2 gateway (fallback to the known SLIRP MAC);
 *   - build Ethernet(14) + IPv4(20) + UDP(8) + 48-byte NTP v4 client request,
 *     post it into the TX ring, and let the kernel `netbridge` poll hand it to
 *     the e1000;
 *   - read the real NTP reply from the RX ring, parse stratum / mode / the
 *     receive (T2) and transmit (T3) timestamps, read local T4 from exec 68,
 *     and report the clock offset from T1/T2/T3/T4 in milliseconds.
 *
 * exec 68 (EExecTimeNow) returns TTime = microseconds since year 0 AD.  The
 * 0AD->1900 bias is 59958302400 seconds (719528 days to 1970, minus the
 * 2208988800 seconds between 1900 and 1970).  The 32-bit wrap is intentional:
 * NTP seconds-since-1900 are themselves 32-bit.
 *
 * Serial markers use genuine EUSER.DLL ordinal 915 (RDebug::Printf), bound by
 * E32Run from the PE2 IAT at 0x408000 (symbol ntpc_printf_slot).
 */
#include "runtime.h"
#include "netbridge.h"

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern unsigned int ntpc_printf_slot;
typedef void (*NtpPrintf)(const char*, ...);

static void marker(const char* text)
{
    unsigned int fn = *(volatile unsigned int*)&ntpc_printf_slot;
    if (fn != 915u && fn != 0u) ((NtpPrintf)fn)(text);
    else print(text);                         /* no EUSER.DLL bound */
}

/* ------------------------------------------------------------ tiny helpers */
static void scopy(void* dst, const void* src, unsigned n)
{
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
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

static unsigned append(char* p, unsigned k, const char* s)
    { while (*s) p[k++] = *s++; return k; }
static unsigned dec(char* p, unsigned k, unsigned v)
{
    char t[12]; int n = 0;
    if (!v) t[n++] = '0';
    while (v) { t[n++] = (char)('0' + v % 10u); v /= 10u; }
    while (n) p[k++] = t[--n];
    return k;
}
static unsigned dec_signed(char* p, unsigned k, long long v)
{
    if (v < 0) { p[k++] = '-'; v = -v; }
    char t[24]; int n = 0;
    if (!v) t[n++] = '0';
    while (v) { t[n++] = (char)('0' + v % 10LL); v /= 10LL; }
    while (n) p[k++] = t[--n];
    return k;
}
static unsigned checksum(const unsigned char* p, unsigned len, unsigned sum)
{
    while (len > 1) { sum += get16(p); p += 2; len -= 2; }
    if (len) sum += (unsigned)p[0] << 8;
    while (sum >> 16) sum = (sum & 0xffffu) + (sum >> 16);
    return sum;
}

/* ------------------------------------------------------------- NTP constants */
#define NTP_PORT     123u
#define UDP_SPORT    49153u
#define NTP_PAYLOAD  48u
#define LI_VN_MODE   0x23u              /* LI=0, VN=4, Mode=3 (client) */
#define SEC_1900_BIAS 59958302400ULL     /* 0AD -> 1900 epoch */

static const unsigned char kSrcIp[4] = { 10, 0, 2, 15 };   /* guest */
static const unsigned char kGwIp[4]  = { 10, 0, 2, 2 };    /* SLIRP gateway */

struct ntp_ts { unsigned sec; unsigned frac; };

/* Read exec 68 (TTime microseconds since 0AD) as an NTP 64-bit timestamp. */
static struct ntp_ts ntp_now(void)
{
    long long us = 0;
    int off = 0;
    exec2(68, (unsigned)&us, (unsigned)&off);
    unsigned long long u = (unsigned long long)us;
    unsigned long long s = u / 1000000ULL - SEC_1900_BIAS;
    unsigned rem = (unsigned)(u % 1000000ULL);
    struct ntp_ts t;
    t.sec  = (unsigned)s;                       /* mod 2^32 == NTP seconds */
    t.frac = (unsigned)(((unsigned long long)rem << 32) / 1000000ULL);
    return t;
}

/* Convert an NTP timestamp to milliseconds since 1900 (signed 64-bit). */
static long long ts_ms(struct ntp_ts t)
{
    return (long long)t.sec * 1000LL +
           (long long)(((unsigned long long)t.frac * 1000ULL) >> 32);
}

/* --------------------------------------------------------- frame builders */
/* ARP request: who-has 10.0.2.2 tell 10.0.2.15 */
static int build_arp(unsigned char* f, const unsigned char* mac)
{
    unsigned i;
    for (i = 0; i < 6; ++i) { f[i] = 0xff; f[6 + i] = mac[i]; }
    f[12] = 0x08; f[13] = 0x06;
    f[14] = 0; f[15] = 1; f[16] = 0x08; f[17] = 0;
    f[18] = 6; f[19] = 4; f[20] = 0; f[21] = 1;   /* Ethernet/IPv4 request */
    for (i = 0; i < 6; ++i) f[22 + i] = mac[i];
    scopy(f + 28, kSrcIp, 4);
    for (i = 0; i < 6; ++i) f[32 + i] = 0;
    scopy(f + 38, kGwIp, 4);                      /* target IP 10.0.2.2 */
    return 42;
}

/* 48-byte NTP v4 client request, transmit timestamp = local T1. */
static void build_ntp_payload(unsigned char* n, struct ntp_ts t1)
{
    unsigned i;
    for (i = 0; i < NTP_PAYLOAD; ++i) n[i] = 0;
    n[0] = (unsigned char)LI_VN_MODE;
    put32(n + 40, t1.sec);                       /* transmit seconds  (T1) */
    put32(n + 44, t1.frac);                      /* transmit fraction      */
}

/* Ethernet + IPv4 + UDP + NTP.  IPv4 dst = gateway 10.0.2.2. */
static int build_ntp_frame(const unsigned char* ntp, unsigned dlen,
                            const unsigned char gwmac[6],
                            const unsigned char* srcmac,
                            unsigned char* f, unsigned cap)
{
    unsigned ulen = 8u + dlen;
    unsigned iplen = 20u + ulen;
    unsigned flen = 14u + iplen;
    if (flen > cap) return -1;

    scopy(f, gwmac, 6);                          /* Ethernet dst = gateway */
    scopy(f + 6, srcmac, 6);
    f[12] = 0x08; f[13] = 0x00;                  /* EtherType IPv4 */

    unsigned char* ip = f + 14;
    ip[0] = 0x45; ip[1] = 0;
    put16(ip + 2, (unsigned short)iplen);
    put16(ip + 4, 0x1234); put16(ip + 6, 0);
    ip[8] = 64; ip[9] = 17; put16(ip + 10, 0);   /* TTL, proto UDP, csum=0 */
    scopy(ip + 12, kSrcIp, 4);
    scopy(ip + 16, kGwIp, 4);
    put16(ip + 10, (unsigned short)~checksum(ip, 20, 0));

    unsigned char* udp = ip + 20;
    put16(udp + 0, UDP_SPORT);
    put16(udp + 2, NTP_PORT);
    put16(udp + 4, (unsigned short)ulen);
    put16(udp + 6, 0);
    scopy(udp + 8, ntp, dlen);
    unsigned sum = checksum(kSrcIp, 4, 0);
    sum = checksum(kGwIp, 4, sum);
    sum += 17u;                                  /* zero + protocol (UDP) */
    sum += ulen;                                 /* UDP length */
    sum = checksum(udp, ulen, sum);
    unsigned csum = ~sum & 0xffffu;
    put16(udp + 6, (unsigned short)(csum ? csum : 0xffffu));
    return (int)flen;
}

/* Recognise a genuine NTP reply to our UDP socket.  Fill ntp_off. */
static unsigned extract_ntp(const unsigned char* f, unsigned len,
                            unsigned char srcip[4], unsigned* ntp_off)
{
    if (len < 14u + 20u + 8u + NTP_PAYLOAD) return 0;
    if (f[12] != 0x08 || f[13] != 0x00) return 0;
    const unsigned char* ip = f + 14;
    if ((ip[0] >> 4) != 4 || ip[9] != 17) return 0;
    unsigned ihl = (unsigned)(ip[0] & 0x0f) * 4u;
    if (ihl < 20) return 0;
    unsigned totlen = get16(ip + 2);
    if (totlen < 20u + 8u + NTP_PAYLOAD || (unsigned)len < 14u + totlen) return 0;
    const unsigned char* udp = ip + ihl;
    if (get16(udp) != NTP_PORT || get16(udp + 2) != UDP_SPORT) return 0;
    unsigned ulen = get16(udp + 4);
    if (ulen < 8u + NTP_PAYLOAD || 14u + ihl + ulen > (unsigned)len) return 0;
    scopy(srcip, ip + 12, 4);
    *ntp_off = 14u + ihl + 8u;
    return ulen - 8u;
}

/* ---------------------------------------------------------------- bridge i/o */
static struct nb_shared* gSh;
static unsigned char gFrame[NB_FRAME_SIZE];

static void tx_post(const unsigned char* f, unsigned len)
{
    struct nb_ring* tx = &gSh->tx;
    const unsigned ti = tx->head;
    struct nb_slot* ts = &tx->slot[ti];
    if (ts->len) return;                          /* ring full: drop */
    scopy((void*)ts->data, f, len);
    ts->len = len;
    __asm__ __volatile__("" : : : "memory");
    tx->head = (ti + 1u) % NB_RING_SLOTS;
}

/* Consume and copy out the next ready RX frame; return its length. */
static unsigned rx_take(void)
{
    struct nb_ring* rx = &gSh->rx;
    const unsigned ri = rx->tail;
    struct nb_slot* rs = &rx->slot[ri];
    const unsigned len = rs->len;
    if (!len) return 0;
    unsigned take = len > NB_FRAME_SIZE ? NB_FRAME_SIZE : len;
    scopy(gFrame, (const void*)rs->data, take);
    rs->len = 0;
    __asm__ __volatile__("" : : : "memory");
    rx->tail = (ri + 1u) % NB_RING_SLOTS;
    return take;
}

static int is_arp_reply_from_gw(const unsigned char* f, unsigned len)
{
    return len >= 42 && f[12] == 0x08 && f[13] == 0x06 &&
           f[20] == 0x00 && f[21] == 0x02 &&
           f[28] == 10 && f[29] == 0 && f[30] == 2 && f[31] == 2;
}

/* ------------------------------------------------------------------ main */
int app_main(void)
{
    char line[160];
    unsigned k;

    int base = exec2(QE_EXEC_NETBRIDGE_BASE, 0, 0);
    int size = exec2(QE_EXEC_NETBRIDGE_SIZE, 0, 0);
    if (base <= 0 || size < (int)sizeof(struct nb_shared))
        { marker("NTP: FAIL bridge discovery (execs 247/248)"); return 1; }
    gSh = (struct nb_shared*)base;
    if (gSh->magic != NB_MAGIC || gSh->version != NB_VERSION)
        { marker("NTP: FAIL bridge magic"); return 1; }

    /* ---- step 1: learn the gateway MAC (ARP, with SLIRP fallback) ---- */
    unsigned char gwmac[6];
    int arp_ok = 0;
    int flen = build_arp(gFrame, (const unsigned char*)gSh->mac);
    tx_post(gFrame, (unsigned)flen);
    for (int poll = 0; poll < 250; ++poll)
        {
        sleep_ms(20);
        unsigned len = rx_take();
        if (is_arp_reply_from_gw(gFrame, len))
            {
            scopy(gwmac, gFrame + 22, 6);        /* sender hardware = gateway */
            arp_ok = 1;
            break;
            }
        }
    if (!arp_ok)
        {
        static const unsigned char kFallback[6] =
            { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };
        scopy(gwmac, kFallback, 6);
        }

    /* ---- step 2: build and post the 48-byte NTP v4 client request ---- */
    struct ntp_ts t1 = ntp_now();                /* T1: local send time */
    unsigned char ntp[NTP_PAYLOAD];
    build_ntp_payload(ntp, t1);
    flen = build_ntp_frame(ntp, NTP_PAYLOAD, gwmac,
                           (const unsigned char*)gSh->mac,
                           gFrame, sizeof(gFrame));
    if (flen < 0) { marker("NTP: FAIL frame build"); gSh->done = 1; return 1; }
    tx_post(gFrame, (unsigned)flen);
    marker("NTP: request sent to 10.0.2.2");

    /* ---- step 3: wait for a real NTP reply in the RX ring ---- */
    int found = 0;
    long long offset_ms = 0;
    unsigned rep_stratum = 0, rep_mode = 0;
    for (int poll = 0; poll < 500; ++poll)
        {
        sleep_ms(20);
        unsigned len = rx_take();
        if (!len) continue;
        unsigned char srcip[4];
        unsigned ntp_off = 0;
        unsigned dlen = extract_ntp(gFrame, len, srcip, &ntp_off);
        if (!dlen || dlen < NTP_PAYLOAD) continue;
        const unsigned char* n = gFrame + ntp_off;
        unsigned mode = n[0] & 0x07u;
        if (mode != 4u) continue;                 /* mode 4 = server */

        struct ntp_ts T4 = ntp_now();             /* local receive time */
        struct ntp_ts T2, T3;
        T2.sec = get32(n + 32); T2.frac = get32(n + 36);  /* receive  */
        T3.sec = get32(n + 40); T3.frac = get32(n + 44);  /* transmit */

        rep_stratum = n[1];
        rep_mode = mode;
        offset_ms = ((ts_ms(T2) - ts_ms(t1)) + (ts_ms(T3) - ts_ms(T4))) / 2LL;

        k = append(line, 0, "NTP: reply from ");
        for (unsigned i = 0; i < 4; ++i)
            { if (i) line[k++] = '.'; k = dec(line, k, srcip[i]); }
        line[k] = 0;
        marker(line);

        k = append(line, 0, "NTP: reply stratum=");
        k = dec(line, k, rep_stratum);
        k = append(line, k, " mode=");
        k = dec(line, k, rep_mode);
        line[k] = 0;
        marker(line);
        found = 1;
        break;
        }

    if (found)
        {
        k = append(line, 0, "NTP: offset ");
        k = dec_signed(line, k, offset_ms);
        k = append(line, k, " ms");
        line[k] = 0;
        marker(line);
        }
    else
        {
        marker("NTP: no reply (timeout)");
        }
    marker("NTP: DONE");
    gSh->done = 1;                               /* stop the kernel poll loop */
    return found ? 0 : 2;
}
