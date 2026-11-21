/* miniz.c -- genuine, freestanding RFC1951 DEFLATE / RFC1952 support.
 * See miniz.h for an overview.  All work areas are static (no heap, no libc).
 */
#include "miniz.h"

/* ------------------------------------------------------------------ helpers */
static void mz_zero(void* p, unsigned n)
{
    unsigned char* q = (unsigned char*)p;
    while (n--) *q++ = 0;
}
static void mz_copy(unsigned char* d, const unsigned char* s, unsigned n)
{
    while (n--) *d++ = *s++;
}

mz_ulong mz_crc32(mz_ulong crc, const unsigned char* p, unsigned n)
{
    unsigned int c = (unsigned int)~crc;   /* keep arithmetic strictly 32-bit */
    while (n--)
        {
        int k;
        c ^= *p++;
        for (k = 0; k < 8; ++k)
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
        }
    return (mz_ulong)(~c & 0xFFFFFFFFu);
}

mz_ulong mz_adler32(mz_ulong adler, const unsigned char* p, unsigned n)
{
    unsigned long s1 = adler & 0xFFFFu, s2 = (adler >> 16) & 0xFFFFu;
    while (n--)
        {
        s1 += *p++;
        if (s1 >= 65521u) s1 -= 65521u;       /* safe: max 255+65520 */
        s2 += s1;
        if (s2 >= 65521u) s2 -= 65521u;       /* safe within one byte step */
        }
    return (s2 << 16) | s1;
}

/* ------------------------------------------------------- RFC1951 code tables */
static const unsigned short kLenBase[29] = {
    3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,
    67,83,99,115,131,163,195,227,258
};
static const unsigned char kLenExtra[29] = {
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0
};
static const unsigned short kDistBase[30] = {
    1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,
    1025,1537,2049,3073,4097,6145,8193,12289,16385,24577
};
static const unsigned char kDistExtra[30] = {
    0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13
};

/* ============================================================== DEFLATE ==== */

/* Hash chains for LZ77 match finding.  Positions are relative to the current
 * src buffer; 0 in kHead means "empty", otherwise it stores position+1. */
#define MZ_WIN      8192u          /* match window (bytes) */
#define MZ_HSIZE    8192u          /* hash buckets */
#define MZ_CHAIN    8              /* candidates inspected per position */
static unsigned short s_head[MZ_HSIZE];   /* 16 KiB */
static unsigned short s_prev[MZ_WIN];     /* 16 KiB */

static unsigned mz_hash(const unsigned char* p)
{
    unsigned h = ((unsigned)p[0] << 10) ^ ((unsigned)p[1] << 5) ^ (unsigned)p[2];
    h ^= h >> 7;
    return h & (MZ_HSIZE - 1);
}

static void mz_insert(const unsigned char* src, unsigned pos)
{
    unsigned h = mz_hash(src + pos);
    s_prev[pos & (MZ_WIN - 1)] = s_head[h];
    s_head[h] = (unsigned short)(pos + 1);
}

/* Search the hash chain for the longest match of src[pos..] of at most maxlen.
 * Returns the match length (0 if < 3) and writes the distance. */
static unsigned mz_match(const unsigned char* src, unsigned pos,
                         unsigned maxlen, unsigned* dist)
{
    unsigned h = mz_hash(src + pos);
    unsigned cand = s_head[h] ? (unsigned)s_head[h] - 1 : 0xFFFFFFFFu;
    unsigned best = 0, bestd = 0;
    int chain = MZ_CHAIN;

    while (cand != 0xFFFFFFFFu && pos - cand <= MZ_WIN && chain-- > 0)
        {
        unsigned l = 0;
        if (src[cand + best] == src[pos + best])
            {
            while (l < maxlen && src[cand + l] == src[pos + l]) ++l;
            }
        if (l > best)
            {
            best = l;
            bestd = pos - cand;
            if (best >= maxlen) break;
            }
        {
        unsigned nx = s_prev[cand & (MZ_WIN - 1)];
        cand = nx ? (unsigned)nx - 1 : 0xFFFFFFFFu;
        }
        }
    if (best < 3) { *dist = 0; return 0; }
    *dist = bestd;
    return best;
}

/* ---- bit writer (LSB-first packing, as required by RFC1951) ---- */
typedef struct {
    unsigned char* out;
    unsigned cap, pos;
    unsigned bits;
    int nbits;
    int overflow;
} mz_bitw;

static void mz_put(mz_bitw* w, unsigned val, int n)
{
    if (n > 0) w->bits |= (val & ((1u << n) - 1u)) << w->nbits;
    w->nbits += n;
    while (w->nbits >= 8)
        {
        if (w->pos >= w->cap) { w->overflow = 1; w->nbits = 0; return; }
        w->out[w->pos++] = (unsigned char)(w->bits & 0xFFu);
        w->bits >>= 8;
        w->nbits -= 8;
        }
}

static void mz_flush(mz_bitw* w)
{
    if (w->nbits > 0)
        {
        if (w->pos >= w->cap) { w->overflow = 1; return; }
        w->out[w->pos++] = (unsigned char)(w->bits & 0xFFu);
        w->bits = 0;
        w->nbits = 0;
        }
}

static unsigned mz_reverse(unsigned code, int n)
{
    unsigned r = 0;
    while (n-- > 0) { r = (r << 1) | (code & 1u); code >>= 1; }
    return r;
}

/* Emit a literal/length symbol using the fixed Huffman code of RFC1951.
 * Huffman codes travel MSB-first but DEFLATE packs bits LSB-first, so the
 * canonical code must be bit-reversed before mz_put(). */
static void mz_put_ll(mz_bitw* w, unsigned sym)
{
    unsigned code;
    int n;
    if (sym <= 143u)      { code = 0x30u + sym;         n = 8; }
    else if (sym <= 255u) { code = 0x190u + sym - 144u; n = 9; }
    else if (sym <= 279u) { code = sym - 256u;          n = 7; }
    else                  { code = 0xC0u + sym - 280u;  n = 8; }
    mz_put(w, mz_reverse(code, n), n);
}

/* Stored (uncompressed) block fallback -- always valid DEFLATE. */
static int mz_stored(const unsigned char* src, unsigned n,
                     unsigned char* dst, unsigned cap, unsigned* dst_len)
{
    unsigned pos = 0, off = 0;
    unsigned blocks = 1u + n / 65535u;
    if (cap < n + 5u * blocks + 1u) return 0;
    for (;;)
        {
        unsigned chunk = n - off;
        unsigned inv;
        int last;
        if (chunk > 65535u) chunk = 65535u;
        last = (off + chunk >= n);
        dst[pos++] = (unsigned char)(last ? 1 : 0);      /* BFINAL, BTYPE=00 */
        dst[pos++] = (unsigned char)(chunk & 0xFFu);
        dst[pos++] = (unsigned char)((chunk >> 8) & 0xFFu);
        inv = (~chunk) & 0xFFFFu;
        dst[pos++] = (unsigned char)(inv & 0xFFu);
        dst[pos++] = (unsigned char)((inv >> 8) & 0xFFu);
        mz_copy(dst + pos, src + off, chunk);
        pos += chunk;
        off += chunk;
        if (last) break;
        }
    *dst_len = pos;
    return 1;
}

int mz_deflate(const unsigned char* src, unsigned src_len,
               unsigned char* dst, unsigned dst_cap, unsigned* dst_len)
{
    mz_bitw w;
    unsigned i, comp;

    if (!src || !dst || !dst_len || dst_cap < 64u) return 0;
    mz_zero(s_head, sizeof(s_head));
    /* s_prev need not be cleared: every entry read is reachable from s_head. */

    w.out = dst; w.cap = dst_cap; w.pos = 0;
    w.bits = 0; w.nbits = 0; w.overflow = 0;

    mz_put(&w, 1, 1);       /* BFINAL = 1 */
    mz_put(&w, 1, 2);       /* BTYPE  = 01 (fixed Huffman) */

    i = 0;
    while (i < src_len)
        {
        unsigned maxlen = src_len - i;
        unsigned dist = 0, len = 0;
        if (maxlen > 258u) maxlen = 258u;
        if (maxlen >= 3u && i + 2u < src_len)
            len = mz_match(src, i, maxlen, &dist);

        if (len >= 3u)
            {
            unsigned lc = 28, dc = 29, j;
            for (j = 0; j < 28; ++j)
                if (len < kLenBase[j + 1]) { lc = j; break; }
            for (j = 0; j < 29; ++j)
                if (dist < kDistBase[j + 1]) { dc = j; break; }

            mz_put_ll(&w, 257u + lc);
            mz_put(&w, len - kLenBase[lc], kLenExtra[lc]);
            mz_put(&w, mz_reverse(dc, 5), 5);
            mz_put(&w, dist - kDistBase[dc], kDistExtra[dc]);
            for (j = 0; j < len; ++j) mz_insert(src, i + j);
            i += len;
            }
        else
            {
            mz_put_ll(&w, src[i]);
            mz_insert(src, i);
            ++i;
            }
        if (w.overflow) return mz_stored(src, src_len, dst, dst_cap, dst_len);
        }

    mz_put_ll(&w, 256u);     /* end-of-block */
    mz_flush(&w);
    if (w.overflow) return mz_stored(src, src_len, dst, dst_cap, dst_len);

    comp = w.pos;
    if (comp >= src_len)     /* no gain: emit a plain stored block */
        return mz_stored(src, src_len, dst, dst_cap, dst_len);
    *dst_len = comp;
    return 1;
}

/* ============================================================== INFLATE ==== */

typedef struct {
    const unsigned char* in;
    unsigned len, pos;
    unsigned bits;
    int nbits;
} mz_bitr;

static int mz_get(mz_bitr* r, int n)
{
    unsigned v;
    if (n < 0 || n > 16) return -1;
    while (r->nbits < n)
        {
        if (r->pos >= r->len) return -1;
        r->bits |= (unsigned)r->in[r->pos++] << r->nbits;
        r->nbits += 8;
        }
    v = r->bits & ((1u << n) - 1u);
    r->bits >>= n;
    r->nbits -= n;
    return (int)v;
}

/* Canonical Huffman decoding table (puff-style; 288 symbols max). */
typedef struct {
    unsigned short count[16];
    unsigned short first[16];
    unsigned short index[16];
    unsigned short sym[288];
} mz_huff;

static void mz_build(mz_huff* h, const unsigned char* lengths, int n)
{
    unsigned cnt[16], offs[16], index = 0, code = 0;
    int bits, i;

    mz_zero(cnt, sizeof(cnt));
    for (i = 0; i < n; ++i) ++cnt[lengths[i]];
    cnt[0] = 0;
    for (bits = 1; bits <= 15; ++bits)
        {
        code = (code + cnt[bits - 1]) << 1;
        h->first[bits] = (unsigned short)code;
        h->index[bits] = (unsigned short)index;
        h->count[bits] = (unsigned short)cnt[bits];
        offs[bits] = index;
        index += cnt[bits];
        }
    for (i = 0; i < n; ++i)
        {
        int l = lengths[i];
        if (l) h->sym[offs[l]++] = (unsigned short)i;
        }
}

static int mz_decode(mz_huff* h, mz_bitr* r)
{
    int code = 0, first = 0, index = 0, len;
    for (len = 1; len <= 15; ++len)
        {
        int b = mz_get(r, 1);
        unsigned cnt;
        if (b < 0) return -1;
        code |= b;
        cnt = h->count[len];
        if (code - first < (int)cnt)
            return h->sym[index + (code - first)];
        index += (int)cnt;
        first = (first + (int)cnt) << 1;
        code <<= 1;
        }
    return -1;
}

static const unsigned char kClOrder[19] = {
    16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15
};

int mz_inflate(const unsigned char* src, unsigned src_len,
               unsigned char* dst, unsigned dst_cap, unsigned* dst_len)
{
    static mz_huff h_ll, h_dd, h_cl;
    mz_bitr r;
    unsigned outpos = 0;
    int final;

    if (!src || !dst || !dst_len) return 0;
    r.in = src; r.len = src_len; r.pos = 0; r.bits = 0; r.nbits = 0;

    do
        {
        int b = mz_get(&r, 1), type;
        if (b < 0) return 0;
        final = b;
        type = mz_get(&r, 2);
        if (type < 0) return 0;

        if (type == 0)                 /* stored block */
            {
            unsigned len, nlen;
            r.bits = 0; r.nbits = 0;   /* discard partial byte */
            if (r.pos + 4u > r.len) return 0;
            len  = (unsigned)src[r.pos] | ((unsigned)src[r.pos + 1] << 8);
            nlen = (unsigned)src[r.pos + 2] | ((unsigned)src[r.pos + 3] << 8);
            r.pos += 4;
            if ((len ^ 0xFFFFu) != nlen) return 0;
            if (len > dst_cap - outpos || len > r.len - r.pos) return 0;
            mz_copy(dst + outpos, src + r.pos, len);
            outpos += len;
            r.pos += len;
            }
        else if (type == 1 || type == 2)
            {
            unsigned char ll[288], dd[32];
            int s, i;

            if (type == 1)             /* fixed Huffman */
                {
                for (i = 0; i < 288; ++i)
                    ll[i] = (unsigned char)(i <= 143 ? 8 : i <= 255 ? 9
                                            : i <= 279 ? 7 : 8);
                for (i = 0; i < 32; ++i) dd[i] = 5;
                }
            else                       /* dynamic Huffman */
                {
                int hl, hd, hc, total, k, v;
                unsigned char cl[19];
                hl = mz_get(&r, 5); if (hl < 0) return 0; hl += 257;
                hd = mz_get(&r, 5); if (hd < 0) return 0; hd += 1;
                hc = mz_get(&r, 4); if (hc < 0) return 0; hc += 4;
                if (hl > 288 || hd > 32 || hc > 19) return 0;
                mz_zero(cl, sizeof(cl));
                for (i = 0; i < hc; ++i)
                    {
                    v = mz_get(&r, 3); if (v < 0) return 0;
                    cl[kClOrder[i]] = (unsigned char)v;
                    }
                mz_build(&h_cl, cl, 19);
                mz_zero(ll, sizeof(ll));
                mz_zero(dd, sizeof(dd));
                total = hl + hd;
                k = 0;
                while (k < total)
                    {
                    int rep, prev;
                    s = mz_decode(&h_cl, &r);
                    if (s < 0) return 0;
                    if (s <= 15)
                        {
                        if (k < hl) ll[k] = (unsigned char)s;
                        else dd[k - hl] = (unsigned char)s;
                        ++k;
                        }
                    else if (s == 16)
                        {
                        if (k == 0) return 0;
                        v = mz_get(&r, 2); if (v < 0) return 0;
                        prev = (k <= hl) ? ll[k - 1] : dd[k - hl - 1];
                        rep = 3 + v;
                        while (rep-- > 0 && k < total)
                            {
                            if (k < hl) ll[k] = (unsigned char)prev;
                            else dd[k - hl] = (unsigned char)prev;
                            ++k;
                            }
                        }
                    else if (s == 17)
                        {
                        v = mz_get(&r, 3); if (v < 0) return 0;
                        rep = 3 + v;
                        while (rep-- > 0 && k < total)
                            { if (k < hl) ll[k] = 0; else dd[k - hl] = 0; ++k; }
                        }
                    else if (s == 18)
                        {
                        v = mz_get(&r, 7); if (v < 0) return 0;
                        rep = 11 + v;
                        while (rep-- > 0 && k < total)
                            { if (k < hl) ll[k] = 0; else dd[k - hl] = 0; ++k; }
                        }
                    else return 0;
                    }
                }
            mz_build(&h_ll, ll, 288);
            mz_build(&h_dd, dd, 32);

            for (;;)
                {
                unsigned length, dist;
                int li, di, eb, db;
                s = mz_decode(&h_ll, &r);
                if (s < 0) return 0;
                if (s < 256)
                    {
                    if (outpos >= dst_cap) return 0;
                    dst[outpos++] = (unsigned char)s;
                    continue;
                    }
                if (s == 256) break;
                if (s < 257 || s > 285) return 0;
                li = s - 257;
                eb = mz_get(&r, kLenExtra[li]); if (eb < 0) return 0;
                length = kLenBase[li] + (unsigned)eb;
                di = mz_decode(&h_dd, &r);
                if (di < 0 || di >= 30) return 0;
                db = mz_get(&r, kDistExtra[di]); if (db < 0) return 0;
                dist = kDistBase[di] + (unsigned)db;
                if (dist == 0 || dist > outpos) return 0;
                if (length > dst_cap - outpos) return 0;
                while (length--)
                    { dst[outpos] = dst[outpos - dist]; ++outpos; }
                }
            }
        else return 0;                 /* BTYPE = 11 is reserved */
        }
    while (!final);

    *dst_len = outpos;
    return 1;
}
