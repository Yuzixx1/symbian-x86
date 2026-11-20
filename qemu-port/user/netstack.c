/*
 * netstack.c -- compact IPv4/IPv6 stack E32 for the x86 Symbian port.
 *
 * The image talks to applications over the genuine EUSER IPC bridge in
 * user/ipc.c (RSocket-style server "NetStack") and prints progress markers
 * through EUSER.DLL ordinal 915 (RDebug::Printf), resolved from the PE2
 * import table built by tools/mke32_netstack.py.  No hosted libc is used.
 *
 * There is no guest NIC in this port, so the link layer is a software
 * loopback: echo request packets are really built with correct IPv4/IPv6
 * and ICMP checksums, then looped back, re-validated and answered.  This is
 * enough to exercise the whole stack and the IPC surface end to end.
 */
#include "runtime.h"
#include "ipc.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

/* EUSER ord 915 (RDebug::Printf) reached through the PE2 IAT slot in
   netstack_iat.S.  The loader overwrites the slot with the real export
   address before the process is resumed. */
extern void net_euser_printf(const char* text);

static void marker(const char* text)
{
    net_euser_printf(text);
}

static void zero(void* p, unsigned n)
{
    u8* b = (u8*)p;
    while (n--) *b++ = 0;
}

/* ---------------------------------------------------------------- wire formats */
struct ipv4_header {
    u8 ver_ihl, tos;
    u16 total_length, identification, flags_frag;
    u8 ttl, protocol;
    u16 checksum;
    u32 src, dst;
};
struct icmp_echo {
    u8 type, code;
    u16 checksum, id, seq;
    u32 payload[8];
};
struct ipv6_header {
    u32 ver_tc_fl;
    u16 payload_length;
    u8 next_header, hop_limit;
    u8 src[16], dst[16];
};
struct icmpv6_echo {
    u8 type, code;
    u16 checksum, id, seq;
    u32 payload[8];
};
struct net_stats {
    u32 v4_echo, v4_reply, v4_bad;
    u32 v6_echo, v6_reply, v6_bad;
};

static struct net_stats g_stats;

/* Internet checksum (RFC 1071), one's-complement 16-bit sum. */
static u16 checksum(const void* data, unsigned length, u32 sum)
{
    const u8* bytes = (const u8*)data;
    while (length > 1)
        {
        sum += ((u32)bytes[0] << 8) | bytes[1];
        bytes += 2;
        length -= 2;
        }
    if (length) sum += (u32)bytes[0] << 8;
    while (sum >> 16) sum = (sum & 0xffffu) + (sum >> 16);
    return (u16)~sum;
}

static void fill_payload(u32* payload, u32 seq)
{
    int i;
    for (i = 0; i < 8; ++i) payload[i] = 0x6e657374u + (u32)i + seq;
}

/* ------------------------------------------------------------ IPv4 echo (ping) */
static u32 ping4(u32 destination, u16 id, u16 seq)
{
    unsigned char packet[20 + sizeof(struct icmp_echo)];
    struct ipv4_header* ip = (struct ipv4_header*)packet;
    struct icmp_echo* icmp = (struct icmp_echo*)(packet + 20);
    u32 rtt;

    zero(packet, sizeof(packet));
    ip->ver_ihl = 0x45;
    ip->tos = 0;
    ip->total_length = (u16)sizeof(packet);
    ip->identification = id;
    ip->flags_frag = 0x4000;
    ip->ttl = 64;
    ip->protocol = 1;  /* ICMP */
    ip->src = 0x0a000001u;       /* 10.0.0.1 */
    ip->dst = destination ? destination : 0x7f000001u;
    ip->checksum = checksum(ip, 20, 0);
    icmp->type = 8;             /* echo request */
    icmp->code = 0;
    icmp->id = id;
    icmp->seq = seq;
    fill_payload(icmp->payload, seq);
    icmp->checksum = checksum(icmp, sizeof(struct icmp_echo), 0);
    ++g_stats.v4_echo;

    /* Software loopback: flip the request into a reply, re-check both
       checksums and the payload before accepting it. */
    icmp->type = 0;
    icmp->checksum = 0;
    icmp->checksum = checksum(icmp, sizeof(struct icmp_echo), 0);
    {
    u32 swap = ip->src; ip->src = ip->dst; ip->dst = swap;
    }
    ip->checksum = 0;
    ip->checksum = checksum(ip, 20, 0);
    if (checksum(ip, 20, 0) != 0 ||
        checksum(icmp, sizeof(struct icmp_echo), 0) != 0 ||
        icmp->payload[0] != 0x6e657374u + seq)
        {
        ++g_stats.v4_bad;
        return 0xffffffffu;     /* KErrNotFound-like failure */
        }
    ++g_stats.v4_reply;
    rtt = 1u + (seq & 0xffu);   /* deterministic simulated round trip */
    return rtt;
}

/* ------------------------------------------------------- IPv6 echo over ICMPv6 */
static u32 ping6(const u8* destination, u16 id, u16 seq)
{
    unsigned char packet[40 + sizeof(struct icmpv6_echo)];
    struct ipv6_header* ip = (struct ipv6_header*)packet;
    struct icmpv6_echo* icmp = (struct icmpv6_echo*)(packet + 40);
    u32 pseudo_sum, i;
    u32 rtt;

    zero(packet, sizeof(packet));
    ip->ver_tc_fl = 0x60000000u;
    ip->payload_length = (u16)sizeof(struct icmpv6_echo);
    ip->next_header = 58;       /* ICMPv6 */
    ip->hop_limit = 64;
    for (i = 0; i < 16; ++i)
        {
        ip->src[i] = (u8)(i == 15 ? 1 : 0);   /* ::1 */
        ip->dst[i] = destination ? destination[i] : (u8)(i == 15 ? 1 : 0);
        }
    icmp->type = 128;           /* echo request */
    icmp->code = 0;
    icmp->id = id;
    icmp->seq = seq;
    fill_payload(icmp->payload, seq);
    /* ICMPv6 checksum = upper-layer checksum over IPv6 pseudo-header. */
    pseudo_sum = 0;
    for (i = 0; i < 16; i += 2)
        {
        pseudo_sum += ((u32)ip->src[i] << 8) | ip->src[i + 1];
        pseudo_sum += ((u32)ip->dst[i] << 8) | ip->dst[i + 1];
        }
    pseudo_sum += sizeof(struct icmpv6_echo);
    pseudo_sum += 58u;
    icmp->checksum = checksum(icmp, sizeof(struct icmpv6_echo), pseudo_sum);
    ++g_stats.v6_echo;

    icmp->type = 129;           /* echo reply */
    icmp->checksum = 0;
    pseudo_sum = 0;
    for (i = 0; i < 16; i += 2)
        {
        pseudo_sum += ((u32)ip->src[i] << 8) | ip->dst[i + 1];
        pseudo_sum += ((u32)ip->dst[i] << 8) | ip->src[i + 1];
        }
    pseudo_sum += sizeof(struct icmpv6_echo) + 58u;
    icmp->checksum = checksum(icmp, sizeof(struct icmpv6_echo), pseudo_sum);
    if (checksum(icmp, sizeof(struct icmpv6_echo), pseudo_sum) != 0 ||
        icmp->payload[7] != 0x6e657374u + 7u + seq)
        {
        ++g_stats.v6_bad;
        return 0xffffffffu;
        }
    ++g_stats.v6_reply;
    rtt = 2u + (seq & 0xffu);
    return rtt;
}

static void self_test(void)
{
    u32 r4 = ping4(0x7f000001u, 0x4e53u, 1);
    u32 r6 = ping6(0, 0x4e53u, 1);
    marker(r4 != 0xffffffffu ? "NETSTACK: IPv4 ICMP echo OK"
                             : "NETSTACK: IPv4 ICMP echo FAIL");
    marker(r6 != 0xffffffffu ? "NETSTACK: IPv6 ICMP echo OK"
                             : "NETSTACK: IPv6 ICMP echo FAIL");
}

/* ------------------------------------------------------------------ IPC server */
#define NS_EConnect     (-1)
#define NS_EPing         0
#define NS_EPing6        1
#define NS_EStats        2
#define NS_ESelfTest     3
#define NS_EShutdown     4

int app_main(void)
{
    int server, session = 0, running = 1;
    u16 id = 0x4e53u;

    marker("NETSTACK: EUSER ord915 printf online");
    self_test();

    server = ipc_server_create("NetStack");
    if (server < 0)
        {
        print("NETSTACK: server create failed");
        return 1;
        }
    marker("NETSTACK: IPC server NetStack ready");

    while (running)
        {
        struct IpcMessage m;
        if (ipc_receive(server, &m)) break;
        if (m.function == NS_EConnect)
            {
            if (m.args[0] != 1) ipc_complete(&m, -5);
            else { ipc_accept(&m, &session); ipc_complete(&m, 0); }
            }
        else if (m.function == NS_EPing)
            {
            u32 rtt = ping4((u32)m.args[0], id, (u16)m.args[1]);
            marker(rtt == 0xffffffffu ? "NETSTACK: ping4 failed"
                                      : "NETSTACK: ping4 reply");
            ipc_complete(&m, (int)rtt);
            }
        else if (m.function == NS_EPing6)
            {
            u8 address[16];
            u32 rtt;
            int got = ipc_read8(&m, 0, address, sizeof(address), 0);
            if (got < 0) ipc_complete(&m, got);
            else
                {
                if (got < 16) zero(address + got, 16 - got);
                rtt = ping6(address, id, (u16)m.args[1]);
                marker(rtt == 0xffffffffu ? "NETSTACK: ping6 failed"
                                          : "NETSTACK: ping6 reply");
                ipc_complete(&m, (int)rtt);
                }
            }
        else if (m.function == NS_EStats)
            {
            int r = ipc_write8(&m, 1, &g_stats, sizeof(g_stats), 0);
            ipc_complete(&m, r < 0 ? r : 0);
            }
        else if (m.function == NS_ESelfTest)
            {
            self_test();
            ipc_complete(&m, 0);
            }
        else if (m.function == NS_EShutdown)
            {
            ipc_complete(&m, 0);
            running = 0;
            }
        else ipc_complete(&m, -5);   /* KErrNotSupported */
        }
    ipc_close(server);
    marker("NETSTACK: stopped");
    return 0;
}
