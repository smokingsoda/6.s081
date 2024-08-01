// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
    struct run *next;
};

struct {
    struct spinlock lock;
    struct run *freelist;
} kmem;

#define PA2PGREF_ID(p) (((p) - KERNBASE) / PGSIZE)
#define PGREF_MAX_ENTRIES PA2PGREF_ID(PHYSTOP)

struct spinlock lock;
int pageref[PGREF_MAX_ENTRIES]; // Must be int or short

#define PA2PGREF(p) pageref[PA2PGREF_ID((uint64)(p))]

void kinit() {
    initlock(&kmem.lock, "kmem");
    initlock(&lock, "ref_count");
    freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end) {
    char *p;
    p = (char *)PGROUNDUP((uint64)pa_start);
    for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE) {
        kfree(p);
    }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa) {
    struct run *r;

    if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end ||
        (uint64)pa >= PHYSTOP) {
        panic("kfree");
    }
    acquire(&lock);
    PA2PGREF(pa) -= 1;
    if (PA2PGREF(pa) <= 0) {
        // Fill with junk to catch dangling refs.
        memset(pa, 1, PGSIZE);

        r = (struct run *)pa;

        acquire(&kmem.lock);
        r->next = kmem.freelist;
        kmem.freelist = r;
        release(&kmem.lock);
        PA2PGREF(pa) = 0;
    }
    release(&lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *kalloc(void) {
    struct run *r;

    acquire(&kmem.lock);
    r = kmem.freelist;
    if (r)
        kmem.freelist = r->next;
    release(&kmem.lock);

    if (r) {
        memset((char *)r, 5, PGSIZE); // fill with junk
        PA2PGREF(r) = 1;
    }
    return (void *)r;
}

void increment_ref(void *pa) {
    if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
        return;
    acquire(&lock);
    PA2PGREF(pa) += 1;
    release(&lock);
}

void *kcopy_n_deref(void *pa) {
    acquire(&lock);

    if (PA2PGREF(pa) <= 1) {
        release(&lock);
        return pa;
    }

    uint64 new_pa = (uint64)kalloc();
    if (new_pa == 0) {
        release(&lock);
        return 0; // out of memory
    }
    memmove((void *)new_pa, (void *)pa, PGSIZE);
    PA2PGREF(pa) -= 1;

    release(&lock);
    return (void *)new_pa;
}