/*
 * netbridge_test.c -- PE2 E32 proof that a real ARP reply crosses the
 * kernel<->userland frame bridge.
 *
 * Discover the bridge via slow execs 247 (base) / 248 (size), build a raw
 * Ethernet/ARP "who-has 10.0.2.2" request into the TX ring, let the kernel's
 * `netbridge` poll command hand it to the e1000, then read the ARP reply out
 * of the RX ring.  Serial markers use genuine EUSER.DLL ordinal 915
 * (RDebug::Printf), bound by E32Run from the PE2 IAT at 0x408000.
 */
#include "runtime.h"
#include "netbridge.h"

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern unsigned int nb_printf_slot;
typedef void (*NbPrintf)(const char*, ...);

static void marker(const char* text)
{
    unsigned int fn = *(volatile unsigned int*)&nb_printf_slot;
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

int app_main(void)
{
    char line[160];
    unsigned k;

    int base = exec2(QE_EXEC_NETBRIDGE_BASE, 0, 0);
    int size = exec2(QE_EXEC_NETBRIDGE_SIZE, 0, 0);
    if (base <= 0 || size < (int)sizeof(struct nb_shared))
        {
        marker("NB: FAIL bridge discovery (execs 247/248)");
        return 1;
        }
    struct nb_shared* sh = (struct nb_shared*)base;
    if (sh->magic != NB_MAGIC || sh->version != NB_VERSION)
        {
        marker("NB: FAIL bridge magic");
        return 1;
        }
    k = append(line, 0, "NB: bridge base=");
    /* base is a low physical (<0x80000000) address: print as decimal+hex-ish */
    k = dec(line, k, (unsigned)base);
    k = append(line, k, " size=");
    k = dec(line, k, (unsigned)size);
    line[k] = 0;
    marker(line);

    /* ---- build the 42-byte ARP request into a free TX slot ---- */
    struct nb_ring* tx = &sh->tx;
    const unsigned ti = tx->head;
    struct nb_slot* ts = &tx->slot[ti];
    if (ts->len)
        {
        marker("NB: FAIL TX ring full");
        return 1;
        }
    unsigned char* f = (unsigned char*)ts->data;
    unsigned i;
    for (i = 0; i < 6; ++i) { f[i] = 0xff; f[6 + i] = sh->mac[i]; }
    f[12] = 0x08; f[13] = 0x06;              /* EtherType ARP */
    f[14] = 0x00; f[15] = 0x01;              /* hardware type Ethernet */
    f[16] = 0x08; f[17] = 0x00;              /* protocol type IPv4 */
    f[18] = 6;    f[19] = 4;                 /* address lengths */
    f[20] = 0x00; f[21] = 0x01;              /* ARP operation: request */
    for (i = 0; i < 6; ++i) f[22 + i] = sh->mac[i];   /* sender hardware */
    f[28] = 10; f[29] = 0; f[30] = 2; f[31] = 15;    /* sender IP 10.0.2.15 */
    /* target hardware (32..37) stays zero */
    f[38] = 10; f[39] = 0; f[40] = 2; f[41] = 2;     /* target IP 10.0.2.2 */
    ts->len = 42;
    __asm__ __volatile__("" : : : "memory");
    tx->head = (ti + 1u) % NB_RING_SLOTS;
    marker("NB: ARP request posted to TX ring");

    /* ---- wait for the ARP reply in the RX ring ---- */
    int found = 0;
    for (int poll = 0; poll < 500; ++poll)
        {
        sleep_ms(20);
        struct nb_ring* rx = &sh->rx;
        const unsigned ri = rx->tail;
        struct nb_slot* rs = &rx->slot[ri];
        const unsigned len = rs->len;
        if (len >= 42 && rs->data[12] == 0x08 && rs->data[13] == 0x06 &&
            rs->data[20] == 0x00 && rs->data[21] == 0x02)
            {
            const unsigned char* r = (const unsigned char*)rs->data;
            k = append(line, 0, "NB: ARP reply from ");
            k = dec(line, k, r[28]); k = append(line, k, ".");
            k = dec(line, k, r[29]); k = append(line, k, ".");
            k = dec(line, k, r[30]); k = append(line, k, ".");
            k = dec(line, k, r[31]);
            k = append(line, k, " sha=");
            for (i = 0; i < 6; ++i)
                {
                if (i) k = append(line, k, ":");
                k = hex2(line, k, r[22 + i]);
                }
            k = append(line, k, " len=");
            k = dec(line, k, len);
            line[k] = 0;
            marker(line);
            rs->len = 0;                      /* release back to kernel */
            __asm__ __volatile__("" : : : "memory");
            rx->tail = (ri + 1u) % NB_RING_SLOTS;
            found = 1;
            break;
            }
        }

    sh->done = 1;                             /* stop the kernel poll loop */
    if (!found)
        {
        marker("NB: FAIL no ARP reply received");
        return 1;
        }
    marker("NB: RESULT PASS - real ARP reply crossed kernel->user boundary");
    return 0;
}
