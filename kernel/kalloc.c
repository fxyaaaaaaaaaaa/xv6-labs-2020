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
#define PA2PGREF_ID(p)  (((p)-KERNBASE)/PGSIZE)   //由物理地址获取物理页id
#define PRGEF_MAX_ENTRIES PA2PGREF_ID(PHYSTOP)    //物理页上限

int pageref[PRGEF_MAX_ENTRIES];                   //每个物理页的引用数，数组pageref[i]表示第i个物理页的引用数
struct spinlock pgreflock;                        //用于pageref数组的锁，防止竞态条件引起的内存泄漏

#define PA2PGREF(p) pageref[PA2PGREF_ID((uint64)(p))]   //获取地址对应物理页的引用数
void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&pgreflock, "pgref");
  freerange(end, (void*)PHYSTOP);
}
//写时复制一个新的物理地址返回
//如果该物理页的引用数 >1 则将引用数-1 并分配一个新的物理页返回
//如果引用数<=1 .则无需操作直接返回该物理页
void* kama_kcopy_n_deref(void* pa)
{
    acquire(&pgreflock);
    
    //当前物理页的引用数为1，无需分配新的物理页
    if(PA2PGREF(pa) <= 1)
    {
        release(&pgreflock);
        return pa;
    }

    //分配新的物理页，并把旧页中的数据复制到新页
    uint64 newpa = (uint64)kalloc();
    if(newpa == 0)
    {
        release(&pgreflock);
        return 0;
    }
    memmove((void*)newpa,(void*)pa,PGSIZE);

    //旧页引用数-1
    PA2PGREF(pa)--;
    
    release(&pgreflock);
    return (void*)newpa;
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

  //当页面的引用数<=0时才释放页面
  acquire(&pgreflock);
  if(--PA2PGREF(pa) <= 0)
  {
     // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
  release(&pgreflock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
  {
    memset((char*)r,5,PGSIZE); // fill with junk
    PA2PGREF(r) = 1;    //将新分配的物理页的引用计数置为1
  } 
  return (void*)r;
}
//物理页引用数+1
void kama_krefpage(void* pa)
{
  acquire(&pgreflock);
  PA2PGREF(pa)++;
  release(&pgreflock);
}