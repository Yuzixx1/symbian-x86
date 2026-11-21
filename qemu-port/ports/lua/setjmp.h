/* Freestanding i486 setjmp/longjmp.  Lua only needs the "throw back to the
 * nearest protected call" behaviour; we save the callee-saved registers plus
 * ESP and the resume address.  No libc, no unwind tables.
 */
#ifndef QPORT_LUA_SETJMP_H
#define QPORT_LUA_SETJMP_H

typedef struct {
    unsigned int ebx, esi, edi, ebp, esp, eip;
} jmp_buf[1];

#define setjmp(env)   e32_setjmp(env)
#define longjmp(env,v) e32_longjmp((env), (v))

static __inline__ int e32_setjmp(jmp_buf env)
{
    int r;
    __asm__ __volatile__(
        "movl %%ebx, 0(%[e])\n\t"
        "movl %%esi, 4(%[e])\n\t"
        "movl %%edi, 8(%[e])\n\t"
        "movl %%ebp, 12(%[e])\n\t"
        "movl %%esp, 16(%[e])\n\t"
        "leal 1f, %%eax\n\t"
        "movl %%eax, 20(%[e])\n\t"
        "movl $0, %%eax\n"
        "1:\n\t"
        "movl %%eax, %[r]"
        : [r] "=m"(r)
        : [e] "r"(env)
        : "eax", "memory");
    return r;
}

static __inline__ void __attribute__((noreturn))
e32_longjmp(jmp_buf env, int val)
{
    __asm__ __volatile__(
        "movl %[v], %%eax\n\t"
        "testl %%eax, %%eax\n\t"
        "jnz 1f\n\t"
        "movl $1, %%eax\n"
        "1:\n\t"
        "movl 0(%[e]), %%ebx\n\t"
        "movl 4(%[e]), %%esi\n\t"
        "movl 8(%[e]), %%edi\n\t"
        "movl 12(%[e]), %%ebp\n\t"
        "movl 16(%[e]), %%esp\n\t"
        "jmp *20(%[e])"
        :
        : [e] "r"(env), [v] "r"(val)
        : "eax", "memory");
    for (;;) { } /* unreachable; keeps noreturn satisfied */
}

#endif /* QPORT_LUA_SETJMP_H */
