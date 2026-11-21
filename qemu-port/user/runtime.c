#include "runtime.h"

/* Genuine TPtr8 and TPtrC8 binary layouts (u32std.h: EPtr=2, EPtrC=1). */
struct Ptr8 { unsigned type_length, maximum; void* pointer; };
struct PtrC8 { unsigned type_length; const void* pointer; };
extern int exec0(unsigned number);
extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern int exec4(unsigned, unsigned, unsigned, unsigned, unsigned);
extern void ipc_wait_any(void);

static int timer_wait(volatile int* status)
{
    /* Self-contained twin of ipc.c's ipc_wait_for: genuine WaitForRequest
       accounting keeps bare SDK apps (which link only runtime.c + malloc.c)
       from needing ipc.c. */
    int extra = -1;
    do { ++extra; ipc_wait_any(); } while (*status == (int)0x80000001u);
    if (extra) exec2(58, (unsigned)extra, 0);
    return *status;
}
void file_close(int handle);
void sleep_ms(int milliseconds);
void print_n(const char* text, unsigned length);

void sleep_us(int microseconds)
{
    /* Fresh RTimer per call: the thread-embedded iTimer behind User::After
       does not complete twice in this port (first After OK, second hangs),
       while single-use request objects — like sessions — repeat reliably.
       Genuine TimerAfter has no TInt return (execs.txt declares it void), so
       EAX on return is scratch and must NOT gate the wait: gating on r==0 made
       every sleep return instantly (DIAG probes showed r=0x1021DA8). Wait
       unconditionally, reusing the genuine WaitForRequest accounting in
       ipc_wait_for so a surplus signal on this thread is consumed and
       reposted rather than swallowed. */
    if (microseconds < 0) microseconds = 0;
    if (microseconds > 60000000) microseconds = 60000000;
    volatile int status = (int)0x80000001u;
    int timer = exec0(133);
    if (timer < 0) return;
    exec4(53, (unsigned)timer, (unsigned)&status, (unsigned)microseconds, 0);
    timer_wait(&status);
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

void thread_park(void) { ipc_wait_any(); }

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

/* ------------------------------------------------------------------ wait/exit
 * Genuine logon executives. The kernel stores our TRequestStatus pointer in a
 * TLogon and completes it (with the target's exit reason) from TLogon::CompleteAll
 * when the last thread of the process dies, or immediately with iExitReason if
 * the process is already exiting when the logon is posted. */
int process_open_by_id(unsigned aId)
{
    return exec2(QE_EXEC_PROCESS_OPEN_BY_ID, aId, QE_OWNER_PROCESS);
}

void process_logon(int aHandle, volatile int* aStatus)
{
    /* SLOW_EXEC3: ECX=handle, EDX=status, EBX=rendezvous=0.
       exec4 supplies exactly those four registers (ESI unused). */
    exec4(QE_EXEC_PROCESS_LOGON, (unsigned)aHandle, (unsigned)aStatus, 0, 0);
}

int process_logon_cancel(int aHandle, volatile int* aStatus)
{
    return exec4(QE_EXEC_PROCESS_LOGON_CANCEL, (unsigned)aHandle,
                 (unsigned)aStatus, 0, 0);
}

void thread_logon(int aHandle, volatile int* aStatus)
{
    exec4(QE_EXEC_THREAD_LOGON, (unsigned)aHandle, (unsigned)aStatus, 0, 0);
}

int thread_logon_cancel(int aHandle, volatile int* aStatus)
{
    return exec4(QE_EXEC_THREAD_LOGON_CANCEL, (unsigned)aHandle,
                 (unsigned)aStatus, 0, 0);
}

int process_wait_exit(unsigned aPid, int* aExitReason, int* aExitType,
                      int aTimeoutMs)
{
    volatile int status = QE_KREQUEST_PENDING;
    int elapsed = 0;
    int handle = process_open_by_id(aPid);
    if (handle < 0)
        return handle;
    process_logon(handle, &status);
    for (;;)
        {
        if (status != QE_KREQUEST_PENDING)
            {
            /* The completion woke our request semaphore; consume exactly that
               signal (sleep_ms reposts any wakes that belonged to its timer). */
            ipc_wait_any();
            break;
            }
        if (aTimeoutMs >= 0 && elapsed >= aTimeoutMs)
            {
            process_logon_cancel(handle, &status);
            if (status != QE_KREQUEST_PENDING)
                ipc_wait_any();   /* consume KErrCancel signal */
            exec2(QE_EXEC_HANDLE_CLOSE, (unsigned)handle, 0);
            return QE_KERR_TIMED_OUT;
            }
        int step = 20;
        if (aTimeoutMs >= 0 && elapsed + step > aTimeoutMs)
            step = aTimeoutMs - elapsed;
        if (step <= 0) step = 1;
        sleep_ms(step);
        elapsed += step;
        }
    if (aExitType)
        *aExitType = exec2(QE_EXEC_PROCESS_EXIT_TYPE, (unsigned)handle, 0);
    if (aExitReason)
        *aExitReason = (int)status;
    exec2(QE_EXEC_HANDLE_CLOSE, (unsigned)handle, 0);
    return (int)status;
}

/* Genuine TChunkCreate (u32std.h): att, force, bottom, top, max, clear. */
struct ChunkCreate { unsigned att, force_fixed; int initial_bottom, initial_top, max_size; unsigned char clear_byte; };
#define CHUNK_GLOBAL 0x10u   /* TChunkCreate::EGlobal */

int chunk_create(const char* name, unsigned initial_size, unsigned max_size)
{
    struct ChunkCreate info;
    info.att = name ? CHUNK_GLOBAL : 0;
    info.force_fixed = 0;
    info.initial_bottom = 0;
    info.initial_top = (int)initial_size;
    info.max_size = (int)max_size;
    info.clear_byte = 0x03;  /* genuine default clear byte */
    if (name)
        {
        unsigned n = 0;
        while (name[n]) ++n;
        struct PtrC8 nd = {0x10000000u | n, name};
        return exec4(107, 0, (unsigned)&nd, (unsigned)&info, 0);
        }
    return exec4(107, 0, 0, (unsigned)&info, 0);
}

int chunk_open(const char* name)
{
    unsigned n = 0;
    while (name[n]) ++n;
    struct PtrC8 nd = {0x10000000u | n, name};
    return exec4(109, 2, (unsigned)&nd, 0, 0);  /* EChunk = 2 */
}

void* chunk_base(int handle) { return (void*)exec2(1, (unsigned)handle, 0); }
int chunk_size(int handle) { return exec2(2, (unsigned)handle, 0); }
int chunk_adjust(int handle, unsigned new_size)
{
    return exec4(108, (unsigned)handle, 0, (unsigned)new_size, 0);
}

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

/* Raw echo via PortCon exec 3. Genuine DebugPrint (exec 86) terminates every
   call with \r\n, so print_n() cannot echo one character at a time without
   stacking input vertically. The lease owner's raw path emits exactly the
   character supplied, with no line ending; the caller adds a single '\n'
   only when Enter is read. */
static void con_echo_char(int c)
{
    int handle = con_channel();
    if (handle < 0) return;
    exec4(10, handle, 3, (unsigned)c, 0);
    file_close(handle);
}

void con_puts_raw(const char* text)
{
    while (*text) con_echo_char((unsigned char)*text++);
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
            if (c == '\n') { con_echo_char('\n'); break; }
            if (c == '\b' || c == 127)
                {
                if (n) { --n; con_echo_char('\b'); con_echo_char(' '); con_echo_char('\b'); }
                continue;
                }
            if (c < 32 || c > 126) continue;
            if (n + 1 < capacity)
                {
                char ch = (char)c;
                buffer[n++] = ch;
                con_echo_char(c);
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

/* Large-file streaming channel (PortBigFile). It supersedes PortFile for
   reads: same fn 0 open / fn 4 sequential read wire shape, but no 64 KiB or
   256-byte whole-file snapshot ceiling. Write/cwd functions still use
   port_channel() above and are untouched. */
static int bigfile_channel(void)
{
    struct PtrC8 name = {0x1000000B, "PortBigFile"};
    struct { unsigned version; int unit; void* physical; void* info; }
        create = {1, -1, 0, 0};
    return exec4(132, (unsigned)&name, (unsigned)&create, 1, 0);
}

int file_open(const char* path)
{
    int handle = bigfile_channel();
    if (handle < 0) return handle;
    unsigned length = 0;
    while (path[length]) ++length;
    struct PtrC8 filename = {0x10000000 | length, path};
    int result = exec4(10, handle, 0, (unsigned)&filename, 0);
    if (result < 0) { file_close(handle); return result; }
    return handle;
}

/* Sequential read from the handle's current position.  Capacity is still
   capped at 256 bytes to preserve the original contract exactly. */
int file_read(int handle, void* buffer, unsigned capacity)
{
    if (capacity > 256) return -6; /* KErrArgument: fail fast, no silent short read */
    struct { void* buffer; unsigned length; } seq = {buffer, capacity};
    return exec4(10, handle, 4, (unsigned)&seq, 0);
}

/* ---- new large-file streaming APIs (append-only) ------------------------ */
long file_size(int handle)
{
    return (long)exec4(10, handle, 1, 0, 0);
}

int file_seek(int handle, unsigned offset)
{
    return exec4(10, handle, 3, offset, 0);
}

int file_read_at(int handle, unsigned offset, void* buf, unsigned len)
{
    struct { void* buffer; unsigned offset; unsigned length; }
        io = {buf, offset, len};
    return exec4(10, handle, 2, (unsigned)&io, 0);
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
