/*
** luae32.c -- Lua 5.4.6 interpreter running as a genuine PE2 E32 executable.
**
** The standard Lua `lua.c` driver uses hosted stdio; this driver replaces it
** with the R1 console runtime (read_line / print_n, EUSER ordinal 915) while
** using the unmodified Lua compiler and VM.  Lines typed on the serial port
** are compiled and executed one at a time; a bare expression is retried with
** an implicit `return`, like the stock interactive interpreter.
*/
#include "runtime.h"

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "e32_shim.h"

/* Small embedded demo script (FAT-file I/O is stubbed in this freestanding
   build, so the demos travel inside the executable). */
static const char demo_script[] =
    "function fib(n) if n < 2 then return n end return fib(n-1) + fib(n-2) end\n"
    "print('LUA: fib(10)=' .. fib(10))\n"
    "local t = {10,20,30}\n"
    "print('LUA: table t[2]=' .. t[2])\n";

static void write_text(const char *s)
{
    unsigned n = 0;
    while (s[n]) ++n;
    e32_lua_write(s, n);
}

static void run_chunk(lua_State *L, const char *src, unsigned len,
                      const char *chunkname)
{
    int status = luaL_loadbuffer(L, src, len, chunkname);
    if (status == LUA_ERRSYNTAX && len < 512)
        {
        char expr[576];
        unsigned i;
        for (i = 0; i < 7; ++i) expr[i] = "return "[i];
        for (i = 0; i < len; ++i) expr[7 + i] = src[i];
        expr[7 + len] = 0;
        status = luaL_loadbuffer(L, expr, 7 + len, chunkname);
        }
    if (status != LUA_OK)
        {
        const char *msg = lua_tostring(L, -1);
        write_text("LUA: ");
        if (msg) write_text(msg); else write_text("error");
        e32_lua_write("\n", 1);
        lua_pop(L, 1);
        return;
        }
    status = lua_pcall(L, 0, LUA_MULTRET, 0);
    if (status != LUA_OK)
        {
        const char *msg = lua_tostring(L, -1);
        write_text("LUA: ");
        if (msg) write_text(msg); else write_text("runtime error");
        e32_lua_write("\n", 1);
        lua_pop(L, 1);
        return;
        }
    /* Echo expression results, as the stock Lua REPL does. */
    {
    int top = lua_gettop(L), i;
    for (i = 1; i <= top; ++i)
        {
        const char *s = luaL_tolstring(L, i, NULL);
        if (i > 1) e32_lua_write("\t", 1);
        if (s) write_text(s);
        lua_pop(L, 1);
        }
    if (top > 0) e32_lua_write("\n", 1);
    }
}

int app_main(void)
{
    char line[512];
    lua_State *L;

    print("LUA: LUA 5.4.6 READY");

    L = lua_newstate(e32_lua_alloc, NULL);
    if (!L)
        {
        print("LUA: ERROR no state");
        print("LUA: DONE");
        return 1;
        }
    luaL_openlibs(L);

    run_chunk(L, demo_script, (unsigned)(sizeof(demo_script) - 1), "=demo");
    e32_lua_flush();

    if (con_acquire() < 0)
        {
        print("LUA: ERROR console");
        print("LUA: DONE");
        lua_close(L);
        return 1;
        }

    for (;;)
        {
        int r;
        unsigned len;
        con_puts_raw("lua> ");
        r = read_line(line, sizeof(line) - 1, 60000);
        /* A freshly launched process can briefly race the shell's console
           lease handoff; retry short errors instead of exiting the REPL. */
        if (r < 0 && r != -33)
            {
            int tries;
            for (tries = 0; tries < 100 && r < 0 && r != -33; ++tries)
                {
                sleep_ms(20);
                r = read_line(line, sizeof(line) - 1, 60000);
                }
            }
        if (r < 0) break;
        len = (unsigned)r;
        while (len && (line[len - 1] == ' ' || line[len - 1] == '\r'))
            --len;
        if (len == 0) continue;
        line[len] = 0;
        if (len == 4 && (line[0] == 'q' || line[0] == 'Q') &&
            (line[1] == 'u' || line[1] == 'U') &&
            (line[2] == 'i' || line[2] == 'I') &&
            (line[3] == 't' || line[3] == 'T'))
            break;
        if (len == 4 && (line[0] == 'e' || line[0] == 'E') &&
            (line[1] == 'x' || line[1] == 'X') &&
            (line[2] == 'i' || line[2] == 'I') &&
            (line[3] == 't' || line[3] == 'T'))
            break;
        run_chunk(L, line, len, "=stdin");
        e32_lua_flush();
        }

    con_release();
    lua_close(L);
    print("LUA: DONE");
    return 0;
}
