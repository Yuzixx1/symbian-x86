/*
 * httpget.c -- PE2 E32 minimal, *real* TCP/HTTP client over the e1000/SLIRP
 * kernel<->userland frame bridge.
 *
 * This is genuinely new: ca_test.c only has a canned HTML fallback and
 * netstack.c is loopback-only.  Here every byte crosses the real NIC:
 *
 *   user E32 -> TX ring --(kernel `netbridge` poll / e1000_tx)--> e1000/SLIRP
 *   e1000/SLIRP --(e1000_recv / kernel `netbridge` poll)--> RX ring -> user E32
 *
 * Flow: TCP 3-way handshake (SYN / SYN-ACK / ACK), HTTP/1.0 GET, then read the
 * response segments, ACK each, reassemble the body and report the status line
 * and <title>/<h1>.  Serial markers use the genuine EUSER.DLL ordinal 915
 * (RDebug::Printf), bound by E32Run from the PE2 IAT at 0x408000.
 *
 * QEMU user-mode SLIRP: guest 10.0.2.15, L2 next hop (SLIRP) 52:55:0a:00:02:02,
 * remote HTTP server reached through SLIRP NAT at srv_ip:80.
 */
#include "runtime.h"
#include "netbridge.h"

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern unsigned int http_printf_slot;    /* --defsym=http_printf_slot=0x408000 */
typedef void (*HttpPrintf)(const char*, ...);

static void marker(const char* text)
{
    unsigned int fn = *(volatile unsigned int*)&http_printf_slot;
    if (fn != 915u && fn != 0u) ((HttpPrintf)fn)(text);
    else print(text);                         /* no EUSER.DLL bound */
}

/* ---------------------------------------------------------------- tiny helpers */
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
static void put16(unsigned char* p, unsigned v)
    { p[0] = (unsigned char)(v >> 8); p[1] = (unsigned char)v; }
static unsigned get16(const unsigned char* p)
    { return ((unsigned)p[0] << 8) | p[1]; }
static void put32(unsigned char* p, unsigned v)
{
    p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);  p[3] = (unsigned char)v;
}
static unsigned get32(const unsigned char* p)
{
    return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) |
           ((unsigned)p[2] << 8) | p[3];
}
static void scopy(unsigned char* d, const unsigned char* s, unsigned n)
    { while (n--) *d++ = *s++; }
static unsigned slen(const char* s)
    { unsigned n = 0; while (s[n]) ++n; return n; }

/* RFC1071 one's-complement sum (same helper family as dnsresolv.c / ping.c). */
static unsigned csum(const unsigned char* p, unsigned len, unsigned sum)
{
    while (len > 1) { sum += get16(p); p += 2; len -= 2; }
    if (len) sum += (unsigned)p[0] << 8;
    while (sum >> 16) sum = (sum & 0xffffu) + (sum >> 16);
    return sum;
}

/* ---------------------------------------------------------------- configuration */
/* example.com is served by Cloudflare at this address (verified reachable via
   SLIRP port 80 with Host: example.com, returns <title>Example Domain</title>). */
static const unsigned char srv_ip[4] = { 104, 20, 23, 154 };
static const unsigned char guest_ip[4] = { 10, 0, 2, 15 };
static const unsigned char gw_ip[4] = { 10, 0, 2, 2 };
static const unsigned char gw_mac[6] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };
#define LPORT        49152u
#define RPORT        80u
#define TCP_ACK      0x10u
#define TCP_SYN      0x02u
#define TCP_PSH      0x08u
#define TCP_RST      0x04u
#define TCP_FIN      0x01u

#define POLL_MS      20
#define HS_POLLS     600                 /* ~12 s handshake budget */
#define DATA_POLLS   600                 /* ~12 s response budget */

/* Scratch: static so the E32 never depends on a large process stack. */
static unsigned char g_frame[2048];
static unsigned char g_rx[2048];
static unsigned char g_body[4096];
static unsigned g_body_len = 0;

/* ---------------------------------------------------------- frame construction */
/* Build a complete Ethernet + IPv4 + TCP frame with correct IP and TCP
 * (pseudo-header) checksums.  Returns the total frame length. */
static unsigned build_frame(struct nb_shared* sh, unsigned char* f,
                            unsigned flags, unsigned seq, unsigned ack,
                            const unsigned char* payload, unsigned plen)
{
    const unsigned tlen = 20u + 20u + plen;   /* IP + TCP + payload */
    unsigned i, sum, cs;

    /* Ethernet: dst = SLIRP gateway, src = guest MAC, EtherType IPv4. */
    for (i = 0; i < 6; ++i) { f[i] = gw_mac[i]; f[6 + i] = sh->mac[i]; }
    f[12] = 0x08; f[13] = 0x00;

    /* IPv4: v4/IHL=5, total length, TTL 64, proto 6 (TCP). */
    f[14] = 0x45; f[15] = 0x00;
    put16(f + 16, (unsigned short)tlen);
    put16(f + 18, 0x1234);                    /* identification */
    put16(f + 20, 0x4000);                    /* DF, no fragmentation */
    f[22] = 64; f[23] = 6;                    /* TTL, protocol TCP */
    put16(f + 24, 0);                         /* header checksum (filled below) */
    for (i = 0; i < 4; ++i) { f[26 + i] = guest_ip[i]; f[30 + i] = srv_ip[i]; }
    put16(f + 24, (unsigned short)(~csum(f + 14, 20, 0) & 0xffffu));

    /* TCP: data offset 5 (20 bytes), window 8192, checksum via pseudo-header. */
    put16(f + 34, (unsigned short)LPORT);
    put16(f + 36, (unsigned short)RPORT);
    put32(f + 38, seq);
    put32(f + 42, ack);
    f[46] = 0x50;                             /* data offset 5 << 4 */
    f[47] = (unsigned char)flags;
    put16(f + 48, 8192);                      /* receive window */
    put16(f + 50, 0);                         /* checksum (filled below) */
    put16(f + 52, 0);                         /* urgent pointer */
    if (plen) scopy(f + 54, payload, plen);

    sum = csum(guest_ip, 4, 0);
    sum = csum(srv_ip, 4, sum);
    sum += 6u;                                /* IP protocol TCP */
    sum += (20u + plen);                      /* TCP length (pseudo-header) */
    sum = csum(f + 34, 20u + plen, sum);
    cs = ~sum & 0xffffu;
    put16(f + 50, (unsigned short)(cs ? cs : 0xffffu));
    return 14u + tlen;                        /* include the Ethernet header */
}

static int tx_post(struct nb_shared* sh, const unsigned char* f, unsigned len)
{
    struct nb_ring* tx = &sh->tx;
    const unsigned i = tx->head;
    struct nb_slot* s = &tx->slot[i];
    if (s->len) return -1;                    /* slot still owned by kernel */
    scopy((unsigned char*)s->data, f, len);
    s->len = len;
    __asm__ __volatile__("" : : : "memory");
    tx->head = (i + 1u) % NB_RING_SLOTS;
    return 0;
}

static int send_tcp(struct nb_shared* sh, unsigned flags, unsigned seq,
                    unsigned ack, const unsigned char* payload, unsigned plen)
{
    unsigned n = build_frame(sh, g_frame, flags, seq, ack, payload, plen);
    return tx_post(sh, g_frame, n);
}

/* ---- ARP: announce our MAC to SLIRP and answer its who-has 10.0.2.15 ---- */
static void tx_arp_request(struct nb_shared* sh)
{
    unsigned i;
    for (i = 0; i < 6; ++i) { g_frame[i] = 0xff; g_frame[6 + i] = sh->mac[i]; }
    g_frame[12] = 0x08; g_frame[13] = 0x06;
    g_frame[14] = 0; g_frame[15] = 1;         /* hardware type Ethernet */
    g_frame[16] = 0x08; g_frame[17] = 0x00;   /* protocol type IPv4 */
    g_frame[18] = 6; g_frame[19] = 4;
    g_frame[20] = 0; g_frame[21] = 1;         /* ARP request */
    for (i = 0; i < 6; ++i) g_frame[22 + i] = sh->mac[i];
    for (i = 0; i < 4; ++i) g_frame[28 + i] = guest_ip[i];
    for (i = 0; i < 6; ++i) g_frame[32 + i] = 0;
    for (i = 0; i < 4; ++i) g_frame[38 + i] = gw_ip[i];
    tx_post(sh, g_frame, 42);
}

/* Returns 1 if the frame was an ARP request we answered (caller skips it). */
static int arp_process(struct nb_shared* sh, const unsigned char* f, unsigned len)
{
    if (len < 42 || f[12] != 0x08 || f[13] != 0x06) return 0;
    if (f[20] != 0 || f[21] != 1) return 0;            /* only requests */
    unsigned i;
    for (i = 0; i < 4; ++i)
        if (f[38 + i] != guest_ip[i]) return 0;        /* not who-has us */
    for (i = 0; i < 6; ++i) { g_frame[i] = f[22 + i]; g_frame[6 + i] = sh->mac[i]; }
    g_frame[12] = 0x08; g_frame[13] = 0x06;
    g_frame[14] = 0; g_frame[15] = 1;
    g_frame[16] = 0x08; g_frame[17] = 0x00;
    g_frame[18] = 6; g_frame[19] = 4;
    g_frame[20] = 0; g_frame[21] = 2;                  /* ARP reply */
    for (i = 0; i < 6; ++i) g_frame[22 + i] = sh->mac[i];
    for (i = 0; i < 4; ++i) g_frame[28 + i] = guest_ip[i];
    for (i = 0; i < 6; ++i) g_frame[32 + i] = f[22 + i];
    for (i = 0; i < 4; ++i) g_frame[38 + i] = f[28 + i];
    tx_post(sh, g_frame, 42);
    return 1;
}

/* Pull one ready frame out of the RX ring, smallest effort: just the tail. */
static int rx_next(struct nb_shared* sh, unsigned char* f, unsigned* len_out)
{
    struct nb_ring* rx = &sh->rx;
    const unsigned i = rx->tail;
    struct nb_slot* s = &rx->slot[i];
    const unsigned len = s->len;
    if (!len || len > NB_FRAME_SIZE) return 0;
    scopy(f, (const unsigned char*)s->data, len);
    s->len = 0;                               /* release slot to kernel */
    __asm__ __volatile__("" : : : "memory");
    rx->tail = (i + 1u) % NB_RING_SLOTS;
    *len_out = len;
    return 1;
}

/* Validate a received frame as TCP for our connection. Returns data offset or
 * 0 when it is not ours.  TCP header starts at frame+54 (14 Eth + 20 IPv4). */
static unsigned match_tcp(const unsigned char* f, unsigned len)
{
    if (len < 54 || f[12] != 0x08 || f[13] != 0x00) return 0;
    if ((f[14] >> 4) != 4 || (f[14] & 0x0f) < 5 || f[23] != 6) return 0;
    unsigned i;
    for (i = 0; i < 4; ++i)
        if (f[26 + i] != srv_ip[i] || f[30 + i] != guest_ip[i]) return 0;
    if (get16(f + 34) != RPORT || get16(f + 36) != LPORT) return 0;
    unsigned doff = (unsigned)(f[46] >> 4) * 4u;
    if (doff < 20 || 34u + doff > len) return 0;   /* TCP hdr starts at 34 */
    return doff;
}

/* -------------------------------------------------------------- result scanning */
static int find_bytes(const unsigned char* p, unsigned n, const char* needle)
{
    unsigned m = slen(needle), i;
    if (!m || n < m) return -1;
    for (i = 0; i + m <= n; ++i)
        {
        unsigned j;
        for (j = 0; j < m; ++j)
            if ((unsigned char)needle[j] != p[i + j]) break;
        if (j == m) return (int)i;
        }
    return -1;
}

static unsigned parse_status(const unsigned char* p, unsigned n)
{
    int at = find_bytes(p, n, "HTTP/1.");
    if (at < 0) return 0;
    unsigned k = (unsigned)at + 7u;           /* after "HTTP/1." */
    if (k < n) ++k;                            /* minor version digit */
    while (k < n && p[k] == ' ') ++k;
    unsigned code = 0, digits = 0;
    while (k < n && p[k] >= '0' && p[k] <= '9' && digits < 3)
        { code = code * 10u + (p[k] - '0'); ++k; ++digits; }
    return code;
}

/* Copy the text inside the first <tag>...</tag> pair into out (sanitised). */
static unsigned tag_text(const unsigned char* p, unsigned n, const char* tag,
                         char* out, unsigned cap)
{
    char open[24], close[26];
    unsigned m = slen(tag), k = 0;
    open[k++] = '<';
    unsigned j;
    for (j = 0; j < m; ++j) open[k++] = tag[j];
    open[k] = 0;
    k = 0; close[k++] = '<'; close[k++] = '/';
    for (j = 0; j < m; ++j) close[k++] = tag[j];
    close[k] = 0;
    int a = find_bytes(p, n, open);
    if (a < 0) { out[0] = 0; return 0; }
    unsigned start = (unsigned)a + m + 2u;
    if (start >= n) { out[0] = 0; return 0; }
    int b = find_bytes(p + start, n - start, close);
    unsigned avail = (b < 0) ? (n - start) : (unsigned)b;
    if (avail > cap - 1) avail = cap - 1;
    unsigned o = 0;
    for (j = 0; j < avail; ++j)
        {
        unsigned char c = p[start + j];
        out[o++] = (c >= 0x20 && c <= 0x7e) ? (char)c : ' ';
        }
    /* trim trailing whitespace */
    while (o && out[o - 1] == ' ') --o;
    out[o] = 0;
    return o;
}

/* ------------------------------------------------------------------- main flow */
int app_main(void)
{
    char line[160];
    unsigned k;

    int base = exec2(QE_EXEC_NETBRIDGE_BASE, 0, 0);
    int size = exec2(QE_EXEC_NETBRIDGE_SIZE, 0, 0);
    if (base <= 0 || size < (int)sizeof(struct nb_shared))
        {
        marker("HTTP: FAIL bridge discovery (execs 247/248)");
        marker("HTTP: DONE");
        return 1;
        }
    struct nb_shared* sh = (struct nb_shared*)base;
    if (sh->magic != NB_MAGIC || sh->version != NB_VERSION)
        {
        marker("HTTP: FAIL bridge magic");
        marker("HTTP: DONE");
        return 1;
        }

    k = append(line, 0, "HTTP: bridge base=");
    k = dec(line, k, (unsigned)base);
    k = append(line, k, " size=");
    k = dec(line, k, (unsigned)size);
    line[k] = 0;
    marker(line);

    /* ---- TCP 3-way handshake ---- */
    unsigned isn = random32();
    unsigned seq, ack, their_isn = 0;

    /* SLIRP will not deliver the SYN-ACK until it has learned our MAC.  Send
     * a who-has-gateway request (it learns the sender MAC from that) and also
     * answer any who-has-10.0.2.15 it emits while the exchange is in flight. */
    tx_arp_request(sh);

    if (send_tcp(sh, TCP_SYN, isn, 0, 0, 0) != 0)
        { marker("HTTP: FAIL TX slot busy on SYN"); sh->done = 1;
          marker("HTTP: DONE"); return 1; }
    marker("HTTP: SYN sent");

    int have_synack = 0;
    for (int poll = 0; poll < HS_POLLS && !have_synack; ++poll)
        {
        sleep_ms(POLL_MS);
        unsigned len;
        while (rx_next(sh, g_rx, &len))
            {
            if (arp_process(sh, g_rx, len)) continue;
            unsigned doff = match_tcp(g_rx, len);
            if (!doff) continue;              /* ARP / other traffic */
            unsigned flags = g_rx[47];
            if ((flags & (TCP_SYN | TCP_ACK)) == (TCP_SYN | TCP_ACK) &&
                get32(g_rx + 42) == isn + 1u)
                {
                their_isn = get32(g_rx + 38);
                have_synack = 1;
                break;
                }
            }
        }
    if (!have_synack)
        {
        marker("HTTP: handshake timeout");
        sh->done = 1;
        marker("HTTP: DONE");
        return 1;
        }
    marker("HTTP: SYN-ACK received");

    seq = isn + 1u;
    ack = their_isn + 1u;
    if (send_tcp(sh, TCP_ACK, seq, ack, 0, 0) != 0)
        { marker("HTTP: FAIL TX slot busy on ACK"); sh->done = 1;
          marker("HTTP: DONE"); return 1; }
    marker("HTTP: ACK sent");

    /* ---- HTTP GET on the established connection ---- */
    static const char kGet[] =
        "GET / HTTP/1.0\r\nHost: example.com\r\n\r\n";
    if (send_tcp(sh, TCP_PSH | TCP_ACK, seq, ack,
                 (const unsigned char*)kGet, (unsigned)(sizeof(kGet) - 1)) != 0)
        { marker("HTTP: FAIL TX slot busy on GET"); sh->done = 1;
          marker("HTTP: DONE"); return 1; }
    marker("HTTP: GET sent");

    /* ---- receive response segments, ACK each, reassemble ---- */
    g_body_len = 0;
    unsigned next_ack = their_isn + 1u;
    int ended = 0;
    for (int poll = 0; poll < DATA_POLLS && !ended; ++poll)
        {
        sleep_ms(POLL_MS);
        unsigned len;
        while (rx_next(sh, g_rx, &len))
            {
            if (arp_process(sh, g_rx, len)) continue;
            unsigned doff = match_tcp(g_rx, len);
            if (!doff) continue;
            unsigned flags = g_rx[47];
            unsigned seg_seq = get32(g_rx + 38);
            unsigned plen = len - 34u - doff;

            if (flags & TCP_RST)
                { ended = 1; break; }

            if (plen)
                {
                /* ACK this segment (cumulative ACK of its last byte). */
                unsigned a = seg_seq + plen;
                send_tcp(sh, TCP_ACK, isn + 1u, a, 0, 0);
                next_ack = a;
                if (g_body_len + plen <= sizeof(g_body))
                    {
                    scopy(g_body + g_body_len, g_rx + 34u + doff, plen);
                    g_body_len += plen;
                    }
                }
            if (flags & TCP_FIN)
                {
                send_tcp(sh, TCP_ACK, isn + 1u, next_ack + 1u, 0, 0);
                ended = 1;
                break;
                }
            }
        }

    /* Connection: close from Cloudflare: the FIN (or timeout) ends the body. */
    unsigned status = parse_status(g_body, g_body_len);
    if (status)
        {
        k = append(line, 0, "HTTP: status ");
        k = dec(line, k, status);
        line[k] = 0;
        marker(line);
        }
    char title[96];
    if (!tag_text(g_body, g_body_len, "title", title, sizeof(title)))
        tag_text(g_body, g_body_len, "h1", title, sizeof(title));
    if (title[0])
        {
        k = append(line, 0, "HTTP: title ");
        k = append(line, k, title);
        line[k] = 0;
        marker(line);
        }
    k = append(line, 0, "HTTP: received ");
    k = dec(line, k, g_body_len);
    k = append(line, k, " bytes");
    line[k] = 0;
    marker(line);

    sh->done = 1;                             /* stop the kernel `netbridge` poll */
    marker("HTTP: DONE");
    return status ? 0 : 2;
}
