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

struct {
    struct spinlock lock;
    uint64 ref[NPAGE];
} ref_count;

void kinit() {
    initlock(&kmem.lock, "kmem");
    initlock(&ref_count.lock, "ref_count");
    freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end) {
    char *p;
    p = (char *)PGROUNDUP((uint64)pa_start);
    for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE) {
        acquire(&ref_count.lock);
        ref_count.ref[(uint64)p / PGSIZE] = 0;
        release(&ref_count.lock);
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
    acquire(&ref_count.lock);
    if (ref_count.ref[(uint64)pa / PGSIZE] <= 1) {
        // Fill with junk to catch dangling refs.
        memset(pa, 1, PGSIZE);

        r = (struct run *)pa;

        acquire(&kmem.lock);
        r->next = kmem.freelist;
        kmem.freelist = r;
        ref_count.ref[(uint64)pa / PGSIZE] = 0;
        release(&kmem.lock);
    } else if (ref_count.ref[(uint64)pa / PGSIZE] > 1) {
        acquire(&kmem.lock);
        ref_count.ref[(uint64)pa / PGSIZE] -= 1;
        release(&kmem.lock);
    }
    release(&ref_count.lock);
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

    if (r)
        memset((char *)r, 5, PGSIZE); // fill with junk

    acquire(&ref_count.lock);
    if (r) {
        ref_count.ref[(uint64)r / PGSIZE] += 1;
    }
    release(&ref_count.lock);
    return (void *)r;
}

void grow_ref(void *pa, uint64 count) {
    acquire(&ref_count.lock);
    ref_count.ref[(uint64)pa / PGSIZE] += count;
    release(&ref_count.lock);
}

uint64 get_ref(void *pa) {
    uint64 ref;
    acquire(&ref_count.lock);
    ref = ref_count.ref[(uint64)pa / PGSIZE];
    release(&ref_count.lock);
    return ref;
}
