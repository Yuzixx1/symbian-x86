#include "runtime.h"

/* WC <file> — count lines and words in a text file. */
static U16 command[128];
static char path[128], chunk[256];

int app_main(void)
{
    int length = command_line(command, 128);
    if (length <= 0) { print("WC: give a file"); return 1; }
    int first = command[0] == '"' ? 1 : 0;
    if (first && command[length - 1] == '"') --length;
    int n = 0;
    for (int i = first; i < length && n < 127; ++i)
        path[n++] = (char)command[i];
    path[n] = 0;
    int handle = file_open(path);
    if (handle < 0) { print("WC: open failed"); return 1; }
    unsigned lines = 0, words = 0, inWord = 0;
    int count;
    while ((count = file_read(handle, chunk, sizeof(chunk))) > 0)
        {
        for (int i = 0; i < count; ++i)
            {
            char c = chunk[i];
            if (c == '\n') ++lines;
            if (c == ' ' || c == '\n' || c == '\r' || c == '\t')
                inWord = 0;
            else if (!inWord) { inWord = 1; ++words; }
            }
        }
    file_close(handle);
    if (count < 0) { print("WC: read failed"); return 1; }
    char out[64];
    int p = 0;
    const char* pre = "WC: ";
    while (*pre) out[p++] = *pre++;
    char digits[16];
    int k = 0;
    if (!lines) digits[k++] = '0';
    while (lines) { digits[k++] = (char)('0' + lines % 10); lines /= 10; }
    while (k) out[p++] = digits[--k];
    out[p++] = ' '; out[p++] = 'l'; out[p++] = ' '; out[p++] = ' ';
    k = 0;
    if (!words) digits[k++] = '0';
    while (words) { digits[k++] = (char)('0' + words % 10); words /= 10; }
    while (k) out[p++] = digits[--k];
    out[p++] = ' '; out[p++] = 'w'; out[p] = 0;
    print(out);
    return 0;
}
