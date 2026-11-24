
#include <stddef.h>
void* memcpy(void* d, const void* s, size_t n) { unsigned char* a=d; const unsigned char* b=s; while(n--) *a++=*b++; return d; }
void* memmove(void* d, const void* s, size_t n) { unsigned char* a=d; const unsigned char* b=s; if(a<b){while(n--)*a++=*b++;} else {a+=n;b+=n;while(n--)*--a=*--b;} return d; }
void* memset(void* d, int c, size_t n) { unsigned char* a=d; while(n--) *a++=(unsigned char)c; return d; }
int memcmp(const void* a, const void* b, size_t n) { const unsigned char* x=a; const unsigned char* y=b; while(n--){ if(*x!=*y) return *x-*y; ++x; ++y; } return 0; }
size_t strlen(const char* s) { size_t n=0; while(s[n]) ++n; return n; }
