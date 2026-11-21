/*
 * bcinterp.c -- freestanding interpreter for a faithful bc-compatible subset.
 *
 * This is an original implementation of the documented bc language subset
 * (not GNU bc source).  It uses the base-10 bignum engine in bignum.c and a
 * bump arena, so it needs no libc and fits comfortably in the 64 KiB heap.
 *
 * Grammar handled here:
 *   program   := statement (separator statement)*
 *   statement := define | if | while | for | return | print | break
 *              | continue | block | expression
 *   expression:= assignment (lowest) .. relational .. plus/minus .. mul/div/mod .. power
 *              .. unary .. primary
 */
#include "bignum.h"
#include "bcinterp.h"

/* ------------------------------------------------------------------ limits */
#define NDIG        96      /* max significant decimal digits per variable   */
#define NVAR        27      /* a..z plus scale (index 26)                     */
#define VAR_SCALE   26
#define ARENA_SIZE  32768   /* 32 KiB expression arena (< 64 KiB cap)         */
#define MAXFUN      12
#define MAXPARAM    4
#define FUNBODY     256
#define MAXFRAME    12
#define MAXITER     200000
#define LINEBUF     1024

/* ------------------------------------------------------------- string utils */
static int slen(const char* s)
{
    int n = 0;
    while (s[n]) ++n;
    return n;
}

static int seq(const char* a, const char* b)
{
    int i = 0;
    while (a[i] && b[i] && a[i] == b[i]) ++i;
    return a[i] == b[i];
}

/* Keyword match with right-hand identifier boundary; advances *p on match. */
static int kw(const char** p, const char* word)
{
    const char* q = *p;
    int i = 0;
    while (word[i] && q[i] == word[i]) ++i;
    if (!word[i] && !(q[i] >= 'a' && q[i] <= 'z')
                 && !(q[i] >= '0' && q[i] <= '9') && q[i] != '_')
        { *p = q + i; return 1; }
    return 0;
}

static void copy_str(char* dst, const char* src, int n)
{
    int i;
    for (i = 0; i < n; ++i) dst[i] = src[i];
    dst[n] = 0;
}

/* ------------------------------------------------------------------ globals */
typedef struct { unsigned char d[NDIG]; int len, scale, neg; } VSlot;

static BcArena   g_arena;
static unsigned char g_arena_mem[ARENA_SIZE];
static BcEmit    g_emit;
static VSlot     g_var[NVAR];
static BcNum     g_vnum[NVAR];      /* BcNum views onto g_var digit arrays   */
static int       g_err;
static int       g_ret, g_brk, g_cont;
static BcNum*    g_retval;
static char      g_line[LINEBUF];

typedef struct {
    char name[16];
    int  used;
    int  np;
    char pname[MAXPARAM][16];
    int  pvar[MAXPARAM];
    char body[FUNBODY];
} Func;
static Func g_fun[MAXFUN];

typedef struct {
    int   np;
    int   pv[MAXPARAM];
    VSlot saved[MAXPARAM];
} Frame;
static Frame g_frame[MAXFRAME];
static int g_depth;

/* --------------------------------------------------------------- variables */
static void var_sync(int i)
{
    g_vnum[i].d = g_var[i].d;
    g_vnum[i].len = g_var[i].len;
    g_vnum[i].scale = g_var[i].scale;
    g_vnum[i].neg = g_var[i].neg;
}

static BcNum* varref(int i) { return &g_vnum[i]; }

static void var_assign(int i, const BcNum* v)
{
    int n = v->len;
    int k;
    if (n > NDIG) n = NDIG;
    for (k = 0; k < n; ++k) g_var[i].d[k] = v->d[k];
    g_var[i].len = n;
    g_var[i].scale = v->scale;
    g_var[i].neg = v->neg;
    var_sync(i);
}

static void var_save(int i, VSlot* s)
{
    int k;
    s->len = g_var[i].len;
    s->scale = g_var[i].scale;
    s->neg = g_var[i].neg;
    for (k = 0; k < s->len && k < NDIG; ++k) s->d[k] = g_var[i].d[k];
}

static void var_restore(int i, const VSlot* s)
{
    int k;
    g_var[i].len = s->len;
    g_var[i].scale = s->scale;
    g_var[i].neg = s->neg;
    for (k = 0; k < s->len && k < NDIG; ++k) g_var[i].d[k] = s->d[k];
    var_sync(i);
}

static int scale_value(void)
{
    int s = bc_num_to_int(varref(VAR_SCALE));
    if (s < 0) s = 0;
    if (s > 200) s = 200;
    return s;
}

/* ------------------------------------------------------------- whitespace
 * ws() skips spaces, tabs, CR and comments.  A bare newline is a statement
 * separator and is handled by skip_seps(), not here. */
static void ws(const char** p)
{
    for (;;)
        {
        const char* q = *p;
        while (*q == ' ' || *q == '\t' || *q == '\r') ++q;
        if (q[0] == '#' || (q[0] == '/' && q[1] == '/'))
            {
            while (*q && *q != '\n') ++q;
            if (*q == '\n') ++q;
            *p = q;
            continue;
            }
        if (q[0] == '/' && q[1] == '*')
            {
            q += 2;
            while (*q && !(q[0] == '*' && q[1] == '/')) ++q;
            if (*q) q += 2;
            *p = q;
            continue;
            }
        *p = q;
        return;
        }
}

static void skip_seps(const char** p)
{
    for (;;)
        {
        ws(p);
        if (**p == ';' || **p == '\n') { ++*p; continue; }
        return;
        }
}

/* ----------------------------------------------------------- identifier var */
static int parse_ident(const char** p, int* vi)
{
    const char* q = *p;
    if (q[0] == 's' && q[1] == 'c' && q[2] == 'a' && q[3] == 'l' && q[4] == 'e'
        && !(q[5] >= 'a' && q[5] <= 'z'))
        {
        *vi = VAR_SCALE;
        *p = q + 5;
        return 1;
        }
    if (q[0] >= 'a' && q[0] <= 'z')
        {
        *vi = q[0] - 'a';
        *p = q + 1;
        return 1;
        }
    return 0;
}

/* ---------------------------------------------------------------- forward */
static BcNum* eval_expr(const char** p);
static void   exec_statement(const char** p);
static void   exec_statements(const char** p, char stop);

/* ----------------------------------------------------------- find stmt end
 * Returns the first character after one statement, without executing it. */
static const char* find_stmt_end(const char* p)
{
    int par = 0, br = 0;
    ws(&p);
    if (*p == '{')
        {
        int depth = 0;
        while (*p)
            {
            if (*p == '{') ++depth;
            else if (*p == '}') { --depth; if (depth == 0) return p + 1; }
            ++p;
            }
        return p;
        }
    while (*p)
        {
        char c = *p;
        if (c == '(') ++par;
        else if (c == ')') { if (par > 0) --par; }
        else if (c == '{') ++br;
        else if (c == '}') { if (br == 0) return p; --br; }
        else if ((c == ';' || c == '\n') && par == 0 && br == 0) return p + 1;
        ++p;
        }
    return p;
}

/* -------------------------------------------------------------- emit marker */
static void emit_value(const char* src, int srclen, const BcNum* v)
{
    char val[LINEBUF];
    int n = 0, i;
    g_line[n++] = 'B'; g_line[n++] = 'C'; g_line[n++] = ':'; g_line[n++] = ' ';
    for (i = 0; i < srclen && n < LINEBUF - 1; ++i)
        g_line[n++] = src[i];
    g_line[n++] = ' '; g_line[n++] = '='; g_line[n++] = ' ';
    bc_num_write(v, val, sizeof(val));
    for (i = 0; val[i] && n < LINEBUF - 1; ++i) g_line[n++] = val[i];
    g_line[n] = 0;
    if (g_emit) g_emit(g_line);
}

static void emit_text(const char* prefix, const char* text)
{
    int n = 0, i;
    for (i = 0; prefix[i] && n < LINEBUF - 1; ++i) g_line[n++] = prefix[i];
    for (i = 0; text[i] && n < LINEBUF - 1; ++i) g_line[n++] = text[i];
    g_line[n] = 0;
    if (g_emit) g_emit(g_line);
}

/* ------------------------------------------------------------ function call */
/* compare a NUL-terminated stored name with a [begin,end) source slice */
static int name_eq(const char* stored, const char* b, int n)
{
    int i;
    if ((int)slen(stored) != n) return 0;
    for (i = 0; i < n; ++i) if (stored[i] != b[i]) return 0;
    return 1;
}

static int find_func_slice(const char* b, int n)
{
    int i;
    for (i = 0; i < MAXFUN; ++i)
        if (g_fun[i].used && name_eq(g_fun[i].name, b, n)) return i;
    return -1;
}

static BcNum* call_function(const char** p, const char* nb, int nn)
{
    BcNum* args[MAXPARAM];
    int nargs = 0;
    int fi = find_func_slice(nb, nn);
    Func* f;
    Frame* fr;
    const char* bp;
    BcNum* result;
    int i;
    int saved_ret = g_ret, saved_brk = g_brk, saved_cont = g_cont;
    BcNum* saved_rv = g_retval;
    ws(p);
    if (**p != '(') { g_err = 1; return bc_num_from_int(&g_arena, 0); }
    ++*p;
    ws(p);
    if (**p != ')')
        {
        for (;;)
            {
            if (nargs < MAXPARAM) args[nargs] = eval_expr(p);
            ++nargs;
            ws(p);
            if (**p == ',') { ++*p; ws(p); continue; }
            break;
            }
        }
    ws(p);
    if (**p == ')') ++*p; else g_err = 1;
    if (fi < 0) { g_err = 1; return bc_num_from_int(&g_arena, 0); }
    f = &g_fun[fi];
    if (g_depth >= MAXFRAME) { g_err = 1; return bc_num_from_int(&g_arena, 0); }
    fr = &g_frame[g_depth++];
    fr->np = f->np;
    for (i = 0; i < f->np; ++i)
        {
        fr->pv[i] = f->pvar[i];
        var_save(fr->pv[i], &fr->saved[i]);
        if (i < nargs && args[i]) var_assign(fr->pv[i], args[i]);
        }
    g_ret = 0; g_brk = 0; g_cont = 0; g_retval = 0;
    bp = f->body;
    exec_statements(&bp, 0);
    result = g_retval ? bc_num_copy(&g_arena, g_retval) : bc_num_from_int(&g_arena, 0);
    for (i = 0; i < fr->np; ++i) var_restore(fr->pv[i], &fr->saved[i]);
    --g_depth;
    g_ret = saved_ret; g_brk = saved_brk; g_cont = saved_cont; g_retval = saved_rv;
    if (!result) result = bc_num_from_int(&g_arena, 0);
    return result;
}

/* ---------------------------------------------------------------- primaries */
static BcNum* eval_primary(const char** p)
{
    ws(p);
    if (**p == '(')
        {
        BcNum* v;
        ++*p;
        v = eval_expr(p);
        ws(p);
        if (**p == ')') ++*p; else g_err = 1;
        return v;
        }
    if ((**p >= '0' && **p <= '9')
        || (**p == '.' && (*p)[1] >= '0' && (*p)[1] <= '9'))
        {
        const char* b = *p;
        while ((**p >= '0' && **p <= '9') || **p == '.') ++*p;
        return bc_num_from_string(&g_arena, b, 0, (int)(*p - b));
        }
    {
    const char* nb = *p;
    const char* q = *p;
    int nn;
    while (*q >= 'a' && *q <= 'z') ++q;
    nn = (int)(q - nb);
    if (nn > 0)
        {
        *p = q;
        ws(p);
        if (**p == '(') return call_function(p, nb, nn);
        if (nn == 1) return varref(nb[0] - 'a');
        if (nn == 5 && nb[0] == 's' && nb[1] == 'c' && nb[2] == 'a'
            && nb[3] == 'l' && nb[4] == 'e') return varref(VAR_SCALE);
        g_err = 1;
        return bc_num_from_int(&g_arena, 0);
        }
    }
    g_err = 1;
    return bc_num_from_int(&g_arena, 0);
}

/* ------------------------------------------------------------------ powers */
static BcNum* eval_unary(const char** p);

static BcNum* eval_pow(const char** p)
{
    BcNum* a = eval_unary(p);
    ws(p);
    if (**p == '^')
        {
        BcNum* b;
        int e;
        ++*p;
        b = eval_pow(p);
        e = bc_num_to_int(b);
        return bc_num_pow(&g_arena, a, e);
        }
    return a;
}

static BcNum* eval_unary(const char** p)
{
    ws(p);
    if (**p == '-')
        {
        BcNum* a;
        ++*p;
        a = eval_unary(p);
        return bc_num_negate(&g_arena, a);
        }
    if (**p == '+') { ++*p; return eval_unary(p); }
    if (**p == '!')
        {
        BcNum* a;
        ++*p;
        a = eval_unary(p);
        return bc_num_from_int(&g_arena, bc_num_is_zero(a));
        }
    return eval_primary(p);
}

static BcNum* eval_mul(const char** p)
{
    BcNum* a = eval_pow(p);
    for (;;)
        {
        char op;
        BcNum* b;
        ws(p);
        op = **p;
        if (op != '*' && op != '/' && op != '%') break;
        ++*p;
        b = eval_pow(p);
        if (op == '*') a = bc_num_mul(&g_arena, a, b);
        else if (op == '/')
            {
            if (bc_num_is_zero(b)) { g_err = 1; a = bc_num_from_int(&g_arena, 0); }
            else a = bc_num_div(&g_arena, a, b, scale_value());
            }
        else
            {
            if (bc_num_is_zero(b)) { g_err = 1; a = bc_num_from_int(&g_arena, 0); }
            else a = bc_num_mod(&g_arena, a, b);
            }
        if (!a) a = bc_num_from_int(&g_arena, 0);
        }
    return a;
}

static BcNum* eval_add(const char** p)
{
    BcNum* a = eval_mul(p);
    for (;;)
        {
        char op;
        BcNum* b;
        ws(p);
        op = **p;
        if (op != '+' && op != '-') break;
        ++*p;
        b = eval_mul(p);
        a = op == '+' ? bc_num_add(&g_arena, a, b)
                      : bc_num_sub(&g_arena, a, b);
        if (!a) a = bc_num_from_int(&g_arena, 0);
        }
    return a;
}

static BcNum* eval_rel(const char** p)
{
    BcNum* a = eval_add(p);
    ws(p);
    if (**p == '<' || **p == '>'
        || (**p == '=' && (*p)[1] == '=')
        || (**p == '!' && (*p)[1] == '='))
        {
        char op = **p;
        BcNum* b;
        int c, r = 0;
        if ((*p)[1] == '=') *p += 2; else ++*p;
        b = eval_add(p);
        c = bc_num_cmp(a, b);
        if (op == '<') r = c < 0;
        else if (op == '>') r = c > 0;
        else if (op == '=') r = c == 0;
        else if (op == '!') r = c != 0;
        else if ((*p)[-2] == '<') r = c <= 0;
        else if ((*p)[-2] == '>') r = c >= 0;
        return bc_num_from_int(&g_arena, r);
        }
    return a;
}

/* -------------------------------------------------------------- assignment */
static BcNum* eval_assign(const char** p, int* suppress)
{
    const char* save = *p;
    int vi = -1;
    ws(p);
    if (parse_ident(p, &vi))
        {
        const char* q = *p;
        char op;
        ws(&q);
        op = *q;
        if (op == '=' || (op && (op == '+' || op == '-' || op == '*'
            || op == '/' || op == '%' || op == '^') && q[1] == '='))
            {
            BcNum* lhs = varref(vi);
            BcNum* rhs;
            BcNum* res;
            *p = q + 1;
            if (op != '=') ++*p;
            rhs = eval_assign(p, suppress);
            if (op == '=') res = bc_num_copy(&g_arena, rhs);
            else if (op == '+') res = bc_num_add(&g_arena, lhs, rhs);
            else if (op == '-') res = bc_num_sub(&g_arena, lhs, rhs);
            else if (op == '*') res = bc_num_mul(&g_arena, lhs, rhs);
            else if (op == '/')
                res = bc_num_is_zero(rhs) ? bc_num_from_int(&g_arena, 0)
                                          : bc_num_div(&g_arena, lhs, rhs, scale_value());
            else if (op == '%')
                res = bc_num_is_zero(rhs) ? bc_num_from_int(&g_arena, 0)
                                          : bc_num_mod(&g_arena, lhs, rhs);
            else res = bc_num_pow(&g_arena, lhs, bc_num_to_int(rhs));
            if (res && bc_num_is_zero(rhs) && op == '/') g_err = 1;
            if (res) var_assign(vi, res);
            *suppress = 1;
            return res ? res : rhs;
            }
        }
    *p = save;
    return eval_rel(p);
}

static BcNum* eval_expr(const char** p)
{
    int suppress = 0;
    return eval_assign(p, &suppress);
}

/* ---------------------------------------------------------------- statements */
static void define_statement(const char** p)
{
    char name[16];
    int pv[MAXPARAM];
    int np = 0, slot = -1;
    const char* body;
    int i, depth, blen;
    ws(p);
    i = 0;
    while (**p >= 'a' && **p <= 'z' && i < 15) name[i++] = *(*p)++;
    name[i] = 0;
    ws(p);
    if (**p != '(') { g_err = 1; return; }
    ++*p;
    ws(p);
    if (**p != ')')
        {
        for (;;)
            {
            int vi;
            ws(p);
            if (parse_ident(p, &vi))
                {
                if (np < MAXPARAM) pv[np++] = vi;
                }
            else
                g_err = 1;
            ws(p);
            if (**p == ',') { ++*p; continue; }
            break;
            }
        }
    ws(p);
    if (**p == ')') ++*p; else g_err = 1;
    ws(p);
    if (**p != '{') { g_err = 1; return; }
    body = *p;
    depth = 0;
    while (**p)
        {
        if (**p == '{') ++depth;
        else if (**p == '}')
            {
            --depth;
            if (depth == 0) { ++*p; break; }
            }
        ++*p;
        }
    if (depth != 0) { g_err = 1; return; }
    for (i = 0; i < MAXFUN; ++i)
        if (!g_fun[i].used) { slot = i; break; }
    if (slot < 0)
        for (i = 0; i < MAXFUN; ++i)
            if (seq(g_fun[i].name, name)) { slot = i; break; }
    if (slot < 0) { g_err = 1; return; }
    copy_str(g_fun[slot].name, name, slen(name));
    g_fun[slot].used = 1;
    g_fun[slot].np = np;
    for (i = 0; i < np; ++i) g_fun[slot].pvar[i] = pv[i];
    blen = (int)(*p - body);
    if (blen >= FUNBODY) blen = FUNBODY - 1;
    copy_str(g_fun[slot].body, body, blen);
    g_fun[slot].body[blen] = 0;
}

static void expression_statement(const char** p)
{
    const char* b = *p;
    const char* e;
    int suppress = 0;
    BcNum* v = eval_assign(p, &suppress);
    ws(p);
    e = *p;
    while (e > b && (e[-1] == ' ' || e[-1] == '\t')) --e;
    if (!suppress && e > b) emit_value(b, (int)(e - b), v);
    if (**p == ';') ++*p;
}

static void print_statement(const char** p)
{
    char out[LINEBUF];
    char val[LINEBUF];
    int n = 0, i;
    out[0] = 0;
    for (;;)
        {
        BcNum* v;
        ws(p);
        if (**p == ';') { ++*p; break; }
        if (**p == '\n' || **p == '}' || **p == 0) break;
        if (**p == '"')
            {
            ++*p;
            while (**p && **p != '"')
                {
                if (n < LINEBUF - 1) out[n++] = **p;
                ++*p;
                }
            if (**p == '"') ++*p;
            ws(p);
            if (**p == ',') { ++*p; continue; }
            if (**p == ';') ++*p;
            break;
            }
        v = eval_expr(p);
        bc_num_write(v, val, sizeof(val));
        for (i = 0; val[i] && n < LINEBUF - 1; ++i) out[n++] = val[i];
        ws(p);
        if (**p == ',') { ++*p; continue; }
        if (**p == ';') ++*p;
        break;
        }
    out[n] = 0;
    emit_text("BC: print: ", out);
}

static void if_statement(const char** p)
{
    BcNum* c;
    int truth;
    ws(p);
    if (**p != '(') { g_err = 1; return; }
    ++*p;
    { int s = 0; c = eval_assign(p, &s); }
    ws(p);
    if (**p == ')') ++*p; else g_err = 1;
    truth = !bc_num_is_zero(c);
    if (truth) exec_statement(p);
    else
        {
        *p = find_stmt_end(*p);
        ws(p);
        if (kw(p, "else"))
            {
            ws(p);
            if (**p == 'i') exec_statement(p);
            else *p = find_stmt_end(*p);
            }
        }
}

static void while_statement(const char** p)
{
    const char* cond;
    const char* body;
    const char* end;
    int iter;
    ws(p);
    if (**p != '(') { g_err = 1; return; }
    ++*p;
    cond = *p;
    { int s = 0; (void)eval_assign(p, &s); }
    ws(p);
    if (**p == ')') ++*p; else g_err = 1;
    ws(p);
    body = *p;
    end = find_stmt_end(body);
    *p = end;
    for (iter = 0; iter < MAXITER; ++iter)
        {
        const char* cp = cond;
        const char* bp = body;
        BcNum* c;
        int s = 0;
        c = eval_assign(&cp, &s);
        if (bc_num_is_zero(c)) break;
        exec_statement(&bp);
        if (g_brk) { g_brk = 0; break; }
        if (g_cont) { g_cont = 0; continue; }
        if (g_ret) break;
        }
}

static void for_statement(const char** p)
{
    const char* cond = 0;
    const char* inc = 0;
    const char* body;
    const char* end;
    int iter;
    ws(p);
    if (**p != '(') { g_err = 1; return; }
    ++*p;
    { int s = 0; (void)eval_assign(p, &s); }   /* init */
    ws(p);
    if (**p == ';') ++*p; else g_err = 1;
    ws(p);
    cond = *p;
    while (**p && **p != ';') ++*p;            /* find condition end */
    if (**p == ';') ++*p; else g_err = 1;
    ws(p);
    inc = *p;
    while (**p && **p != ')') ++*p;
    if (**p == ')') ++*p; else g_err = 1;
    ws(p);
    body = *p;
    end = find_stmt_end(body);
    *p = end;
    for (iter = 0; iter < MAXITER; ++iter)
        {
        const char* cp = cond;
        const char* bp = body;
        const char* ip = inc;
        BcNum* c;
        int s = 0;
        c = eval_assign(&cp, &s);
        if (bc_num_is_zero(c)) break;
        exec_statement(&bp);
        if (g_brk) { g_brk = 0; break; }
        if (g_cont) { /* fall through to increment */ }
        if (g_ret) break;
        (void)eval_assign(&ip, &s);
        }
}

static void exec_statement(const char** p)
{
    const char* save;
    ws(p);
    save = *p;
    if (kw(p, "define")) { define_statement(p); return; }
    *p = save;
    if (kw(p, "print")) { print_statement(p); return; }
    *p = save;
    if (kw(p, "return"))
        {
        const char* q = *p;
        ws(&q);
        if (*q != ';' && *q != '\n' && *q != '}' && *q)
            g_retval = eval_expr(&q);
        else g_retval = bc_num_from_int(&g_arena, 0);
        *p = q;
        g_ret = 1;
        if (**p == ';') ++*p;
        return;
        }
    *p = save;
    if (kw(p, "break")) { g_brk = 1; if (**p == ';') ++*p; return; }
    *p = save;
    if (kw(p, "continue")) { g_cont = 1; if (**p == ';') ++*p; return; }
    *p = save;
    if (kw(p, "if")) { if_statement(p); return; }
    *p = save;
    if (kw(p, "while")) { while_statement(p); return; }
    *p = save;
    if (kw(p, "for")) { for_statement(p); return; }
    *p = save;
    if (**p == '{')
        {
        ++*p;
        exec_statements(p, '}');
        if (g_ret || g_brk || g_cont)
            {
            /* A return/break/continue unwinds the remaining statements of
               this block; skip to its closing brace without executing them. */
            const char* q = *p;
            int depth = 0;
            while (*q)
                {
                if (*q == '{') ++depth;
                else if (*q == '}')
                    {
                    if (depth == 0) { ++q; break; }
                    --depth;
                    }
                ++q;
                }
            *p = q;
            return;
            }
        ws(p);
        if (**p == '}') ++*p; else g_err = 1;
        return;
        }
    expression_statement(p);
}

static void exec_statements(const char** p, char stop)
{
    for (;;)
        {
        const char* before;
        skip_seps(p);
        if (**p == 0) return;
        if (stop && **p == stop) return;
        before = *p;
        exec_statement(p);
        if (g_ret || g_brk || g_cont) return;
        if (*p == before && **p) ++*p;   /* guarantee forward progress */
        }
}

/* ------------------------------------------------------------------ public */
void bc_setup(BcEmit aEmit)
{
    int i;
    g_emit = aEmit;
    bc_arena_init(&g_arena, g_arena_mem, ARENA_SIZE);
    for (i = 0; i < MAXFUN; ++i) g_fun[i].used = 0;
    for (i = 0; i < NVAR; ++i)
        {
        g_var[i].len = 1;
        g_var[i].d[0] = 0;
        g_var[i].scale = 0;
        g_var[i].neg = 0;
        var_sync(i);
        }
    g_depth = 0;
}

void bc_run(const char* text)
{
    const char* p = text;
    bc_arena_reset(&g_arena);
    g_err = 0;
    g_ret = 0; g_brk = 0; g_cont = 0; g_retval = 0;
    exec_statements(&p, 0);
    if (g_err) emit_text("BC: error", "");
}
