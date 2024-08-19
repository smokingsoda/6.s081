#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "fcntl.h"

uint64 sys_exit(void) {
    int n;
    if (argint(0, &n) < 0)
        return -1;
    exit(n);
    return 0; // not reached
}

uint64 sys_getpid(void) { return myproc()->pid; }

uint64 sys_fork(void) { return fork(); }

uint64 sys_wait(void) {
    uint64 p;
    if (argaddr(0, &p) < 0)
        return -1;
    return wait(p);
}

uint64 sys_sbrk(void) {
    int addr;
    int n;

    if (argint(0, &n) < 0)
        return -1;
    addr = myproc()->sz;
    if (growproc(n) < 0)
        return -1;
    return addr;
}

uint64 sys_sleep(void) {
    int n;
    uint ticks0;

    if (argint(0, &n) < 0)
        return -1;
    acquire(&tickslock);
    ticks0 = ticks;
    while (ticks - ticks0 < n) {
        if (myproc()->killed) {
            release(&tickslock);
            return -1;
        }
        sleep(&ticks, &tickslock);
    }
    release(&tickslock);
    return 0;
}

uint64 sys_kill(void) {
    int pid;

    if (argint(0, &pid) < 0)
        return -1;
    return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64 sys_uptime(void) {
    uint xticks;

    acquire(&tickslock);
    xticks = ticks;
    release(&tickslock);
    return xticks;
}

uint64 sys_mmap(void) {
    int length;
    int prot;
    int flags;
    int fd;
    uint64 ret;
    struct proc *p = myproc();
    if (argint(1, &length) < 0 || argint(2, &prot) < 0 ||
        argint(3, &flags) < 0 || argint(4, &fd) < 0) {
        return -1;
    }
    int index = 0;
    for (int i = 0; i < VMANUM; i++) {
        if (p->vma[i].valid == VMAINVALID) {
            index = i;
            break;
        }
    }
    if (!p->ofile[fd]->readable) {
        return -1;
    }
    if (p->ofile[fd]->readable && !p->ofile[fd]->writable &&
        (flags & MAP_SHARED)) {
        return -1;
    }
    p->vma[index].valid = VMAVALID;
    p->vma[index].length = PGROUNDUP(length);
    ret = p->vma[index].addr = p->sz;
    p->sz = p->sz + PGROUNDUP(length);
    p->vma[index].prot = prot;
    p->ofile[fd]->ref += 1;
    p->vma[index].fp = p->ofile[fd];
    p->vma[index].flags = flags;

    return ret;
}

uint64 sys_munmap(void) {
    uint64 addr;
    int length;
    int pages = 0;
    // int written = 0;
    if (argaddr(0, &addr) < 0 || argint(1, &length) < 0) {
        return -1;
    }
    int index;
    struct proc *p = myproc();
    for (int i = 0; i < VMANUM; i++) {
        if (p->vma[i].valid == VMAVALID) {
            printf("i: %d, addr: %p, vaddr: %p, l: %d, vl: %d\n", i, addr,
                   p->vma[i].addr, length, p->vma[i].length);
        }
        if (p->vma[i].valid == VMAVALID && p->vma[i].addr <= addr &&
            p->vma[i].addr + p->vma[i].length > addr &&
            length <= p->vma[i].length) {
            index = i;
            goto found;
        }
    }
    return -1;

found:
    if (addr == p->vma[index].addr && length == p->vma[index].length) {
        // munmap all
        if ((p->vma[index].flags & MAP_SHARED)) {
            filewrite(p->vma[index].fp, addr, length);
        }
        while (pages < PGROUNDUP(length)) {
            uvmunmap(p->pagetable, addr + pages, 1, 1);
            pages += PGSIZE;
        }
        p->vma[index].fp->ref -= 1;
        p->sz -= PGROUNDUP(p->vma[index].length);
        p->vma[index].valid = VMAINVALID;
    } else {
        if ((p->vma[index].flags & MAP_SHARED)) {
            filewrite(p->vma[index].fp, addr, length);
        }
        while (pages < PGROUNDUP(length)) {
            uvmunmap(p->pagetable, addr + pages, 1, 1);
            pages += PGSIZE;
        }
        p->sz -= PGROUNDUP(length);
        p->vma[index].length -= PGROUNDUP(length);
        if (addr == p->vma[index].addr) {
            p->vma[index].addr = p->vma[index].addr + PGROUNDUP(length);
        } else if (addr + PGROUNDUP(length) <=
                   p->vma[index].addr + PGROUNDUP(p->vma[index].length)) {
            panic("HEAD OR TAIL");
        } else {
            // do nothing
        }
    }
    return 0;
}
