#include "runtime.h"

/* Freestanding first-fit allocator over a static pool. Single-threaded
   user programs only (no locks); the kernel heap stays out of reach.
   Groundwork until chunk-backed growth exists: fixed 16 KiB, NULL on OOM,
   coalescing on free so allocate-all/free-all/repeat is stable. */

#define POOL_SIZE 16384
#define ALIGN 8

static unsigned char pool[POOL_SIZE];

struct Block { unsigned size; struct Block* next; };  /* size incl. header */
static struct Block* free_list = 0;
static int initialized = 0;

static unsigned align_up(unsigned n) { return (n + ALIGN - 1) & ~(unsigned)(ALIGN - 1); }

static void init(void)
{
    struct Block* b = (struct Block*)pool;
    b->size = POOL_SIZE;
    b->next = 0;
    free_list = b;
    initialized = 1;
}

void* malloc(unsigned size)
{
    if (!size) size = 1;
    size = align_up(size + sizeof(struct Block));
    if (!initialized) init();
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
    return 0;
}

void free(void* ptr)
{
    struct Block* b;
    struct Block** prev;
    if (!ptr) return;
    b = (struct Block*)((unsigned char*)ptr - sizeof(struct Block));
    /* Basic sanity: must land inside the pool, stay aligned, and carry a
       plausible size. (Double-free, like classic allocators, is unchecked.) */
    if ((unsigned char*)b < pool || (unsigned char*)b >= pool + POOL_SIZE ||
        ((unsigned)ptr & (ALIGN - 1)) ||
        b->size < sizeof(struct Block) + ALIGN || b->size > POOL_SIZE)
        return;
    if (!initialized) init();
    prev = &free_list;
    while (*prev && *prev < b) prev = &(*prev)->next;
    b->next = *prev;
    *prev = b;
    /* Coalesce with next. */
    if (b->next && (unsigned char*)b + b->size == (unsigned char*)b->next)
        {
        b->size += b->next->size;
        b->next = b->next->next;
        }
    /* Coalesce with previous: find the block ending at b. */
    {
    struct Block* p = free_list;
    while (p && p->next != b) p = p->next;
    /* p is NULL only if b is the head (no previous); otherwise check touch. */
    if (p && p != b && (unsigned char*)p + p->size == (unsigned char*)b)
        {
        p->size += b->size;
        p->next = b->next;
        }
    }
}
