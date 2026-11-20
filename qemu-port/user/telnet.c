/*
 * telnet.c -- PE2 E32 TELNET-style interactive TCP client over the e1000/SLIRP
 * kernel<->userland frame bridge.
 *
 * Genuinely new application (distinct from httpget.c / wbrowser.c / wget.c):
 * after a real TCP 3-way handshake it sends a short interactive line and
 * behaves like a line-oriented terminal -- it polls the RX ring for echoed /
 * response bytes and prints the printable text received on the serial console.
 *
 * Every byte crosses the real NIC:
 *   user E32 -> TX ring --(kernel `netbridge` poll / e1000_tx)--> e1000/SLIRP
 *   e1000/SLIRP --(e1000_recv / kernel `netbridge` poll)--> RX ring -> user E32
 *
 * The target defaults to the SLIRP gateway 10.0.2.2:80 (handshake/timeout is
 * still fully exercised when nothing listens there).  An optional "host port"
 * line typed on PortCon at startup overrides it, e.g. "104.20.23.154 80".
 * Typing "quit" at that prompt (or 'q' during the receive phase) ends early.
 *
 * Serial markers use the genuine EUSER.DLL ordinal 915 (RDebug::Printf), bound
 * by E32Run from the PE2 IAT at 0x408000.
 */
#include "runtime.h"
#include "netbridge.h"

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern unsigned int telnet_printf_slot;   /* --defsym=telnet_printf_slot=0x408000 */
typedef void (*TelnetPrintf)(const char*, ...);

static void marker(const char* text)
{
    unsigned int fn = *(volatile unsigned int*)&telnet_printf_slot;
    if (fn != 915u && fn != 0u) ((TelnetPrintf)fn)(text);
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

/* RFC1071 one's-complement sum (same helper family as httpget.c / ping.c). */
static unsigned csum(const unsigned char* p, unsigned len, unsigned sum)
{
    while (len > 1) { sum += get16(p); p += 2; len -= 2; }
    if (len) sum += (unsigned)p[0] << 8;
    while (sum >> 16) sum = (sum & 0xffffu) + (sum >> 16);
    return sum;
}

/* ---------------------------------------------------------------- configuration */
static unsigned char rmt_ip[4] = { 10, 0, 2, 2 };  /* SLIRP gateway by default */
static unsigned       rmt_port  = 80u;
static const unsigned char guest_ip[4] = { 10, 0, 2, 15 };
static const unsigned char gw_ip[4]    = { 10, 0, 2, 2 };
static const unsigned char gw_mac[6]   = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };
#define LPORT        49153u
#define TCP_ACK      0x10u
#define TCP_SYN      0x02u
#define TCP_PSH      0x08u
#define TCP_RST      0x04u
#define TCP_FIN      0x01u

#define POLL_MS      20
#define HS_POLLS     600                 /* ~12 s handshake budget */
#define DATA_POLLS   600                 /* ~12 s receive budget */

static const char kHello[] = "EKA2-TELNET-HELLO\r\n";

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
    f[12] = 0x08; f[13] = 0x00;               /* EtherType IPv4 */

    f[14] = 0x45; f[15] = 0x00;
    put16(f + 16, (unsigned short)tlen);
    put16(f + 18, 0x1234);
    put16(f + 20, 0x4000);                    /* DF */
    f[22] = 64; f[23] = 6;                    /* TTL, protocol TCP */
    put16(f + 24, 0);
    for (i = 0; i < 4; ++i) { f[26 + i] = guest_ip[i]; f[30 + i] = rmt_ip[i]; }
    put16(f + 24, (unsigned short)(~csum(f + 14, 20, 0) & 0xffffu));

    put16(f + 34, (unsigned short)LPORT);
    put16(f + 36, (unsigned short)rmt_port);
    put32(f + 38, seq);
    put32(f + 42, ack);
    f[46] = 0x50;                             /* data offset 5 */
    f[47] = (unsigned char)flags;
    put16(f + 48, 8192);                      /* receive window */
    put16(f + 50, 0);                         /* checksum (filled below) */
    put16(f + 52, 0);
    if (plen) scopy(f + 54, payload, plen);

    sum = csum(guest_ip, 4, 0);
    sum = csum(rmt_ip, 4, sum);
    sum += 6u;                                /* IP protocol TCP */
    sum += (20u + plen);                      /* TCP length (pseudo-header) */
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
    if (f[20] != 0 || f[21] != 1) return 0;            /* requests only */
    unsigned i;
    for (i = 0; i < 4; ++i)
        if (f[38 + i] != guest_ip[i]) return 0;
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

/* Validate a received frame as TCP for our connection; return data offset. */
static unsigned match_tcp(const unsigned char* f, unsigned len)
{
    if (len < 54 || f[12] != 0x08 || f[13] != 0x00) return 0;
    if ((f[14] >> 4) != 4 || (f[14] & 0x0f) < 5 || f[23] != 6) return 0;
    unsigned i;
    for (i = 0; i < 4; ++i)
        if (f[26 + i] != rmt_ip[i] || f[30 + i] != guest_ip[i]) return 0;
    if (get16(f + 34) != rmt_port || get16(f + 36) != LPORT) return 0;
    unsigned doff = (unsigned)(f[46] >> 4) * 4u;
    if (doff < 20 || 34u + doff > len) return 0;
    return doff;
}

/* -------------------------------------------------------------- config parsing */
static unsigned parse_uint(const char** p)
{
    unsigned v = 0;
    while (**p >= '0' && **p <= '9') { v = v * 10u + (unsigned)(**p - '0'); ++*p; }
    return v;
}

/* Parse "a.b.c.d port" into rmt_ip/rmt_port. Returns 1 on success. */
static int parse_target(const char* s)
{
    unsigned ip[4], port, i;
    for (i = 0; i < 4; ++i)
        {
        ip[i] = parse_uint(&s);
        if (i < 3) { if (*s != '.') return 0; ++s; }
        if (ip[i] > 255u) return 0;
        }
    while (*s == ' ') ++s;
    port = parse_uint(&s);
    while (*s == ' ') ++s;
    if (*s || !port || port > 65535u) return 0;
    for (i = 0; i < 4; ++i) rmt_ip[i] = (unsigned char)ip[i];
    rmt_port = port;
    return 1;
}

static unsigned format_target(char* out, unsigned k)
{
    k = append(out, k, "TNET: connecting to ");
    k = dec(out, k, rmt_ip[0]); k = append(out, k, ".");
    k = dec(out, k, rmt_ip[1]); k = append(out, k, ".");
    k = dec(out, k, rmt_ip[2]); k = append(out, k, ".");
    k = dec(out, k, rmt_ip[3]); k = append(out, k, ":");
    k = dec(out, k, rmt_port);
    out[k] = 0;
    return k;
}

static int word_is(const char* w, unsigned n, const char* lit)
{
    unsigned i = 0;
    while (i < n && lit[i])
        {
        char a = w[i], b = lit[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (a != b) return 0;
        ++i;
        }
    return (i == n && lit[i] == 0) ? 1 : 0;
}

/* Non-blocking 'q' check during the receive phase (the shell is then inside
 * its `netbridge` poll loop, so it is not itself consuming PortCon bytes). */
static int quit_key(void)
{
    int c = con_tryread();
    return (c == 'q' || c == 'Q') ? 1 : 0;
}

/* Shell arguments ("telnet host port") arrive as the E32 command line; an
 * interactive PortCon line is the fallback. UTF-16 -> narrow, low byte only. */
static int copy_command_line(char* out, unsigned cap)
{
    U16 wide[160];
    int n = command_line(wide, 160);
    if (n < 0) return n;
    if ((unsigned)n >= cap) return -9;
    for (int i = 0; i < n; ++i) out[i] = (char)(wide[i] & 0xff);
    out[n] = 0;
    return n;
}

/* ------------------------------------------------------------------- main flow */
int app_main(void)
{
    char line[160];
    unsigned k;

    /* Optional target / quit: prefer the shell command line, then PortCon. */
    int r = copy_command_line(line, sizeof(line));
    if (r <= 0 || line[0] == 0)
        r = read_line(line, sizeof(line), 400);
    if (r >= 0 && line[0])
        {
        const char* w = line;
        const char* e = line;
        while (*e && *e != ' ') ++e;
        if (word_is(w, (unsigned)(e - w), "quit") ||
            word_is(w, (unsigned)(e - w), "q"))
            {
            marker("TNET: quit");
            marker("TNET: DONE");
            return 0;
            }
        (void)parse_target(line);             /* invalid line keeps the default */
        }

    format_target(line, 0);
    marker(line);

    int base = exec2(QE_EXEC_NETBRIDGE_BASE, 0, 0);
    int size = exec2(QE_EXEC_NETBRIDGE_SIZE, 0, 0);
    if (base <= 0 || size < (int)sizeof(struct nb_shared))
        { marker("TNET: FAIL bridge discovery (execs 247/248)");
          marker("TNET: DONE"); return 1; }
    struct nb_shared* sh = (struct nb_shared*)base;
    if (sh->magic != NB_MAGIC || sh->version != NB_VERSION)
        { marker("TNET: FAIL bridge magic");
          marker("TNET: DONE"); return 1; }

    /* ---- TCP 3-way handshake ---- */
    unsigned isn = random32();
    unsigned seq, ack, their_isn = 0;

    tx_arp_request(sh);
    if (send_tcp(sh, TCP_SYN, isn, 0, 0, 0) != 0)
        { marker("TNET: FAIL TX slot busy on SYN"); sh->done = 1;
          marker("TNET: DONE"); return 1; }
    marker("TNET: SYN sent");

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
            if (flags & TCP_RST)
                { break; }                    /* refused -> fall through to timeout */
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
        marker("TNET: connect timeout");
        sh->done = 1;
        marker("TNET: DONE");
        return 1;
        }
    marker("TNET: SYN-ACK received");
    marker("TNET: connected");

    seq = isn + 1u;
    ack = their_isn + 1u;
    if (send_tcp(sh, TCP_ACK, seq, ack, 0, 0) != 0)
        { marker("TNET: FAIL TX slot busy on ACK"); sh->done = 1;
          marker("TNET: DONE"); return 1; }
    marker("TNET: ACK sent");

    /* ---- send the interactive greeting line on the established connection ---- */
    unsigned hello_len = (unsigned)(sizeof(kHello) - 1);
    if (send_tcp(sh, TCP_PSH | TCP_ACK, seq, ack,
                 (const unsigned char*)kHello, hello_len) != 0)
        { marker("TNET: FAIL TX slot busy on HELLO"); sh->done = 1;
          marker("TNET: DONE"); return 1; }
    seq += hello_len;
    k = append(line, 0, "TNET: sent ");
    k = dec(line, k, hello_len);
    k = append(line, k, " bytes"); line[k] = 0;
    marker(line);

    /* ---- poll RX for echoed / response data, ACK each segment ---- */
    g_body_len = 0;
    unsigned next_ack = their_isn + 1u;
    int ended = 0, quit = 0;
    for (int poll = 0; poll < DATA_POLLS && !ended && !quit; ++poll)
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
                send_tcp(sh, TCP_ACK, seq, a, 0, 0);
                next_ack = a;
                if (g_body_len + plen <= sizeof(g_body))
                    {
                    scopy(g_body + g_body_len, g_rx + 34u + doff, plen);
                    g_body_len += plen;
                    }
                }
            if (flags & TCP_FIN)
                {
                send_tcp(sh, TCP_ACK, seq, next_ack + 1u, 0, 0);
                ended = 1;
                break;
                }
            }
        /* allow an interactive 'q' to end the session early */
        if (poll > 75 && quit_key()) quit = 1;
        }

    if (g_body_len)
        {
        /* Report at most the first 96 bytes, sanitised to printable ASCII. */
        unsigned show = g_body_len;
        if (show > 96u) show = 96u;
        char text[96 + 1];
        unsigned o = 0, i;
        for (i = 0; i < show; ++i)
            {
            unsigned char c = g_body[i];
            text[o++] = (c >= 0x20 && c <= 0x7e) ? (char)c : '.';
            }
        text[o] = 0;
        k = append(line, 0, "TNET: recv ");
        k = dec(line, k, g_body_len);
        k = append(line, k, " bytes: ");
        k = append(line, k, text); line[k] = 0;
        marker(line);
        }
    else
        marker("TNET: no data (timeout)");

    sh->done = 1;                             /* stop the kernel `netbridge` poll */
    marker("TNET: DONE");
    return quit ? 0 : (g_body_len ? 0 : 2);
}
