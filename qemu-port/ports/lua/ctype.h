/* Minimal freestanding <ctype.h> (ASCII only) for the Lua 5.4 E32 port. */
#ifndef QPORT_LUA_CTYPE_H
#define QPORT_LUA_CTYPE_H

static __inline__ int isalpha(int c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}
static __inline__ int isdigit(int c) { return (c >= '0' && c <= '9'); }
static __inline__ int isalnum(int c) { return isalpha(c) || isdigit(c); }
static __inline__ int isspace(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
static __inline__ int islower(int c) { return (c >= 'a' && c <= 'z'); }
static __inline__ int isupper(int c) { return (c >= 'A' && c <= 'Z'); }
static __inline__ int isxdigit(int c) {
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
static __inline__ int iscntrl(int c) { return (c >= 0 && c < 32) || c == 127; }
static __inline__ int isgraph(int c) { return c > 32 && c < 127; }
static __inline__ int isprint(int c) { return c >= 32 && c < 127; }
static __inline__ int ispunct(int c) { return isgraph(c) && !isalnum(c); }
static __inline__ int tolower(int c) { return isupper(c) ? (c | 32) : c; }
static __inline__ int toupper(int c) { return islower(c) ? (c & ~32) : c; }

#endif /* QPORT_LUA_CTYPE_H */
