#include "runtime.h"
#include "ipc.h"

/* Service integration fixture: exercise genuine DServer/DSession/RMessageK.
   Deliberately not named after a system server whose protocol it lacks. */
int app_main(void)
{
    int server = ipc_server_create("PortIpcCheck");
    if (server < 0) { print("IPCSRV: create failed"); return 1; }
    /* Session cookie: the kernel stores it opaquely (never dereferenced),
       but use a real object address rather than a magic integer so any
       future diagnostic dereference stays in our data. */
    static int session_anchor;
    /* Before announcing readiness no client can queue a request in this
       fixture. Cancel a pending receive, then reuse the server below. */
    struct IpcMessage pending;
    volatile int status;
    ipc_receive_async(server, &pending, &status);
    ipc_receive_cancel(server);
    if (ipc_wait(&status) != -3)
        { ipc_close(server); print("IPCSRV: cancel failed"); return 2; }
    print("IPCSRV: receive cancellation verified");
    print("IPCSRV: ready");
    for (;;)
        {
        struct IpcMessage m;
        if (ipc_receive(server, &m)) break;
        if (m.function == -1)
            {
            if (m.args[0] != 1) ipc_complete(&m, -5);
            else { ipc_accept(&m, &session_anchor); ipc_complete(&m, 0); }
            }
        else if (m.function == -2) ipc_complete(&m, 0);
        else if (m.function == 0) ipc_complete(&m, m.args[0]);
        else if (m.function == 1) { ipc_complete(&m, 0); break; }
        else if (m.function == 2) break; /* test pending-client teardown */
        else if (m.function == 3)
            {
            unsigned char buffer[64];
            int length = ipc_length(&m, 0);
            int result = length < 0 ? length : 0;
            int offset = m.args[2];
            if (offset < 0) result = -6;
            while (result >= 0 && offset < length)
                {
                int count = ipc_read8(&m, 0, buffer, sizeof(buffer), offset);
                if (count <= 0) { result = count < 0 ? count : -6; break; }
                result = ipc_write8(&m, 1, buffer, count, offset - m.args[2]);
                offset += count;
                }
            ipc_complete(&m, result);
            }
        else if (m.function == 4)
            {
            unsigned short buffer[32];
            int length = ipc_length(&m, 0);
            int result = length < 0 ? length : 0;
            int offset = m.args[2];
            if (offset < 0) result = -6;
            while (result >= 0 && offset < length)
                {
                int count = ipc_read16(&m, 0, buffer, 32, offset);
                if (count <= 0) { result = count < 0 ? count : -6; break; }
                result = ipc_write16(&m, 1, buffer, count, offset - m.args[2]);
                offset += count;
                }
            ipc_complete(&m, result);
            }
        else ipc_complete(&m, -5); /* KErrNotSupported */
        }
    ipc_close(server);
    print("IPCSRV: stopped");
    return 0;
}
