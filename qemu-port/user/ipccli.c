#include "runtime.h"
#include "ipc.h"

static int descriptors(int session)
{
    unsigned char input[257], output[258];
    for (unsigned i = 0; i < sizeof(input); ++i) input[i] = (unsigned char)i;
    struct IpcConst8 source = {0x10000101u, input};
    struct IpcBuffer8 target = {0x20000000u, 257, output};
    struct IpcArgs args = {{(unsigned)&source, (unsigned)&target, 0, 0}, 6 | (4 << 3)};
    output[257] = 0xa5;
    if (ipc_call_args(session, 3, &args) || target.type_length != 0x20000101u)
        return 1;
    for (unsigned i = 0; i < sizeof(input); ++i)
        if (input[i] != output[i]) return 2;
    if (output[257] != 0xa5) return 3;
    args.args[2] = 63;
    target.type_length = 0x20000000u;
    if (ipc_call_args(session, 3, &args) || target.type_length != 0x200000c2u)
        return 4;
    for (unsigned i = 0; i < 194; ++i)
        if (output[i] != input[i + 63]) return 5;
    args.args[2] = 0;
    target.maximum = 8;
    target.type_length = 0x20000000u;
    if (ipc_call_args(session, 3, &args) != -9) return 6;
    /* A valid descriptor address advertised as an integer must not be
       accepted as a descriptor. No invalid memory pointer is needed. */
    args.flags = 4 << 3;
    if (ipc_call_args(session, 3, &args) != -38) return 7;
    args.flags = 6 | (4 << 3);
    args.args[2] = (unsigned)-1;
    if (ipc_call_args(session, 3, &args) != -6) return 9;
    args.args[2] = 0;
    target.maximum = 257;
    args.flags = 6 | (6 << 3); /* const output must refuse writes */
    if (ipc_call_args(session, 3, &args) != -38) return 10;
    args.flags = 6 | (4 << 3);
    source.type_length = 0x10000000u;
    if (ipc_call_args(session, 3, &args)) return 8;
    return 0;
}

static int descriptors16(int session)
{
    static unsigned short input[129], output[130];
    for (unsigned i = 0; i < 129; ++i) input[i] = (unsigned short)(0x4100 + i);
    input[64] = 0; /* embedded zero must survive as data */
    struct IpcConst16 source = {0x10000081u, input};
    struct IpcBuffer16 target = {0x20000000u, 129, output};
    /* EDesC16=7, EDes16=5 */
    struct IpcArgs args = {{(unsigned)&source, (unsigned)&target, 0, 0}, 7 | (5 << 3)};
    output[129] = 0x5a5a;
    if (ipc_call_args(session, 4, &args) || target.type_length != 0x20000081u)
        return 1;
    for (unsigned i = 0; i < 129; ++i)
        if (input[i] != output[i]) return 2;
    if (output[129] != 0x5a5a) return 3;
    args.args[2] = 17;
    target.type_length = 0x20000000u;
    if (ipc_call_args(session, 4, &args) || target.type_length != 0x20000070u)
        return 4;
    for (unsigned i = 0; i < 112; ++i)
        if (output[i] != input[i + 17]) return 5;
    /* 8-bit server copy on 16-bit descriptors must be rejected. */
    struct IpcArgs wrong = {{(unsigned)&source, (unsigned)&target, 0, 0}, 7 | (5 << 3)};
    if (ipc_call_args(session, 3, &wrong) != -38) return 6;
    return 0;
}

int app_main(void)
{
    U16 command[32];
    int length = command_line(command, 32);
    int session = ipc_connect("PortIpcCheck");
    if (session < 0) { print("IPCCLI: connect failed"); return 1; }
    if (length == 3 && command[0] == 'd' && command[1] == 'i' && command[2] == 'e')
        {
        int r = ipc_call(session, 2, 0);
        ipc_close(session);
        if (r != -15) { print("IPCCLI: termination mismatch"); return 5; }
        print("IPCCLI: pending request received KErrServerTerminated");
        return 0;
        }
    if (length == 4 && command[0] == 's' && command[1] == 't' &&
        command[2] == 'o' && command[3] == 'p')
        {
        int r = ipc_call(session, 1, 0);
        ipc_close(session);
        if (r) { print("IPCCLI: stop failed"); return 2; }
        print("IPCCLI: stop acknowledged");
        return 0;
        }
    for (int i = 0; i < 32; ++i)
        if (ipc_call(session, 0, 100 + i) != 100 + i)
            { ipc_close(session); print("IPCCLI: reply mismatch"); return 3; }
    int check = descriptors(session);
    if (check)
        {
        ipc_close(session);
        if (check < 10)
            {
            char stage[] = "IPCCLI: descriptor check failed at 0";
            stage[sizeof(stage) - 2] += check;
            print(stage);
            }
        else print("IPCCLI: descriptor check failed at 10");
        return 6;
        }
    print("IPCCLI: binary descriptors, offsets, overflow and type checks verified");
    check = descriptors16(session);
    if (check)
        {
        ipc_close(session);
        if (check < 10)
            {
            char stage[] = "IPCCLI: descriptor16 check failed at 0";
            stage[sizeof(stage) - 2] += check;
            print(stage);
            }
        else print("IPCCLI: descriptor16 check failed at 10");
        return 7;
        }
    print("IPCCLI: 16-bit descriptors verified");
    if (ipc_call(session, 99, 0) != -5)
        { ipc_close(session); print("IPCCLI: unsupported mismatch"); return 4; }
    ipc_close(session);
    print("IPCCLI: 32 replies and unsupported request verified");
    return 0;
}
