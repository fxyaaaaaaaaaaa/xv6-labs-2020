// 用户进程、内核栈、页表页和管道缓冲区的物理内存分配器。
// 分配整个4096字节的页面。

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // 内核后的第一个地址。
// 由kernel.ld定义。

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
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

// 释放由v指向的物理内存页，
// 该页通常应该由kalloc()返回。
// （初始化分配器时除外；参见上面的kinit。）
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // 填充垃圾数据以捕获悬空引用。
  //如果在释放内存后，程序仍然试图访问这块内存，填充的垃圾数据可以帮助检测和调试这种错误。
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  //将内存页加入到空闲链表
  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// 分配一个4096字节的物理内存页。
// 返回内核可以使用的指针。
// 如果内存无法分配，则返回0。
void *
kalloc(void)
{
  // run结构体通常包含一个指向下一块可用内存的指针
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  // 如果获取到的内存块 r 不为空，则用 memset 将其全部填充为数字 5，
  // 当分配内存时，kalloc 函数会将内存页填充为垃圾数据（例如，填充为 5）。这样做的目的是检测未初始化的内存使用。如果程序在初始化内存之前使用它，填充的垃圾数据可以帮助检测和调试这种错误。
  if(r)
    memset((char*)r, 5, PGSIZE); // 填充垃圾数据
  return (void*)r;
}
// 获取空闲内存
void
kama_freebytes(uint64* dst)
{
  *dst = 0;
  struct run* p = kmem.freelist;
  
  acquire(&kmem.lock);

  while (p)
  {
    *dst +=PGSIZE;
    p=p->next;
  }
  release(&kmem.lock);
}
