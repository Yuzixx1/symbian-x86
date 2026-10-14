#include "runtime.h"

/* Also exercise initialized pointers and BSS through the ELF->E32 packer. */
static const char* banner = "ECHO: ";
static U16 arguments[128];
static char output[128];

int app_main(void)
{
    int length = command_line(arguments, 128);
    if (length < 0)
        {
        print("ECHO: command line too long");
        return 1;
        }
    /* One trace record makes output deterministic despite shell scheduling. */
    char line[144];
    unsigned n = 0;
    while (banner[n]) { line[n] = banner[n]; ++n; }
    for (int i = 0; i < length; ++i)
        {
        output[i] = arguments[i] < 128 ? (char)arguments[i] : '?';
        line[n++] = output[i];
        }
    print_n(line, n);
    return 0;
}
