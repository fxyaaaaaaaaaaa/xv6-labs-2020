#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"

/*
页表 就像是一个地址簿，记录了每栋别墅的房间（虚拟地址）对应到大房子里的哪个房间（物理地址）。
kvminit() 和 uvmcreate() 是初始化地址簿的函数。
kvmmap() 和 mappages() 是往地址簿里添加新记录的函数。
walk() 和 walkaddr() 是查找地址簿的函数，告诉你某个虚拟地址对应的物理地址。
uvmalloc() 和 uvmdealloc() 是分配和释放房间的函数。
uvmcopy() 是复制地址簿和房间内容的函数，用于创建新的别墅（子进程）。
copyout() 和 copyin() 是在大房子和别墅之间搬运东西的函数。


这些函数共同实现了一个 虚拟内存管理系统，核心功能包括：
虚拟地址到物理地址的映射：通过页表管理虚拟地址和物理地址的对应关系。
内存的分配和释放：为用户进程分配和释放物理内存。
内存的复制和共享：支持进程间的内存复制（如 fork）和内核与用户空间的数据交换。
权限管理：控制用户进程对内存的访问权限。
*/


/*
 * 内核的页表。
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld 将其设置为内核代码段的结束地址。

extern char trampoline[]; // trampoline.S

/*
 * 为内核创建一个直接映射的页表。
 初始化内核的页表，将内核的代码、数据、设备寄存器等映射到虚拟地址空间
 */
//kvm ---> kernel virtual memory
//kama----> kernel address memory allocation
pagetable_t
kama_kvminit_newpgtbl()
{
    pagetable_t pgtbl = (pagetable_t)kalloc();
    memset(pgtbl, 0, PGSIZE);
    kama_kvm_map_pagetable(pgtbl);
    return pgtbl;
}

void 
kama_kvm_map_pagetable(pagetable_t pgtbl)
{
  //将各种内核需要的direct mapping 添加到页表pgtbl中
  // uart register
  kvmmap(pgtbl,UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio 磁盘接口
  kvmmap(pgtbl,VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // CLINT
  //kvmmap(pgtbl,CLINT, CLINT, 0x10000, PTE_R | PTE_W);

  // PLIC
  kvmmap(pgtbl,PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // 映射内核文本为可执行和只读。
  kvmmap(pgtbl,KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // 映射内核数据和我们将使用的物理 RAM。
  kvmmap(pgtbl,(uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // 映射用于陷阱进入/退出的跳板到内核中的最高虚拟地址。
  kvmmap(pgtbl,TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
}
void
kvminit()
{
  kernel_pagetable =  kama_kvminit_newpgtbl();
  kvmmap(kernel_pagetable,CLINT,CLINT,0x10000,PTE_R|PTE_W);
}

// 将硬件页表寄存器切换到内核的页表，并启用分页。
//启用分页的意思是启动虚拟内存管理机制。通过设置satp寄存器，CPU将开始使用页表进行地址转换，
//将虚拟地址转换为物理地址。
void
kvminithart()
{
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}

// 返回页表 pagetable 中对应于虚拟地址 va 的 PTE 的地址。
// 如果 alloc!=0，则创建任何所需的页表页。
//
// RISC-V Sv39 方案有三级页表页。
// 一个页表页包含 512 个 64 位 PTE。
// 一个 64 位虚拟地址被分成五个字段：
//   39..63 -- 必须为零。
//   30..38 -- 9 位的二级索引。
//   21..29 -- 9 位的一级索引。
//   12..20 -- 9 位的零级索引。
//    0..11 -- 页内字节偏移的 12 位。

// 查找虚拟地址对应的页表项（PTE），如果页表项不存在且 alloc 参数为1，则创建新的页表项。如果alloc参数为0，即使页表项不存在也不创建新页表。
//拿到一个va一个找到第三个PTE(xv6使用的是三级页表)的地址
//walk一般的使用方式为:struct proc * p =myproc();
// pa =  PTE2PA(*walk(p->pagetable,va,0))  walk得到pte使用宏函数PTE2PA得到物理地址

pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}




int kama_pgtblprint(pagetable_t pagetable , int depth)
{ 
   for(int i=0;i<512;i++)
   {
      pte_t pte = pagetable[i];
      if((pte & PTE_V))
      {
        printf("..");
        int n=depth;
        while(n--)
          printf( " ..");
        printf("%d: pte %p pa %p\n",i,pte,(pagetable_t)PTE2PA(pte));
        if( (pte & (PTE_R|PTE_W|PTE_X)) == 0)
        {
          kama_pgtblprint((pagetable_t)PTE2PA(pte),depth + 1);
        }
      }
   }
   return 0;
}

//打印页表
int kama_vmprint(pagetable_t pagetable)
{
  printf("page table %p\n",pagetable);
  return kama_pgtblprint(pagetable,0);
}

// 查找虚拟地址，返回物理地址，
// 如果未映射则返回 0。
// 只能用于查找用户页。

// 从页表中查找虚拟地址对应的物理地址 未映射则返回 0
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}

// 向内核页表添加映射。
// 仅在引导时使用。
// 不刷新 TLB 或启用分页。
//在内核页表中添加一个映射，将虚拟地址（va）映射到物理地址（pa）
void
kvmmap(pagetable_t pagetable,uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(pagetable, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// 将内核虚拟地址转换为物理地址。
// 仅用于堆栈上的地址。
// 假设 va 是页对齐的。

//将内核虚拟地址转换为物理地址，仅用于内核栈上的地址
uint64
kvmpa(pagetable_t pagetable,uint64 va)
{
  uint64 off = va % PGSIZE; //获取页内偏移量
  pte_t *pte;
  uint64 pa;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("kvmpa");
  if((*pte & PTE_V) == 0)
    panic("kvmpa");
  pa = PTE2PA(*pte);
  return pa+off;
}

// 为从 va 开始的虚拟地址创建 PTE，指向从 pa 开始的物理地址。
// va 和 size 可能不是页对齐的。成功返回 0，如果 walk() 无法分配所需的页表页则返回 -1。
//在页表中添加多个页的映射，支持将一段虚拟地址空间映射到一段物理地址空间
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  //虚拟地址的起点和终点
  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);

    
  for(;;){
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_V)
      panic("remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// 从 va 开始移除 npages 的映射。va 必须是页对齐的。映射必须存在。
// 可选地释放物理内存。
//从页表中移除虚拟地址空间的映射，并可选地释放物理内存
//在某些情况下，可能需要暂时保留物理内存页，而不立即释放。例如，在某些内存管理策略中，可能会延迟释放物理内存页，
//以便在将来重新使用这些内存页。操作系统需要有额外的机制来跟踪和回收这些物理内存页，以避免内存泄漏。
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    if((*pte & PTE_V) == 0)
      panic("uvmunmap: not mapped");
    if(PTE_FLAGS(*pte) == PTE_V) //用于判断是否是非叶子节点，如果非叶子节点则标志位里只有PTE_v
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}

// 创建一个空的用户页表。
// 如果内存不足则返回 0。

pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// 将用户 initcode 加载到 pagetable 的地址 0，
// 用于第一个进程。
// sz 必须小于一页。
void
uvminit(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;
  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U);
  printf("222\n");
  memmove(mem, src, sz);
}

// 分配 PTE 和物理内存以将进程从 oldsz 增长到 newsz，
// 这不需要页对齐。成功返回新大小，错误返回 0。

//为用户进程分配新的物理内存，并将其映射到虚拟地址空间。
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// 释放用户页以将进程大小从 oldsz 减少到 newsz。
// oldsz 和 newsz 不需要页对齐，newsz 也不需要小于 oldsz。
// oldsz 可以大于实际进程大小。返回新进程大小。
//释放用户进程的物理内存，并取消虚拟地址空间的映射
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// 递归释放页表页。
// 所有叶子映射必须已被移除。
//递归释放页表页，用于销毁整个页表

//通过判断PTE_V来看页表项是否有效
//然后通过PTE_R PTE_W PTE_X 是否有效来判断当前PTE是否是叶子节点。也就是是否是指向物理内存的PTE
//因为在三级页表中，只有最后一级指向物理内存的叶子节点的PTE中的PTE_R PTE_W PTE_X才会被设置。

/*在递归释放页表的过程中，所有叶子节点应该都已经被移除了。这是因为在操作系统的内存管理中，页表的释放通常分为两个步骤：
1.解除映射并释放物理内存。这是用uvmunmap函数实现的。他会遍历页表，找到所有有效的叶子节点，接触映射释放物理内存。
2.递归释放页表，也就是下面这个freewalk函数实现的。

*/

void
freewalk(pagetable_t pagetable)
{
  // 页表中有 2^9 = 512 个 PTE。
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // 这个 PTE 指向一个低级页表。child是下一级页表的物理地址
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}
//递归释放一个内核页表中所有的映射，但是不释放其指向的物理页
void kama_kvm_free_kernelpgtbl(pagetable_t pagetable)
{
  for(int i =0;i<512;i++)
  {
      pte_t pte = pagetable[i];
      uint64 child = PTE2PA(pte);
      if((pte & PTE_V) && (pte & (PTE_R |PTE_W |PTE_X)) == 0)
      {
        kama_kvm_free_kernelpgtbl((pagetable_t)child);
        pagetable[i] = 0;
      }
  }
  kfree((void*)pagetable);  //释放当前级别页表所占内存
}

//复制页表的函数
int 
kama_kvmcopymappings(pagetable_t src,pagetable_t dst,uint64 start,uint64 sz)
{
  pte_t *pte;
  uint64 pa,i;
  uint flags;
  //PGROUNDUP：将地址向上取整到页边界，防止重新映射已经映射的页面，特别是在执行growproc操作时
  for(i =PGROUNDUP(start);i<start + sz;i +=PGSIZE)
  {
      if((pte = walk(src,i,0)) == 0)
        panic("kvmcopymappings: pte should exist");
       if((*pte & PTE_V) == 0)
        panic("kvmcopymappings: page not present");
      pa = PTE2PA(*pte);


      // '& ~PTE_U'表示该页的权限是这为非用户页
      // 必须设置该权限，因为RISC-V中内核时无法直接访问用户页的
      flags = PTE_FLAGS(*pte) & ~PTE_U;
      if(mappages(dst,i,PGSIZE,pa,flags) != 0)
        goto err;
  }
  return 0;
err:
  //解除目标页表中已经映射的页表项
  uvmunmap(dst,PGROUNDUP(start),(i - PGROUNDUP(start))/PGSIZE,0);
  return -1;
}
//缩减内存的函数.用于内核页表和用户页表内存映射同步:
//与 uvmdealloc 功能类似，将程序内存从oldsz缩减到newsz，但不释放实际的内存
uint64 kama_kvmdealloc(pagetable_t pagetable,uint64 oldsz,uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;
  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz))
  {
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz))/PGSIZE;
    uvmunmap(pagetable,PGROUNDUP(newsz),npages,0);
  }
  return newsz;
}
// 释放用户内存页，然后释放页表页。
//释放用户进程的页表和物理内存
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}

// 给定父进程的页表，将其内存复制到子进程的页表中。
// 复制页表和物理内存。
// 成功返回 0，失败返回 -1。
// 失败时释放任何已分配的页。

//将父进程的页表和物理内存复制到子进程的页表中，用于实现 fork 系统调用
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}


// 由 exec 用于用户堆栈保护页。
//权限管理 将页表项标记为无效，禁止用户访问，用于保护用户堆栈
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// 从内核复制到用户。
// 从 src 复制 len 字节到给定页表中的虚拟地址 dstva。
// 成功返回 0，错误返回 -1。
// 从内核复制数据到用户空间的虚拟地址
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// 从用户复制到内核。
// 从给定页表中的虚拟地址 srcva 复制 len 字节到 dst。
// 成功返回 0，错误返回 -1。
//从用户空间的虚拟地址复制数据到内核
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  return copyin_new(pagetable,dst,srcva,len);
}

// 从用户复制一个以 null 结尾的字符串到内核。
// 从给定页表中的虚拟地址 srcva 复制字节到 dst，直到 '\0' 或 max。
// 成功返回 0，错误返回 -1。
//从用户空间的虚拟地址复制字符串到内核.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  return copyinstr_new(pagetable, dst, srcva, max);
}
