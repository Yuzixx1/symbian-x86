#include "runtime.h"

/* Chunk-backed first-fit allocator for single-threaded user programs.
   The old static 16 KiB pool is replaced by a private RChunk: the first malloc
   creates it with chunk_create (genuine EExecChunkCreate) and OOM grows it
   with chunk_adjust (EExecChunkAdjust). The free list lives inside the chunk;
   free() coalesces both neighbours so allocate/free cycles stay stable.
   No locks: user-thread isolation is M-03, not this layer.

   Heap ceiling raised from 64 KiB to 4 MiB (0x400000). This is a genuine
   RChunk ceiling, not a kernel cap: the Direct memory model backs chunks with
   reserved RAM from the 128 MiB QEMU image, and the 107/108 executives really
   create/adjust the chunk. 4 MiB is deliberate: it covers a DOOM-class
   framebuffer/texture working set and a Lua-class GC heap while leaving ample
   RAM for the kernel, VBE framebuffer and other processes. NB: the old
   `malloc(100000)` "oversize must fail" probe in malloctest.c is obsolete
   under this ceiling -- a 100 KiB allocation is now expected to succeed. */

#define ALIGN 8
#define INITIAL_CHUNK 16384u      /* start small, grow on demand to MAX_CHUNK */
#define GROW_STEP 16384u
#define MAX_CHUNK 0x400000u       /* 4 MiB genuine chunk-backed user heap */

struct Block { unsigned size; struct Block* next; };  /* size incl. header */

static struct Block* free_list = 0;
static int heap_handle = -1;
static unsigned char* heap_base = 0;
static unsigned heap_size = 0;   /* bytes currently committed to the allocator */

static unsigned align_up(unsigned n) { return (n + ALIGN - 1) & ~(unsigned)(ALIGN - 1); }

/* Insert into the address-ordered free list, coalescing with both neighbours.
   Shared by free() and by grow() when a fresh trailing span is added. */
static void insert_block(struct Block* b)
{
    struct Block** prev = &free_list;
    while (*prev && *prev < b) prev = &(*prev)->next;
    b->next = *prev;
    *prev = b;
    /* Coalesce with next. */
    if (b->next && (unsigned char*)b + b->size == (unsigned char*)b->next)
        {
        b->size += b->next->size;
        b->next = b->next->next;
        }
    /* Coalesce with previous. */
    {
    struct Block* p = free_list;
    while (p && p->next != b) p = p->next;
    if (p && p != b && (unsigned char*)p + p->size == (unsigned char*)b)
        {
        p->size += b->size;
        p->next = b->next;
        }
    }
}

/* Hand [start, start+len) to the free list. */
static void add_span(unsigned char* start, unsigned len)
{
    struct Block* b = (struct Block*)start;
    b->size = len;
    insert_block(b);
}

static int init(void)
{
    unsigned base, aligned, size;
    int handle = chunk_create(0, INITIAL_CHUNK, MAX_CHUNK);
    if (handle < 0) return 0;
    size = (unsigned)chunk_size(handle);
    if (size < sizeof(struct Block) + ALIGN) return 0;
    base = (unsigned)chunk_base(handle);
    aligned = align_up(base);
    if (!aligned || aligned - base + sizeof(struct Block) + ALIGN > size) return 0;
    heap_handle = handle;
    heap_base = (unsigned char*)aligned;
    heap_size = size - (aligned - base);
    add_span(heap_base, heap_size);
    return 1;
}

/* Extend the chunk by GROW_STEP (or just enough) and add the new tail span.
   Returns 0 on kernel refusal or at MAX_CHUNK. */
static int grow(unsigned need)
{
    unsigned target = heap_size;
    while (target - heap_size < need && target < MAX_CHUNK)
        target += GROW_STEP;
    if (target > MAX_CHUNK) target = MAX_CHUNK;
    if (target <= heap_size) return 0;
    if (chunk_adjust(heap_handle, target) != 0) return 0;
    add_span(heap_base + heap_size, target - heap_size);
    heap_size = target;
    return 1;
}

void* malloc(unsigned size)
{
    if (!size) size = 1;
    size = align_up(size + sizeof(struct Block));
    /* No chunk can ever satisfy this: fail before creating/growing one. */
    if (size > MAX_CHUNK - sizeof(struct Block)) return 0;
    if (!free_list && !init()) return 0;
    for (;;)
        {
        struct Block** prev = &free_list;
        while (*prev)
            {
            struct Block* b = *prev;
            if (b->size >= size)
                {
                if (b->size - size >= sizeof(struct Block) + ALIGN)
                    {
                    struct Block* rest = (struct Block*)((unsigned char*)b + size);
                    rest->size = b->size - size;
                    rest->next = b->next;
                    b->size = size;
                    *prev = rest;
                    }
                else *prev = b->next;
                return (unsigned char*)b + sizeof(struct Block);
                }
            prev = &b->next;
            }
        if (!grow(size)) return 0;
        }
}

/* Validate a user pointer and recover its block header. 0 for NULL/invalid. */
static struct Block* block_of(void* ptr)
{
    struct Block* b;
    if (!ptr) return 0;
    b = (struct Block*)((unsigned char*)ptr - sizeof(struct Block));
    /* Sanity: inside the committed chunk, 8-aligned, plausible size. */
    if ((unsigned char*)b < heap_base || (unsigned char*)b >= heap_base + heap_size ||
        ((unsigned)ptr & (ALIGN - 1)) ||
        b->size < sizeof(struct Block) + ALIGN || b->size > heap_size)
        return 0;
    return b;
}

void free(void* ptr)
{
    struct Block* b = block_of(ptr);
    if (b) insert_block(b);
}

/* Zero-filled allocation. Overflow-checked before multiplying. */
void* calloc(unsigned count, unsigned size)
{
    unsigned total;
    unsigned char* p;
    if (size && count > 0xffffffffu / size) return 0;
    total = count * size;
    p = (unsigned char*)malloc(total ? total : 1);
    if (!p) return 0;
    for (unsigned i = 0; i < total; ++i) p[i] = 0;
    return p;
}

/* Resize an allocation, preserving the prefix. Shrinks in place; grows by
   allocating a new block, copying the old payload and releasing the old one.
   ptr == NULL behaves like malloc, new_size == 0 behaves like free. */
void* realloc(void* ptr, unsigned new_size)
{
    struct Block* b;
    unsigned old_size, copy, i;
    unsigned char* p;
    if (!ptr) return malloc(new_size);
    if (!new_size) { free(ptr); return 0; }
    b = block_of(ptr);
    if (!b) return 0;
    old_size = b->size - sizeof(struct Block);
    if (new_size <= old_size) return ptr;   /* shrink: keep the block */
    p = (unsigned char*)malloc(new_size);
    if (!p) return 0;                       /* old block still valid on OOM */
    copy = old_size < new_size ? old_size : new_size;
    for (i = 0; i < copy; ++i) p[i] = ((unsigned char*)ptr)[i];
    free(ptr);
    return p;
}
