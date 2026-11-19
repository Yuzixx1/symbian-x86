#include "runtime.h"

/* P-06 probe: WAITFOR.E32 <pid> [timeout_ms]
 * Posts a genuine ProcessLogon executive for <pid> and blocks on the thread
 * request semaphore until that process dies or the timeout expires. */

static U16 command[128];

static int next_uint(int* pos, int length, unsigned* value)
{
    int p = *pos;
    while (p < length && (command[p] == ' ' || command[p] == '\t')) ++p;
    if (p >= length || command[p] < '0' || command[p] > '9') return -1;
    unsigned n = 0;
    while (p < length && command[p] >= '0' && command[p] <= '9')
        {
        n = n * 10u + (unsigned)(command[p] - '0');
        ++p;
        }
    while (p < length && (command[p] == ' ' || command[p] == '\t')) ++p;
    *pos = p;
    *value = n;
    return 0;
}

static unsigned put_int(char* buffer, unsigned at, int value)
{
    char digits[12];
    unsigned n = 0;
    unsigned v = value < 0 ? (unsigned)(-value) : (unsigned)value;
    if (value < 0) buffer[at++] = '-';
    do { digits[n++] = (char)('0' + (v % 10)); v /= 10; } while (v);
    while (n) buffer[at++] = digits[--n];
    return at;
}

int app_main(void)
{
    int length = command_line(command, 128);
    int pos = 0;
    unsigned pid = 0, timeout = 3000;
    if (length < 0 || next_uint(&pos, length, &pid) < 0)
        {
        print("WAITFOR: usage: WAITFOR.E32 <pid> [timeout_ms]");
        return 1;
        }
    if (pos < length && next_uint(&pos, length, &timeout) < 0)
        {
        print("WAITFOR: bad timeout");
        return 1;
        }
    if (pos < length)
        {
        print("WAITFOR: extra arguments");
        return 1;
        }
    int reason = QE_KREQUEST_PENDING, type = 0;
    int r = process_wait_exit(pid, &reason, &type, (int)timeout);
    char line[128];
    unsigned n = 0;
    const char* tag = "WAITFOR: result pid=";
    while (*tag) line[n++] = *tag++;
    n = put_int(line, n, (int)pid);
    line[n++] = ' ';
    line[n++] = 'r';
    line[n++] = '=';
    n = put_int(line, n, r);
    line[n++] = ' ';
    line[n++] = 't';
    line[n++] = '=';
    n = put_int(line, n, type);
    line[n++] = 0;
    print_n(line, n - 1);
    return r == QE_KERR_TIMED_OUT ? 2 : 0;
}
