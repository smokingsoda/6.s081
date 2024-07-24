#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
uint64
sys_pgaccess(void)
{
  // Parse arguments
  pagetable_t pagetable = myproc()->pagetable;
  pagetable_t proc_pagetable = pagetable;
  uint64 return_bit = 0;
  uint64 va;
  int length;
  uint64 mask_addr;
  if (argaddr(0, &va) < 0 || argaddr(2, &mask_addr) < 0 || argint(1, &length) || length > 64) {
      return -1;
  }

  // Walk the pagetable to check the PTE_A bit
  // First I need to convert the va to pa which is pointing to the first page
  // in the physical memory
  for (int level = 2; level > 0; level --) {
      pte_t *pte = &pagetable[PX(level, va)];
      if (*pte & PTE_V) {
          pagetable = (pagetable_t) PTE2PA(*pte);
      } else {
          return -1;
      }
  }

  // Check whether the iteration won't touch the top of the level 0 pagetable
  int current = PX(0, va);
  if (current + length > 512) {
    return -1;
  }

  // Now pagetable is the level 0 pagetable, and we need to get 
  // the starting pte according to the va
  // Now we want to do iteration
  for (int i = 0; i < length; i++) {
      if (pagetable[current + i] & PTE_A) {
          //printf("page %p is accessed, then set the bit to zero\n", pagetable[current + i]);
          return_bit = return_bit | (1 << i);
          // Reset the PTE_A
          pagetable[current + i] = pagetable[current + i] ^ PTE_A;
      }
  }
  //printf("now return bit is %p\n", return_bit);
  // Now copyout the returnbit
  if (copyout(proc_pagetable, mask_addr, (char *) &return_bit, sizeof(uint64)) < 0){
    return -1;
  }
  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
