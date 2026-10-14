#include "ipc.h"

extern int exec2(unsigned, unsigned, unsigned);
extern int exec4(unsigned, unsigned, unsigned, unsigned, unsigned);
extern void ipc_wait_any(void);
struct Name { unsigned type_length; const char* pointer; };
static struct Name descriptor(const char* text)
{
    unsigned n = 0;
    while (text[n]) ++n;
    struct Name result = {0x10000000u | n, text};
    return result;
}
int ipc_wait(volatile int* status)
{
    /* Genuine WaitForRequest preserves surplus signals; stealing another
       request's wakeup loses it. Poll first so an already-completed status
       consumes no signal; leftover signals only cause harmless spurious
       wakeups on the next wait. */
    while (*status == (int)0x80000001u) { ipc_wait_any(); }
    return *status;
}
int ipc_server_create(const char* name)
{
    struct Name n = descriptor(name);
    return exec2(126, (unsigned)&n, 0);
}
int ipc_receive(int server, struct IpcMessage* message)
{
    volatile int status;
    ipc_receive_async(server, message, &status);
    return ipc_wait(&status);
}
void ipc_receive_async(int server, struct IpcMessage* message, volatile int* status)
{
    *status = (int)0x80000001u;
    exec4(34, server, (unsigned)status, (unsigned)message, 0);
}
void ipc_receive_cancel(int server) { exec2(35, server, 0); }
int ipc_length(struct IpcMessage* message, int slot)
{
    return exec2(167, message->handle, slot);
}
static int copy(struct IpcMessage* message, int slot, const void* data,
                int length, int offset, unsigned flags)
{
    struct { const void* pointer; int length; unsigned flags; }
        info = {data, length, flags}; /* SIpcCopyInfo, u32std.h */
    return exec4(169, message->handle, slot, (unsigned)&info, offset);
}
int ipc_read8(struct IpcMessage* message, int slot, void* data, int capacity, int offset)
{
    return copy(message, slot, data, capacity, offset, 0);
}
int ipc_write8(struct IpcMessage* message, int slot, const void* data, int length, int offset)
{
    return copy(message, slot, data, length, offset, 0x10000000u);
}
int ipc_read16(struct IpcMessage* message, int slot, void* data, int capacity, int offset)
{
    return copy(message, slot, data, capacity, offset, 0x80000000u);
}
int ipc_write16(struct IpcMessage* message, int slot, const void* data, int length, int offset)
{
    return copy(message, slot, data, length, offset, 0x90000000u);
}
void ipc_complete(struct IpcMessage* message, int result)
{
    exec2(65, message->handle, result);
    message->handle = 0;
}
void ipc_accept(struct IpcMessage* message, void* cookie)
{
    exec2(36, message->handle, (unsigned)cookie);
}
int ipc_call(int session, int function, int argument)
{
    struct IpcArgs args = {{argument, 0, 0, 0}, 0};
    return ipc_call_args(session, function, &args);
}
int ipc_call_args(int session, int function, const struct IpcArgs* args)
{
    volatile int status = (int)0x80000001u;
    int r = exec4(77, session, function, (unsigned)args, (unsigned)&status);
    if (r) return r;
    ipc_wait(&status);
    return status;
}
int ipc_connect(const char* name)
{
    struct Name n = descriptor(name);
    int handle = exec4(128, (unsigned)&n, 1, 0, 0);
    if (handle < 0) return handle;
    int r = ipc_call(handle, -1, 1); /* TVersion(1,0,0), EConnect */
    if (r) { ipc_close(handle); return r; }
    return handle;
}
void ipc_close(int handle) { exec2(106, handle, 0); }
