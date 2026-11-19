#include "runtime.h"

/* Chunk-backed first-fit allocator for single-threaded user programs.
   The old static 16 KiB pool is replaced by a private RChunk: the first malloc
   creates it with chunk_create (genuine EExecChunkCreate) and OOM grows it
   with chunk_adjust (EExecChunkAdjust). The free list lives inside the chunk;
   free() coalesces both neighbours so allocate/free cycles stay stable.
   No locks: user-thread isolation is M-03, not this layer. Growth is capped
   below the 100 KiB oversize probe in malloctest.c. */

#define ALIGN 8
#define INITIAL_CHUNK 16384u
#define GROW_STEP 16384u
#define MAX_CHUNK 65536u

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

void free(void* ptr)
{
    struct Block* b;
    if (!ptr) return;
    b = (struct Block*)((unsigned char*)ptr - sizeof(struct Block));
    /* Sanity: inside the committed chunk, 8-aligned, plausible size. */
    if ((unsigned char*)b < heap_base || (unsigned char*)b >= heap_base + heap_size ||
        ((unsigned)ptr & (ALIGN - 1)) ||
        b->size < sizeof(struct Block) + ALIGN || b->size > heap_size)
        return;
    insert_block(b);
}
