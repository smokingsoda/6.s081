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
    ref_count.ref[(uint64)pa / PGSIZE] -= 1;
    if (ref_count.ref[(uint64)pa / PGSIZE] <= 0) {
        // Fill with junk to catch dangling refs.
        memset(pa, 1, PGSIZE);

        r = (struct run *)pa;

        acquire(&kmem.lock);
        r->next = kmem.freelist;
        kmem.freelist = r;
        release(&kmem.lock);
        ref_count.ref[(uint64)pa / PGSIZE] = 0;
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

    if (r) {
        memset((char *)r, 5, PGSIZE); // fill with junk
        ref_count.ref[(uint64)r / PGSIZE] = 1;
    }
    return (void *)r;
}

void increment_ref(void *pa) {
    if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
        return;
    acquire(&ref_count.lock);
    ref_count.ref[(uint64)pa / PGSIZE] += 1;
    release(&ref_count.lock);
}

int uvm_cow_copy(pagetable_t pagetable, uint64 va) {
    pte_t *pte;
    if ((pte = walk(pagetable, va, 0)) == 0) {
        panic("uvmcowcopy: walk");
    }
    va = PGROUNDDOWN(va);
    uint64 pa = PTE2PA(*pte);
    uint64 new_pa = (uint64)kcopy_n_deref((void *)pa);
    if (new_pa == 0) {
        return -1;
    }

    uint64 flag = (PTE_FLAGS(*pte) | PTE_W) & ~PTE_COW;
    uvmunmap(pagetable, PGROUNDDOWN(va), 1, 0);
    if (mappages(pagetable, va, 1, new_pa, flag) == -1) {
        panic("uvmcowcopy: mappages");
    }
    return 0;
}

void *kcopy_n_deref(void *pa) {
    acquire(&ref_count.lock);

    if (ref_count.ref[(uint64)pa / PGSIZE] <= 1) {
        release(&ref_count.lock);
        return pa;
    }

    uint64 new_pa = (uint64)kalloc();
    if (new_pa == 0) {
        release(&ref_count.lock);
        return 0; // out of memory
    }
    memmove((void *)new_pa, (void *)pa, PGSIZE);
    ref_count.ref[(uint64)pa / PGSIZE] -= 1;

    release(&ref_count.lock);
    return (void *)new_pa;
}