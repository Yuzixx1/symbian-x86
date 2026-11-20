/*
 * wbrowser.c -- PE2 E32 *real* web browser over the e1000/SLIRP netbridge.
 *
 * This is genuinely new: ca_test.c only has a canned HTML fallback.  Every
 * byte here crosses the real NIC through the kernel<->userland frame bridge:
 *
 *   user E32 -> TX ring --(kernel `netbridge` / e1000_tx)--> e1000/SLIRP
 *   e1000/SLIRP --(e1000_recv / kernel `netbridge`)--> RX ring -> user E32
 *
 * Flow: take an optional URL from PortCon (default http://example.com/),
 * parse host+path, TCP 3-way handshake (SYN / SYN-ACK / ACK), HTTP/1.0 GET,
 * read+ACK+reassemble the response, then render it as readable text (skip the
 * HTTP headers and <head>/<script>/<style>, strip tags, decode entities,
 * collapse whitespace and wrap long lines).  Serial markers use the genuine
 * EUSER.DLL ordinal 915 (RDebug::Printf), bound by E32Run from the PE2 IAT at
 * 0x408000 (symbol wbrowser_printf_slot).
 *
 * QEMU user-mode SLIRP: guest 10.0.2.15, L2 next hop (SLIRP) 52:55:0a:00:02:02,
 * the remote HTTP server is reached through SLIRP NAT at srv_ip:80 and the
 * Host: header is taken from the URL (so vhosts such as example.com work).
 */
#include "runtime.h"
#include "netbridge.h"

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern unsigned int wbrowser_printf_slot;    /* --defsym=wbrowser_printf_slot=0x408000 */
typedef void (*WebPrintf)(const char*, ...);

static void marker(const char* text)
{
    unsigned int fn = *(volatile unsigned int*)&wbrowser_printf_slot;
    if (fn != 915u && fn != 0u) ((WebPrintf)fn)(text);
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
/* The SLIRP "known path": example.com's Cloudflare front end, reachable on
 * port 80 through the 10.0.2.2 NAT.  The Host: header is set from the URL. */
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
#define URL_BUF      128

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

/* ---- ARP: announce our MAC to SLIRP and answer its who-has 10.0.2.15 ---- */
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

/* Returns 1 if the frame was an ARP request we answered (caller skips it). */
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

/* ------------------------------------------------------------------ URL parsing */
static void parse_url(const char* url, char* host, unsigned hcap,
                      char* path, unsigned pcap)
{
    const char* p = url;
    unsigned h = 0, q = 0;
    const char* s = url;
    while (s[0] && s[1] && !(s[0] == ':' && s[1] == '/')) ++s;
    if (s[0] == ':' && s[1] == '/')              /* skip http:// */
        { p = s + 2; if (*p == '/') ++p; }
    while (*p && *p != '/' && *p != '?' && *p != ':' && h + 1 < hcap)
        host[h++] = *p++;
    host[h] = 0;
    if (*p == ':')                              /* optional :port */
        while (*p && *p != '/' && *p != '?') ++p;
    if (*p != '/') { path[0] = '/'; path[1] = 0; return; }
    while (*p && *p != ' ' && q + 1 < pcap) path[q++] = *p++;
    path[q] = 0;
}

static unsigned build_get(char* dst, const char* path, const char* host)
{
    unsigned k = append(dst, 0, "GET ");
    k = append(dst, k, path);
    k = append(dst, k, " HTTP/1.0\r\nHost: ");
    k = append(dst, k, host);
    k = append(dst, k, "\r\n\r\n");
    dst[k] = 0;
    return k;
}

/* --------------------------------------------------------------- HTML renderer */
/* Case-insensitive match of a tag name at p[len], requiring a name boundary. */
static int tag_is(const unsigned char* p, unsigned avail, const char* name)
{
    unsigned m = slen(name), i;
    for (i = 0; i < m && i < avail; ++i)
        {
        unsigned a = p[i], b = (unsigned char)name[i];
        if (a >= 'A' && a <= 'Z') a += 32;
        if (b >= 'A' && b <= 'Z') b += 32;
        if (a != b) return 0;
        }
    if (i != m) return 0;
    if (i < avail && ((p[i] >= 'a' && p[i] <= 'z') ||
                      (p[i] >= 'A' && p[i] <= 'Z') ||
                      (p[i] >= '0' && p[i] <= '9'))) return 0;
    return 1;
}

/* Decode &amp; &lt; &gt; &quot; &nbsp; &#NN; (and &#xHH;), return bytes used. */
static int entity(const unsigned char* p, unsigned avail, char* out)
{
    static const char* kName[5] = { "amp;", "lt;", "gt;", "quot;", "nbsp;" };
    static const char  kVal[5]  = { '&',   '<',   '>',   '"',     ' '    };
    if (!avail) return 0;
    for (unsigned i = 0; i < 5; ++i)
        {
        unsigned m = slen(kName[i]);
        if (avail >= m + 1 && tag_is(p + 1, avail - 1, kName[i]))
            { *out = kVal[i]; return (int)m + 1; }
        }
    if (avail >= 3 && p[1] == '#')
        {
        unsigned k = 2, base = 10, v = 0, digits = 0;
        if (avail >= 4 && (p[2] == 'x' || p[2] == 'X'))
            { base = 16; k = 3; }
        while (k < avail && p[k] != ';' && digits < 5)
            {
            unsigned d;
            if (p[k] >= '0' && p[k] <= '9') d = p[k] - '0';
            else if (base == 16 && p[k] >= 'a' && p[k] <= 'f') d = p[k] - 'a' + 10;
            else if (base == 16 && p[k] >= 'A' && p[k] <= 'F') d = p[k] - 'A' + 10;
            else break;
            v = v * base + d; ++k; ++digits;
            }
        if (digits && k < avail && p[k] == ';')
            { *out = (char)(v & 0x7fu); return (int)k + 1; }
        }
    return 0;
}

/* Decode entities, drop non-printables, collapse whitespace runs to one space. */
static unsigned sanitize(const unsigned char* p, unsigned n, char* out, unsigned cap)
{
    unsigned o = 0;
    int pending = 0;
    for (unsigned i = 0; i < n; ++i)
        {
        unsigned char c = p[i];
        if (c == '&')
            {
            char d;
            int used = entity(p + i, n - i, &d);
            if (used > 0) { c = (unsigned char)d; i += (unsigned)(used - 1); }
            }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
            { pending = 1; continue; }
        if (c < 0x20 || c > 0x7e) continue;
        if (pending && o && out[o - 1] != ' ' && o + 1 < cap) out[o++] = ' ';
        pending = 0;
        if (o + 1 < cap) out[o++] = (char)c;
        }
    while (o && out[o - 1] == ' ') --o;
    out[o] = 0;
    return o;
}

/* Copy the raw text inside the first <tag>...</tag> pair. */
static unsigned tag_raw(const unsigned char* p, unsigned n, const char* tag,
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
    for (j = 0; j < avail; ++j) out[j] = (char)p[start + j];
    out[avail] = 0;
    return avail;
}

/* Strip HTTP headers, <head>/<script>/<style> blocks and all tags into raw text. */
static unsigned render_html(const unsigned char* html, unsigned n,
                            char* out, unsigned cap)
{
    unsigned start = 0, o = 0;
    int at = find_bytes(html, n, "\r\n\r\n");
    if (at >= 0) start = (unsigned)at + 4u;
    else { at = find_bytes(html, n, "\n\n"); if (at >= 0) start = (unsigned)at + 2u; }

    int in_tag = 0, skip = 0;
    for (unsigned i = start; i < n; ++i)
        {
        unsigned char c = html[i];
        if (c == '<')
            {
            unsigned avail = n - i - 1u;
            if (tag_is(html + i + 1, avail, "head")) skip = 1;
            else if (tag_is(html + i + 1, avail, "script")) skip = 2;
            else if (tag_is(html + i + 1, avail, "style")) skip = 3;
            else if (tag_is(html + i + 1, avail, "/head")) skip = 0;
            else if (tag_is(html + i + 1, avail, "/script")) skip = 0;
            else if (tag_is(html + i + 1, avail, "/style")) skip = 0;
            in_tag = 1;
            continue;
            }
        if (c == '>') { in_tag = 0; continue; }
        if (in_tag || skip) continue;
        if (o + 1 < cap) out[o++] = (char)c;
        }
    out[o] = 0;
    return o;
}

static void emit_wrapped(const char* text)
{
    char line[96], msg[128];
    unsigned k = 0, i = 0, n = slen(text);
    while (i < n)
        {
        while (i < n && text[i] == ' ') ++i;
        if (i >= n) break;
        unsigned ws = i;
        while (i < n && text[i] != ' ') ++i;
        unsigned w = i - ws;
        if (w > 72) w = 72;                       /* hard-break huge tokens */
        if (k && k + 1u + w > 72u)
            {
            line[k] = 0;
            unsigned m = append(msg, 0, "WEB: ");
            m = append(msg, m, line); msg[m] = 0;
            marker(msg);
            k = 0;
            }
        if (k) line[k++] = ' ';
        for (unsigned j = 0; j < w; ++j) line[k++] = text[ws + j];
        }
    if (k)
        {
        line[k] = 0;
        unsigned m = append(msg, 0, "WEB: ");
        m = append(msg, m, line); msg[m] = 0;
        marker(msg);
        }
}

/* ------------------------------------------------------------------- main flow */
int app_main(void)
{
    char line[160], url[URL_BUF], host[URL_BUF], path[URL_BUF], get[192];
    unsigned k;

    /* Optional URL from PortCon; short timeout -> default when unattended. */
    static const char kDefaultUrl[] = "http://example.com/";
    url[0] = 0;
    int r = read_line(url, sizeof(url), 400);
    if (r < 0 || url[0] == 0)
        {
        unsigned u = 0;
        while (kDefaultUrl[u] && u + 1 < sizeof(url))
            { url[u] = kDefaultUrl[u]; ++u; }
        url[u] = 0;
        }
    parse_url(url, host, sizeof(host), path, sizeof(path));
    if (!host[0]) { marker("WEB: FAIL bad URL"); marker("WEB: DONE"); return 1; }

    k = append(line, 0, "WEB: GET ");
    k = append(line, k, url);
    line[k] = 0;
    marker(line);

    int base = exec2(QE_EXEC_NETBRIDGE_BASE, 0, 0);
    int size = exec2(QE_EXEC_NETBRIDGE_SIZE, 0, 0);
    if (base <= 0 || size < (int)sizeof(struct nb_shared))
        { marker("WEB: FAIL bridge discovery (execs 247/248)");
          marker("WEB: DONE"); return 1; }
    struct nb_shared* sh = (struct nb_shared*)base;
    if (sh->magic != NB_MAGIC || sh->version != NB_VERSION)
        { marker("WEB: FAIL bridge magic"); marker("WEB: DONE"); return 1; }

    k = append(line, 0, "WEB: bridge base=");
    k = dec(line, k, (unsigned)base);
    k = append(line, k, " size=");
    k = dec(line, k, (unsigned)size);
    line[k] = 0;
    marker(line);

    /* ---- TCP 3-way handshake ---- */
    unsigned isn = random32();
    unsigned seq, ack, their_isn = 0;

    tx_arp_request(sh);
    if (send_tcp(sh, TCP_SYN, isn, 0, 0, 0) != 0)
        { marker("WEB: FAIL TX slot busy on SYN"); sh->done = 1;
          marker("WEB: fetch failed (timeout)"); marker("WEB: DONE"); return 1; }

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
        sh->done = 1;
        marker("WEB: fetch failed (timeout)");
        marker("WEB: DONE");
        return 1;
        }

    seq = isn + 1u;
    ack = their_isn + 1u;
    if (send_tcp(sh, TCP_ACK, seq, ack, 0, 0) != 0)
        { sh->done = 1; marker("WEB: fetch failed (timeout)");
          marker("WEB: DONE"); return 1; }

    /* ---- HTTP GET on the established connection ---- */
    unsigned glen = build_get(get, path, host);
    if (send_tcp(sh, TCP_PSH | TCP_ACK, seq, ack,
                 (const unsigned char*)get, glen) != 0)
        { sh->done = 1; marker("WEB: fetch failed (timeout)");
          marker("WEB: DONE"); return 1; }

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
    sh->done = 1;                             /* stop the kernel `netbridge` poll */

    unsigned status = parse_status(g_body, g_body_len);
    if (!status || !g_body_len)
        {
        marker("WEB: fetch failed (timeout)");
        marker("WEB: DONE");
        return 1;
        }

    k = append(line, 0, "WEB: status ");
    k = dec(line, k, status);
    line[k] = 0;
    marker(line);

    /* title: raw <title> text -> decoded/collapsed */
    static char raw[160], title[160];
    if (!tag_raw(g_body, g_body_len, "title", raw, sizeof(raw)))
        tag_raw(g_body, g_body_len, "h1", raw, sizeof(raw));
    sanitize((const unsigned char*)raw, slen(raw), title, sizeof(title));
    if (title[0])
        {
        k = append(line, 0, "WEB: title ");
        k = append(line, k, title);
        line[k] = 0;
        marker(line);
        }

    /* body: strip tags, then decode entities / collapse whitespace / wrap */
    static char text[2048], clean[2048];
    unsigned tlen = render_html(g_body, g_body_len, text, sizeof(text));
    sanitize((const unsigned char*)text, tlen, clean, sizeof(clean));
    if (clean[0]) emit_wrapped(clean);

    k = append(line, 0, "WEB: bytes ");
    k = dec(line, k, g_body_len);
    line[k] = 0;
    marker(line);
    marker("WEB: DONE");
    return 0;
}
