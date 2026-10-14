#include "runtime.h"

static U16 command[128];
static char source[128], destination[128];

static int path_argument(int* position, int length, char* output)
{
    int p = *position;
    while (p < length && (command[p] == ' ' || command[p] == '\t')) ++p;
    int quoted = p < length && command[p] == '"';
    if (quoted) ++p;
    int n = 0;
    while (p < length)
        {
        U16 c = command[p];
        if (quoted ? c == '"' : c == ' ' || c == '\t') break;
        if (c == '"' || c > 127 || n == 127) return -1;
        output[n++] = (char)c;
        ++p;
        }
    if (quoted)
        {
        if (p == length) return -1;
        ++p;
        if (p < length && command[p] != ' ' && command[p] != '\t') return -1;
        }
    output[n] = 0;
    *position = p;
    return n ? 0 : -1;
}

static int split_paths(int length)
{
    int p = 0;
    if (path_argument(&p, length, source) ||
        path_argument(&p, length, destination)) return -1;
    while (p < length && (command[p] == ' ' || command[p] == '\t')) ++p;
    return p == length ? 0 : -1;
}

int app_main(void)
{
    int length = command_line(command, 128);
    if (length <= 0 || split_paths(length) < 0)
        {
        print("CP: use two paths; quote paths containing spaces");
        return 1;
        }
    int reader = file_open(source);
    if (reader < 0) { print("CP: open source failed"); return 1; }
    int writer = file_create_write(destination);
    if (writer < 0) { file_close(reader); print("CP: open destination failed"); return 1; }
    /* Streaming copy in 256-byte pieces: no staging cap besides the
       64 KiB PortFile/FAT snapshot limit. Abort without commit so an
       oversized source leaves an existing destination unchanged. */
    static char chunk[256];
    unsigned total = 0;
    int count;
    while ((count = file_read(reader, chunk, sizeof(chunk))) > 0)
        {
        if (total + (unsigned)count > 65536)
            {
            file_close(reader);
            file_close(writer);
            print("CP: source exceeds 65536 bytes; destination unchanged");
            return 1;
            }
        int written = file_append(writer, chunk, (unsigned)count);
        if (written < 0)
            {
            file_close(reader);
            file_close(writer);
            print("CP: write failed; destination unchanged");
            return 1;
            }
        total += (unsigned)count;
        }
    file_close(reader);
    if (count < 0)
        {
        file_close(writer);
        print("CP: read failed");
        return 1;
        }
    int result = file_commit(writer);
    file_close(writer);
    if (result < 0) { print("CP: commit failed"); return 1; }
    print("CP: copied");
    return 0;
}
