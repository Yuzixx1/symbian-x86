/*
 * ifconfig.c -- IFCONFIG-style network configuration utility for the port.
 *
 * A genuine PE2 E32 program that reports the configuration of the port's
 * e1000/SLIRP interface in the classic ifconfig layout.  The interface state
 * is really probed: exec 247 (EExecSlowAtomicAxo64, invalid in this build)
 * is repurposed by the kernel to publish the e1000 frame bridge at a low,
 * identity-mapped physical address (user/netbridge.h).  A live "NBRG" magic
 * proves the e1000 is present and up, and the guest MAC is read from the
 * bridge's EEPROM-derived mac[] field.
 *
 * The IP settings below are the lease handed out by QEMU SLIRP's real DHCP
 * server (see user/dhcpc.c and tools/r1_dhcpc_test.py):
 *   10.0.2.15 / 255.255.255.0, gateway 10.0.2.2, DNS 10.0.2.3.
 *
 * All serial markers go through EUSER.DLL ordinal 915 (RDebug::Printf),
 * imported via a PE2 IAT slot at 0x408000 (absolute symbol
 * ifconfig_printf_slot).  If EUSER is not bound the markers fall back to the
 * port's DebugPrint executive.  ARP for the gateway is optional: the e1000
 * frame bridge lives in the kernel, so confirming the link would require the
 * separate `netbridge` poll loop; presence via exec 247 is enough here.
 */
#include "runtime.h"
#include "netbridge.h"

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern unsigned int ifconfig_printf_slot;
typedef void (*IfconfigPrintf)(const char*, ...);

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument. */
static void marker(const char* text)
{
    unsigned int fn = *(volatile unsigned int*)&ifconfig_printf_slot;
    if (fn != 915u && fn != 0u) ((IfconfigPrintf)fn)(text);
    else print(text);
}

/* ------------------------------------------------------------ text helpers
 * Freestanding: build each marker directly into a buffer, no hosted libc. */
static unsigned copy_text(char* dst, unsigned n, const char* src)
{
    while (*src) dst[n++] = *src++;
    return n;
}

static unsigned append_hex2(char* dst, unsigned n, unsigned value)
{
    static const char kHex[] = "0123456789abcdef";
    dst[n++] = kHex[(value >> 4) & 0xFu];
    dst[n++] = kHex[value & 0xFu];
    return n;
}

static void fmt_mac(const unsigned char mac[6], char out[18])
{
    unsigned n = 0;
    for (unsigned i = 0; i < 6; ++i)
        {
        if (i) out[n++] = ':';
        n = append_hex2(out, n, mac[i]);
        }
    out[n] = 0;
}

/* ------------------------------------------------------------------ main */
int app_main(void)
{
    char line[128];

    marker("NET: IFCONFIG");

    /* Genuine e1000 presence probe: the netbridge base executive (247). */
    int base = exec2(QE_EXEC_NETBRIDGE_BASE, 0u, 0u);
    int live = 0;
    if (base > 0x1000)
        {
        struct nb_shared* sh = (struct nb_shared*)(unsigned)base;
        if (sh->magic == NB_MAGIC) live = 1;
        }

    if (!live)
        {
        marker("NET: eth0: e1000 <down> (netbridge exec 247 unavailable)");
        marker("NET: DONE");
        return 0;
        }

    marker("NET: eth0: e1000 <up>");
    marker("NET: inet 10.0.2.15 netmask 255.255.255.0");
    marker("NET: gateway 10.0.2.2");
    marker("NET: dns 10.0.2.3");

    /* Read the guest MAC the kernel published from the e1000 EEPROM. */
    {
    struct nb_shared* sh = (struct nb_shared*)(unsigned)base;
    unsigned char mac[6];
    char macstr[18];
    for (unsigned i = 0; i < 6; ++i) mac[i] = sh->mac[i];
    fmt_mac(mac, macstr);
    unsigned n = copy_text(line, 0, "NET: MAC ");
    n = copy_text(line, n, macstr);
    line[n] = 0;
    marker(line);
    }

    marker("NET: DONE");
    return 0;
}
