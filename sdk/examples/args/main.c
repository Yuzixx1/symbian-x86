#include "runtime.h"

/* Echo the command line: `exec ARGS.E32 "two words"  three` round-trips
   the shell tail verbatim (quotes and spacing preserved). */
static U16 arguments[128];
static char output[128];

int app_main(void)
{
    int length = command_line(arguments, 128);
    if (length < 0)
        {
        print("ARGS: command line too long");
        return 1;
        }
    for (int i = 0; i < length; ++i)
        output[i] = arguments[i] < 128 ? (char)arguments[i] : '?';
    print("ARGS: ");
    print_n(output, length);
    return 0;
}
