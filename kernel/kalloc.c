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
} kmem[NCPU];
char* kmem_lock_names[]=
{
    "kmem_cpu_0",
    "kmem_cpu_1",
    "kmem_cpu_2",
    "kmem_cpu_3",
    "kmem_cpu_4",
    "kmem_cpu_5",
    "kmem_cpu_6",
    "kmem_cpu_7",
};
void
kinit()
{
  for(int i=0;i<NCPU;i++)
  {
      initlock(&kmem[i].lock,kmem_lock_names[i]);
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
  r = (struct run*)pa;
  push_off();
  int cpu = cpuid();  //获取cpu编号。中断关闭时调用cpuid才是安全的，所以上面push_off
  acquire(&kmem[cpu].lock);
  r->next = kmem[cpu].freelist;
  kmem[cpu].freelist = r;
  release(&kmem[cpu].lock);
  pop_off();      //重新打开中断
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.


//下面这段代码有死锁的可能性 但是还没有找到解决的办法
//死锁原因：当A线程持有A锁 想要持有B锁， 当B线程持有B锁 想要持有A锁，会发生死锁。
void *
kalloc(void)
{
  struct run * r = 0;
  push_off();
  int cid = cpuid();
  acquire(&kmem[cid].lock);
  if(!kmem[cid].freelist)
  {
      int steal_page = 64;
      for(int i=0;i<NCPU;i++)
      {
          if(i == cid)
              continue;
          acquire(&kmem[i].lock);
          if(kmem[i].freelist == 0)
          {
              release(&kmem[i].lock);
              continue;
          }
          while(steal_page && kmem[i].freelist)
          {
            r = kmem[i].freelist;
            kmem[i].freelist = r->next;
            r->next = kmem[cid].freelist;
            kmem[cid].freelist = r;
            steal_page--;
          }
          release(&kmem[i].lock);
          if(steal_page == 0)
              break;
      }
  }
  r = kmem[cid].freelist;
  if(r)
    kmem[cid].freelist = r->next;
  release(&kmem[cid].lock);
  pop_off();                //打开中断

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return r;
}
