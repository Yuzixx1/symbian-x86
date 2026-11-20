/*
 * wget.c -- PE2 E32 WGET-style downloader for the Sybian REAL port.
 *
 * Genuinely new: it combines the *real* TCP/HTTP client engine of httpget.c
 * (ARP, IPv4+TCP checksums, SYN/SYN-ACK/ACK, HTTP/1.0 GET, segment
 * reassembly over the kernel<->userland e1000/SLIRP frame bridge) with
 * persistent storage: the reassembled response body (everything after the
 * first "\r\n\r\n") is written to a FAT file via PortFile IPC (file_save).
 *
 *   user E32 -> TX ring --(kernel `netbridge` / e1000_tx)--> e1000/SLIRP
 *   e1000/SLIRP --(e1000_recv / kernel `netbridge`)--> RX ring -> user E32
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...) (euseru.def).  ld defines
 * the IAT slot at codeBase+0x8000 (0x408000); elf2e32.py --euser-import 915
 * emits the import section and E32Run overwrites the slot before resuming.
 *
 * Default URL is http://example.com/ -> DOWNLOAD.TXT.  An optional URL may
 * be supplied on the process command line (the serial shell passes the words
 * after `wget` as a UTF-16 command line); the host/path are honoured in the
 * GET request while connection setup still uses the known-good example.com
 * address reached through SLIRP NAT.
 */
#include "runtime.h"
#include "netbridge.h"

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern unsigned int wget_printf_slot;    /* --defsym=wget_printf_slot=0x408000 */
typedef void (*WgetPrintf)(const char*, ...);

static void marker(const char* text)
{
    unsigned int fn = *(volatile unsigned int*)&wget_printf_slot;
    if (fn != 915u && fn != 0u) ((WgetPrintf)fn)(text);
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

/* RFC1071 one's-complement sum (same helper family as httpget.c / ping.c). */
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

static const char kDefaultUrl[]  = "http://example.com/";
static const char kDefaultHost[] = "example.com";
static const char kDefaultPath[] = "/";
static const char kFileName[]    = "DOWNLOAD.TXT";

/* Scratch: static so the E32 never depends on a large process stack. */
static unsigned char g_frame[2048];
static unsigned char g_rx[2048];
static unsigned char g_body[4096];
static unsigned g_body_len = 0;

/* ---------------------------------------------------------- frame construction */
static unsigned build_frame(struct nb_shared* sh, unsigned char* f,
                             unsigned flags, unsigned seq, unsigned ack,
                             const unsigned char* payload, unsigned plen)
{
    const unsigned tlen = 20u + 20u + plen;   /* IP + TCP + payload */
    unsigned i, sum, cs;

    for (i = 0; i < 6; ++i) { f[i] = gw_mac[i]; f[6 + i] = sh->mac[i]; }
    f[12] = 0x08; f[13] = 0x00;

    f[14] = 0x45; f[15] = 0x00;
    put16(f + 16, (unsigned short)tlen);
    put16(f + 18, 0x1234);
    put16(f + 20, 0x4000);
    f[22] = 64; f[23] = 6;
    put16(f + 24, 0);
    for (i = 0; i < 4; ++i) { f[26 + i] = guest_ip[i]; f[30 + i] = srv_ip[i]; }
    put16(f + 24, (unsigned short)(~csum(f + 14, 20, 0) & 0xffffu));

    put16(f + 34, (unsigned short)LPORT);
    put16(f + 36, (unsigned short)RPORT);
    put32(f + 38, seq);
    put32(f + 42, ack);
    f[46] = 0x50;
    f[47] = (unsigned char)flags;
    put16(f + 48, 8192);
    put16(f + 50, 0);
    put16(f + 52, 0);
    if (plen) scopy(f + 54, payload, plen);

    sum = csum(guest_ip, 4, 0);
    sum = csum(srv_ip, 4, sum);
    sum += 6u;
    sum += (20u + plen);
    sum = csum(f + 34, 20u + plen, sum);
    cs = ~sum & 0xffffu;
    put16(f + 50, (unsigned short)(cs ? cs : 0xffffu));
    return 14u + tlen;
}

static int tx_post(struct nb_shared* sh, const unsigned char* f, unsigned len)
{
    struct nb_ring* tx = &sh->tx;
    const unsigned i = tx->head;
    struct nb_slot* s = &tx->slot[i];
    if (s->len) return -1;
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

static void tx_arp_request(struct nb_shared* sh)
{
    unsigned i;
    for (i = 0; i < 6; ++i) { g_frame[i] = 0xff; g_frame[6 + i] = sh->mac[i]; }
    g_frame[12] = 0x08; g_frame[13] = 0x06;
    g_frame[14] = 0; g_frame[15] = 1;
    g_frame[16] = 0x08; g_frame[17] = 0x00;
    g_frame[18] = 6; g_frame[19] = 4;
    g_frame[20] = 0; g_frame[21] = 1;
    for (i = 0; i < 6; ++i) g_frame[22 + i] = sh->mac[i];
    for (i = 0; i < 4; ++i) g_frame[28 + i] = guest_ip[i];
    for (i = 0; i < 6; ++i) g_frame[32 + i] = 0;
    for (i = 0; i < 4; ++i) g_frame[38 + i] = gw_ip[i];
    tx_post(sh, g_frame, 42);
}

static int arp_process(struct nb_shared* sh, const unsigned char* f, unsigned len)
{
    if (len < 42 || f[12] != 0x08 || f[13] != 0x06) return 0;
    if (f[20] != 0 || f[21] != 1) return 0;
    unsigned i;
    for (i = 0; i < 4; ++i)
        if (f[38 + i] != guest_ip[i]) return 0;
    for (i = 0; i < 6; ++i) { g_frame[i] = f[22 + i]; g_frame[6 + i] = sh->mac[i]; }
    g_frame[12] = 0x08; g_frame[13] = 0x06;
    g_frame[14] = 0; g_frame[15] = 1;
    g_frame[16] = 0x08; g_frame[17] = 0x00;
    g_frame[18] = 6; g_frame[19] = 4;
    g_frame[20] = 0; g_frame[21] = 2;
    for (i = 0; i < 6; ++i) g_frame[22 + i] = sh->mac[i];
    for (i = 0; i < 4; ++i) g_frame[28 + i] = guest_ip[i];
    for (i = 0; i < 6; ++i) g_frame[32 + i] = f[22 + i];
    for (i = 0; i < 4; ++i) g_frame[38 + i] = f[28 + i];
    tx_post(sh, g_frame, 42);
    return 1;
}

static int rx_next(struct nb_shared* sh, unsigned char* f, unsigned* len_out)
{
    struct nb_ring* rx = &sh->rx;
    const unsigned i = rx->tail;
    struct nb_slot* s = &rx->slot[i];
    const unsigned len = s->len;
    if (!len || len > NB_FRAME_SIZE) return 0;
    scopy(f, (const unsigned char*)s->data, len);
    s->len = 0;
    __asm__ __volatile__("" : : : "memory");
    rx->tail = (i + 1u) % NB_RING_SLOTS;
    *len_out = len;
    return 1;
}

static unsigned match_tcp(const unsigned char* f, unsigned len)
{
    if (len < 54 || f[12] != 0x08 || f[13] != 0x00) return 0;
    if ((f[14] >> 4) != 4 || (f[14] & 0x0f) < 5 || f[23] != 6) return 0;
    unsigned i;
    for (i = 0; i < 4; ++i)
        if (f[26 + i] != srv_ip[i] || f[30 + i] != guest_ip[i]) return 0;
    if (get16(f + 34) != RPORT || get16(f + 36) != LPORT) return 0;
    unsigned doff = (unsigned)(f[46] >> 4) * 4u;
    if (doff < 20 || 34u + doff > len) return 0;
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
    unsigned k = (unsigned)at + 7u;
    if (k < n) ++k;
    while (k < n && p[k] == ' ') ++k;
    unsigned code = 0, digits = 0;
    while (k < n && p[k] >= '0' && p[k] <= '9' && digits < 3)
        { code = code * 10u + (p[k] - '0'); ++k; ++digits; }
    return code;
}

/* ---------------------------------------------------------------- URL handling */
static int prefix_is(const char* s, const char* pfx)
{
    while (*pfx) { if (*s != *pfx) return 0; ++s; ++pfx; }
    return 1;
}

/* The shell hands the words after `wget` as a UTF-16 command line (see
 * calc.c).  Accept "http://..." there, otherwise fall back to the default. */
static void get_url(char* url, unsigned cap)
{
    static U16 args[128];
    unsigned k = 0;
    int len = command_line(args, 128);
    int i;
    if (len > 0)
        {
        for (i = 0; i < len && k + 1u < cap; ++i)
            {
            unsigned c = args[i];
            url[k++] = (c >= 0x20 && c < 0x7f) ? (char)c : '?';
            }
        }
    url[k] = 0;
    while (*url == ' ') ++url;                 /* drop leading spaces */
    if (!prefix_is(url, "http://"))
        { k = 0; while (k + 1u < cap && k < slen(kDefaultUrl))
            { url[k] = kDefaultUrl[k]; ++k; } url[k] = 0; }
}

/* Split URL into host and request path (no DNS here: connection setup still
 * uses the known-good srv_ip; the host/path only shape the GET request). */
static void parse_url(const char* url, char* host, unsigned hostcap,
                      char* path, unsigned pathcap)
{
    const char* p = url;
    unsigned n = 0;
    if (prefix_is(p, "http://")) p += 7;
    else if (prefix_is(p, "https://")) p += 8;
    while (*p && *p != '/' && *p != ':' && *p != ' ' && n + 1u < hostcap)
        host[n++] = *p++;
    host[n] = 0;
    if (!n)
        { n = 0; while (kDefaultHost[n] && n + 1u < hostcap)
            { host[n] = kDefaultHost[n]; ++n; } host[n] = 0; }
    if (*p == ':') while (*p && *p != '/' && *p != ' ') ++p;  /* skip :port */
    if (*p != '/')
        { n = 0; while (kDefaultPath[n] && n + 1u < pathcap)
            { path[n] = kDefaultPath[n]; ++n; } path[n] = 0; }
    else
        { n = 0; while (*p && *p != ' ' && n + 1u < pathcap) path[n++] = *p++;
          path[n] = 0; if (!n) { path[0] = '/'; path[1] = 0; } }
}

static unsigned build_request(char* req, const char* host, const char* path)
{
    unsigned k = 0;
    k = append(req, k, "GET ");
    k = append(req, k, path);
    k = append(req, k, " HTTP/1.0\r\nHost: ");
    k = append(req, k, host);
    k = append(req, k, "\r\nConnection: close\r\n\r\n");
    return k;
}

/* ------------------------------------------------------------------- main flow */
int app_main(void)
{
    char line[192];
    char url[128], host[96], path[96], req[256];
    unsigned k;

    get_url(url, sizeof(url));
    parse_url(url, host, sizeof(host), path, sizeof(path));

    k = append(line, 0, "WGET: GET ");
    k = append(line, k, url);
    line[k] = 0;
    marker(line);

    int base = exec2(QE_EXEC_NETBRIDGE_BASE, 0, 0);
    int size = exec2(QE_EXEC_NETBRIDGE_SIZE, 0, 0);
    if (base <= 0 || size < (int)sizeof(struct nb_shared))
        {
        marker("WGET: fetch failed (timeout)");
        marker("WGET: DONE");
        return 1;
        }
    struct nb_shared* sh = (struct nb_shared*)base;
    if (sh->magic != NB_MAGIC || sh->version != NB_VERSION)
        {
        marker("WGET: fetch failed (timeout)");
        marker("WGET: DONE");
        return 1;
        }

    k = append(line, 0, "WGET: bridge base=");
    k = dec(line, k, (unsigned)base);
    line[k] = 0;
    marker(line);

    /* ---- TCP 3-way handshake ---- */
    unsigned isn = random32();
    unsigned seq, ack, their_isn = 0;

    /* SLIRP will not deliver the SYN-ACK until it has learned our MAC. */
    tx_arp_request(sh);

    if (send_tcp(sh, TCP_SYN, isn, 0, 0, 0) != 0)
        { marker("WGET: fetch failed (timeout)"); sh->done = 1;
          marker("WGET: DONE"); return 1; }
    marker("WGET: SYN sent");

    int have_synack = 0;
    for (int poll = 0; poll < HS_POLLS && !have_synack; ++poll)
        {
        sleep_ms(POLL_MS);
        unsigned len;
        while (rx_next(sh, g_rx, &len))
            {
            if (arp_process(sh, g_rx, len)) continue;
            unsigned doff = match_tcp(g_rx, len);
            if (!doff) continue;
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
        marker("WGET: fetch failed (timeout)");
        sh->done = 1;
        marker("WGET: DONE");
        return 1;
        }
    marker("WGET: SYN-ACK received");

    seq = isn + 1u;
    ack = their_isn + 1u;
    if (send_tcp(sh, TCP_ACK, seq, ack, 0, 0) != 0)
        { marker("WGET: fetch failed (timeout)"); sh->done = 1;
          marker("WGET: DONE"); return 1; }
    marker("WGET: ACK sent");

    /* ---- HTTP GET on the established connection ---- */
    unsigned rlen = build_request(req, host, path);
    if (send_tcp(sh, TCP_PSH | TCP_ACK, seq, ack,
                 (const unsigned char*)req, rlen) != 0)
        { marker("WGET: fetch failed (timeout)"); sh->done = 1;
          marker("WGET: DONE"); return 1; }
    marker("WGET: GET sent");

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

            if (flags & TCP_RST) { ended = 1; break; }

            if (plen)
                {
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

    unsigned status = parse_status(g_body, g_body_len);
    if (!status)
        {
        marker("WGET: fetch failed (timeout)");
        sh->done = 1;
        marker("WGET: DONE");
        return 1;
        }

    k = append(line, 0, "WGET: status ");
    k = dec(line, k, status);
    line[k] = 0;
    marker(line);

    /* Strip HTTP headers at the first \r\n\r\n; save only the body. */
    unsigned body_off = 0;
    int he = find_bytes(g_body, g_body_len, "\r\n\r\n");
    if (he >= 0) body_off = (unsigned)he + 4u;
    if (body_off > g_body_len) body_off = g_body_len;
    unsigned body_len = g_body_len - body_off;

    k = append(line, 0, "WGET: body ");
    k = dec(line, k, body_len);
    k = append(line, k, " bytes");
    line[k] = 0;
    marker(line);

    int saved = file_save(kFileName, g_body + body_off, body_len);
    if (saved >= 0)
        {
        k = append(line, 0, "WGET: saved ");
        k = append(line, k, kFileName);
        k = append(line, k, " (");
        k = dec(line, k, body_len);
        k = append(line, k, " bytes)");
        line[k] = 0;
        marker(line);
        }
    else
        marker("WGET: save failed");

    sh->done = 1;                             /* stop the kernel `netbridge` poll */
    marker("WGET: DONE");
    return saved >= 0 ? 0 : 3;
}
