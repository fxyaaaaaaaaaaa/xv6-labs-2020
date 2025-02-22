// 物理内存布局

// qemu -machine virt 是这样设置的，
// 基于 qemu 的 hw/riscv/virt.c:
//
// 00001000 -- 启动 ROM，由 qemu 提供
// 02000000 -- CLINT
// 0C000000 -- PLIC
// 10000000 -- uart0 
// 10001000 -- virtio 磁盘 
// 80000000 -- 启动 ROM (指read only memory 只读寄存器，一般存储启动代码，比如BIOS或引导加载程序)在机器模式下跳转到这里
//             -内核在这里加载内核
// 80000000 之后的未使用 RAM。

// 内核使用物理内存如下：
// 80000000 -- entry.S，然后是内核文本和数据
// end -- 内核页面分配区域的开始
// PHYSTOP -- 内核使用的 RAM 结束

// qemu 将 UART 寄存器放在物理内存中的这个位置。
#define UART0 0x10000000L
#define UART0_IRQ 10

// virtio mmio 接口
#define VIRTIO0 0x10001000
#define VIRTIO0_IRQ 1

// 本地中断控制器，包含定时器。
#define CLINT 0x2000000L
#define CLINT_MTIMECMP(hartid) (CLINT + 0x4000 + 8*(hartid))
#define CLINT_MTIME (CLINT + 0xBFF8) // cycles since boot.

// qemu 将可编程中断控制器放在这里。
//在xv6内核中，用于映射程序内存的地址范围是[0,PLIC).PLIC =0x0c000000L
#define PLIC 0x0c000000L
#define PLIC_PRIORITY (PLIC + 0x0)
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_MENABLE(hart) (PLIC + 0x2000 + (hart)*0x100)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100)
#define PLIC_MPRIORITY(hart) (PLIC + 0x200000 + (hart)*0x2000)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_MCLAIM(hart) (PLIC + 0x200004 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000)

// 内核期望有 RAM
// 供内核和用户页面使用
// 从物理地址 0x80000000 到 PHYSTOP。
#define KERNBASE 0x80000000L   //2GB
#define PHYSTOP (KERNBASE + 128*1024*1024) //128MB

// 将跳板页面映射到最高地址，
// 在用户空间和内核空间中。
#define TRAMPOLINE (MAXVA - PGSIZE)

// 将内核栈映射到跳板下方，
// 每个栈都被无效的保护页包围。
//TRAMPOLINE是被映射到虚拟地址空间的最高的页面，用于来实现从用户态到内核态的陷阱和终端。
//*2*PGSIZE是因为每个内核栈都要和一个保护页搭配
//输入p是进程的索引，返回的是内核栈的起始地址。
#define KSTACK(p) (TRAMPOLINE - ((p)+1)* 2*PGSIZE)

// 用户内存布局。
// 地址从零开始：
//   文本
//   原始数据和 bss
//   固定大小的栈
//   可扩展的堆
//   ...
//   TRAPFRAME (p->trapframe，由跳板使用)
//   TRAMPOLINE (与内核中的页面相同)
#define TRAPFRAME (TRAMPOLINE - PGSIZE)
