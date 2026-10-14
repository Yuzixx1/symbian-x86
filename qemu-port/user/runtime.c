#include "runtime.h"

/* Genuine TPtr8 and TPtrC8 binary layouts (u32std.h: EPtr=2, EPtrC=1). */
struct Ptr8 { unsigned type_length, maximum; void* pointer; };
struct PtrC8 { unsigned type_length; const void* pointer; };
extern int exec0(unsigned number);
extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern int exec4(unsigned, unsigned, unsigned, unsigned, unsigned);
extern void ipc_wait_any(void);
void file_close(int handle);
void sleep_ms(int milliseconds);
void print_n(const char* text, unsigned length);

void sleep_us(int microseconds)
{
    /* Fresh RTimer per call: the thread-embedded iTimer behind User::After
       does not complete twice in this port (first After OK, second hangs),
       while single-use request objects — like sessions — repeat reliably.
       Single semaphore wait: with no other outstanding requests on this
       thread, the next signal is this timer (same constraint as ipc_call). */
    if (microseconds < 0) microseconds = 0;
    if (microseconds > 60000000) microseconds = 60000000;
    volatile int status = (int)0x80000001u;
    int timer = exec0(133);
    if (timer < 0) return;
    int r = exec4(53, (unsigned)timer, (unsigned)&status, (unsigned)microseconds, 0);
    if (r == 0) ipc_wait_any();
    exec2(106, (unsigned)timer, 0);
}

void sleep_ms(int milliseconds)
{
    if (milliseconds < 0) milliseconds = 0;
    /* Guard the 32-bit multiply: cap at ~35 minutes. */
    if (milliseconds > 2000000) milliseconds = 2000000;
    sleep_us(milliseconds * 1000);
}

void thread_yield(void) { sleep_us(0); }

int time_now(void)
{
    /* EExecTimeNow: TTime microseconds since 0AD + UTC offset. Anchor:
       2000-01-01 = day 730485 (proleptic Gregorian, verified against the
       Unix 1970 anchor 719528 + 10957). Needs libgcc __divdi3 (user.mk). */
    long long microseconds = 0;
    int offset = 0;
    int r = exec2(68, (unsigned)&microseconds, (unsigned)&offset);
    if (r) return r;
    long long seconds = microseconds / 1000000LL - 63113904000LL;
    return (int)seconds;
}

unsigned random32(void) { return (unsigned)exec0(11); }

/* Transitional console input over the PortCon logical device. Contract:
   acquire returns KErrNone once; the shell stops consuming input while the
   lease is held and reclaims it if the owner dies. Always release before
   exit; read_line echoes, handles backspace, ends at \n (CR accepted). */
static int con_channel(void)
{
    struct PtrC8 name = {0x10000007, "PortCon"};
    struct { unsigned version; int unit; void* physical; void* info; }
        create = {1, -1, 0, 0};
    return exec4(132, (unsigned)&name, (unsigned)&create, 1, 0);
}

int con_acquire(void)
{
    int handle = con_channel();
    if (handle < 0) return handle;
    int result = exec4(10, handle, 1, 0, 0);
    file_close(handle);
    return result;
}

void con_release(void)
{
    int handle = con_channel();
    if (handle < 0) return;
    exec4(10, handle, 2, 0, 0);
    file_close(handle);
}

int con_tryread(void)
{
    int handle = con_channel();
    if (handle < 0) return handle;
    int result = exec4(10, handle, 0, 0, 0);
    file_close(handle);
    return result;
}

int read_line(char* buffer, unsigned capacity, int timeout_ms)
{
    unsigned n = 0;
    int elapsed = 0;
    if (!capacity) return -6;
    for (;;)
        {
        int c = con_tryread();
        if (c >= 0)
            {
            if (c == '\r') c = '\n';
            if (c == '\n') { print_n("\n", 1); break; }
            if (c == '\b' || c == 127)
                {
                if (n) { --n; print_n("\b \b", 3); }
                continue;
                }
            if (c < 32 || c > 126) continue;
            if (n + 1 < capacity)
                {
                char ch = (char)c;
                buffer[n++] = ch;
                print_n(&ch, 1);
                }
            continue;
            }
        if (c != -18) return c;  /* real error, not empty */
        if (timeout_ms >= 0 && elapsed >= timeout_ms) return -33; /* KErrTimedOut */
        sleep_ms(20);
        elapsed += 20;
        }
    buffer[n] = 0;
    return (int)n;
}

static int port_channel(void)
{
    struct PtrC8 name = {0x10000008, "PortFile"};
    /* TVersion is two bytes plus a 16-bit build number. */
    struct { unsigned version; int unit; void* physical; void* info; }
        create = {1, -1, 0, 0};
    return exec4(132, (unsigned)&name, (unsigned)&create, 1, 0);
}

int file_open(const char* path)
{
    int handle = port_channel();
    if (handle < 0) return handle;
    unsigned length = 0;
    while (path[length]) ++length;
    struct PtrC8 filename = {0x10000000 | length, path};
    int result = exec4(10, handle, 0, (unsigned)&filename, 0);
    if (result < 0) { file_close(handle); return result; }
    return handle;
}

int file_read(int handle, void* buffer, unsigned capacity)
{
    if (capacity > 256) return -6; /* KErrArgument: fail fast, no silent short read */
    struct Ptr8 descriptor = {0x20000000, capacity, buffer};
    return exec4(10, handle, 1, (unsigned)&descriptor, capacity);
}

void file_close(int handle) { exec2(106, handle, 0); }

int file_create_write(const char* path)
{
    int handle = port_channel();
    if (handle < 0) return handle;
    unsigned path_length = 0;
    while (path[path_length]) ++path_length;
    struct PtrC8 filename = {0x10000000 | path_length, path};
    int result = exec4(10, handle, 2, (unsigned)&filename, 0);
    if (result < 0) { file_close(handle); return result; }
    return handle;
}

int file_append(int handle, const void* data, unsigned length)
{
    const char* bytes = data;
    int total = 0;
    while (length)
        {
        unsigned count = length > 256 ? 256 : length;
        struct PtrC8 chunk = {0x10000000 | count, bytes};
        int result = exec4(10, handle, 3, (unsigned)&chunk, 0);
        if (result < 0) return result;
        bytes += count;
        length -= count;
        total += result;
        }
    return total;
}

int file_commit(int handle)
{
    return exec4(10, handle, 4, 0, 0);
}

int change_dir(const char* path)
{
    int handle = port_channel();
    if (handle < 0) return handle;
    unsigned length = 0;
    while (path[length]) ++length;
    struct PtrC8 dirname = {0x10000000 | length, path};
    int result = exec4(10, handle, 5, (unsigned)&dirname, 0);
    file_close(handle);
    return result;
}

int get_cwd(char* buffer, unsigned capacity)
{
    int handle = port_channel();
    if (handle < 0) return handle;
    struct Ptr8 descriptor = {0x20000000, capacity, buffer};
    int result = exec4(10, handle, 6, (unsigned)&descriptor, capacity);
    file_close(handle);
    return result;
}

int file_save(const char* path, const void* data, unsigned length)
{
    int handle = file_create_write(path);
    if (handle < 0) return handle;
    int result = file_append(handle, data, length);
    if (result >= 0) result = file_commit(handle);
    file_close(handle);
    return result;
}

int command_line(U16* buffer, unsigned capacity)
{
    int length = exec2(82, 0xffff8000u, 0);
    if (length < 0) return length;
    if ((unsigned)length >= capacity)
        return -9; /* KErrOverflow; leave room for a terminating zero. */
    struct Ptr8 descriptor = {0x20000000, (capacity - 1) * 2, buffer};
    int r = exec2(23, 0xffff8000u, (unsigned)&descriptor);
    if (r < 0) return r;
    length = (descriptor.type_length & 0x0fffffff) / 2;
    buffer[length] = 0;
    return length;
}

void print_n(const char* text, unsigned length)
{
    /* Genuine DebugPrint executive: early console transport, 256 bytes
       per call. A proper console server will replace this debug channel. */
    while (length)
        {
        unsigned count = length > 256 ? 256 : length;
        struct PtrC8 descriptor = {0x10000000 | count, text};
        exec2(86, (unsigned)&descriptor, 0);
        text += count;
        length -= count;
        }
}

void print(const char* text)
{
    unsigned length = 0;
    while (text[length]) ++length;
    print_n(text, length);
}
