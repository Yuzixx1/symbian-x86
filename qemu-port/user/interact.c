#include "runtime.h"

/* Interactive fixture: console lease, line input, sleep, time, random.
   Proves a program can own the keyboard, converse, and hand input back.
   NOTE: DebugPrint ends every call with a newline, so each asserted line
   is assembled first and emitted with ONE print_n call. */
static char name[64];

struct Line { char buffer[128]; unsigned length; };
static void putc_(struct Line* line, char c)
{
    if (line->length + 1 < sizeof(line->buffer))
        line->buffer[line->length++] = c;
}
static void puts_(struct Line* line, const char* text)
{
    while (*text) putc_(line, *text++);
}
static void putu_(struct Line* line, unsigned v)
{
    char digits[11];
    int n = 0;
    if (!v) digits[n++] = '0';
    else while (v) { digits[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) putc_(line, digits[--n]);
}
static void put2_(struct Line* line, unsigned v)
{
    putc_(line, (char)('0' + (v / 10) % 10));
    putc_(line, (char)('0' + v % 10));
}
static void emit(struct Line* line)
{
    putc_(line, '\n');
    print_n(line->buffer, line->length);
    line->length = 0;
}

static void put_time(struct Line* line, int seconds)
{
    static const unsigned char mdays[12] =
        {31,28,31,30,31,30,31,31,30,31,30,31};
    int days = seconds / 86400;
    int rem = seconds % 86400;
    int year = 2000;
    for (;;)
        {
        int leap = (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
        int ydays = leap ? 366 : 365;
        if (days < ydays) break;
        days -= ydays;
        ++year;
        }
    unsigned mon = 1;
    for (;;)
        {
        int leap = (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
        unsigned md = mdays[mon - 1] + ((mon == 2 && leap) ? 1 : 0);
        if (days < (int)md) break;
        days -= md;
        ++mon;
        }
    putu_(line, (unsigned)year);
    putc_(line, '-');
    put2_(line, mon);
    putc_(line, '-');
    put2_(line, (unsigned)(days + 1));
    putc_(line, ' ');
    put2_(line, (unsigned)(rem / 3600));
    putc_(line, ':');
    put2_(line, (unsigned)((rem % 3600) / 60));
    putc_(line, ':');
    put2_(line, (unsigned)(rem % 60));
}

int app_main(void)
{
    static struct Line line;
    int r = con_acquire();
    if (r < 0) { print("INTERACT: no lease"); return 1; }
    int now = time_now();
    if (now < 0) { print("INTERACT: no clock"); con_release(); return 1; }
    puts_(&line, "INTERACT: ready ");
    put_time(&line, now);
    puts_(&line, " random=");
    putu_(&line, random32() % 1000);
    emit(&line);
    print("Name? ");
    r = read_line(name, sizeof(name), 60000);
    if (r < 0) { print("INTERACT: input timed out"); con_release(); return 1; }
    sleep_ms(100);
    puts_(&line, "Hello ");
    puts_(&line, name);
    puts_(&line, "!");
    emit(&line);
    print("INTERACT: done");
    con_release();
    return 0;
}
