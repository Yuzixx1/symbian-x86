/* Minimal freestanding <locale.h>: Lua's decimal point is hard-wired to '.'.
 * luaconf.h in this port defines lua_getlocaledecpoint() as '.', so no locale
 * object is ever needed; this header only satisfies source-level includes.
 */
#ifndef QPORT_LUA_LOCALE_H
#define QPORT_LUA_LOCALE_H
struct lconv { char *decimal_point; };
static __inline__ struct lconv *localeconv(void) {
    static struct lconv lc = { "." };
    return &lc;
}
#endif
