// which hart (core) is this?
static inline uint64
r_mhartid()
{
  uint64 x;
  asm volatile("csrr %0, mhartid" : "=r" (x) );
  return x;
}

// Machine Status Register, mstatus

#define MSTATUS_MPP_MASK (3L << 11) // previous mode.
#define MSTATUS_MPP_M (3L << 11)
#define MSTATUS_MPP_S (1L << 11)
#define MSTATUS_MPP_U (0L << 11)
#define MSTATUS_MIE (1L << 3)    // machine-mode interrupt enable.
//这些寄存器是 RISC-V 架构中的硬件寄存器，用于管理和控制处理器的各种状态和操作。
//它们通过特定的汇编指令进行访问，而不是普通的软件变量。通过这些寄存器，处理器可以实现对异常、中断、地址转换等功能的精细控制。
static inline uint64
r_mstatus()
{
  uint64 x;
  asm volatile("csrr %0, mstatus" : "=r" (x) );
  return x;
}

static inline void 
w_mstatus(uint64 x)
{
  asm volatile("csrw mstatus, %0" : : "r" (x));
}

// machine exception program counter, holds the
// instruction address to which a return from
// exception will go.
// 机器异常程序计数器，保存从异常返回的指令地址。
static inline void 
w_mepc(uint64 x)
{
  asm volatile("csrw mepc, %0" : : "r" (x));
}

// Supervisor Status Register, sstatus

#define SSTATUS_SPP (1L << 8)  // 之前的模式，1=Supervisor（管理模式），0=User（用户模式）
#define SSTATUS_SPIE (1L << 5) // 管理模式之前的中断使能
#define SSTATUS_UPIE (1L << 4) // 用户模式之前的中断使能
#define SSTATUS_SIE (1L << 1)  // 管理模式中断使能
#define SSTATUS_UIE (1L << 0)  // 用户模式中断使能
static inline uint64

//sstatus寄存器是 RISC-V架构中的一个控制和状态寄存器，用于管理和报告处理器的状态。sstatus寄存器主要用于管理模式.包含了多个标志位，用于
//控制和报告处理器的各种状态信息。特别是处理陷阱(中断、异常和系统调用)。
r_sstatus()
{
  uint64 x;
  asm volatile("csrr %0, sstatus" : "=r" (x) );
  return x;
}

static inline void 
w_sstatus(uint64 x)
{
  asm volatile("csrw sstatus, %0" : : "r" (x));
}

// Supervisor Interrupt Pending
static inline uint64
r_sip()
{
  uint64 x;
  asm volatile("csrr %0, sip" : "=r" (x) );
  return x;
}

static inline void 
w_sip(uint64 x)
{
  asm volatile("csrw sip, %0" : : "r" (x));
}

// Supervisor Interrupt Enable
#define SIE_SEIE (1L << 9) // external
#define SIE_STIE (1L << 5) // timer
#define SIE_SSIE (1L << 1) // software
static inline uint64
r_sie()
{
  uint64 x;
  asm volatile("csrr %0, sie" : "=r" (x) );
  return x;
}

static inline void 
w_sie(uint64 x)
{
  asm volatile("csrw sie, %0" : : "r" (x));
}

// Machine-mode Interrupt Enable
#define MIE_MEIE (1L << 11) // external
#define MIE_MTIE (1L << 7)  // timer
#define MIE_MSIE (1L << 3)  // software
static inline uint64
r_mie()
{
  uint64 x;
  asm volatile("csrr %0, mie" : "=r" (x) );
  return x;
}

static inline void 
w_mie(uint64 x)
{
  asm volatile("csrw mie, %0" : : "r" (x));
}

// machine exception program counter, holds the
// instruction address to which a return from
// exception will go.

//sepc（Supervisor Exception Program Counter）寄存器用于保存发生异常或中断时的程序计数器（PC）值。
//当异常或中断发生时，CPU 会将当前的 PC 值保存到 sepc 寄存器中，以便在处理完异常或中断后可以恢复执行
static inline void 
w_sepc(uint64 x)
{
  asm volatile("csrw sepc, %0" : : "r" (x));
}

static inline uint64
r_sepc()
{
  uint64 x;
  asm volatile("csrr %0, sepc" : "=r" (x) );
  return x;
}

// Machine Exception Delegation
//机器异常委托寄存器，用于将特定的异常委托个管理模式处理。
static inline uint64
r_medeleg()
{
  uint64 x;
  asm volatile("csrr %0, medeleg" : "=r" (x) );
  return x;
}

static inline void 
w_medeleg(uint64 x)
{
  asm volatile("csrw medeleg, %0" : : "r" (x));
}

// Machine Interrupt Delegation
//机器中断委托寄存器，用于将特定的中断委托给管理模式处理/
static inline uint64
r_mideleg()
{
  uint64 x;
  asm volatile("csrr %0, mideleg" : "=r" (x) );
  return x;
}

static inline void 
w_mideleg(uint64 x)
{
  asm volatile("csrw mideleg, %0" : : "r" (x));
}

// Supervisor Trap-Vector 寄存器
//记录了trampoline.S中的uservec的地址，当内核态返回到用户态时，系统调用、中断和异常可以正确跳转到trampoline.S中的uservec处理程序。保存陷阱处理程序的基地址。
static inline void 
w_stvec(uint64 x)
{
  asm volatile("csrw stvec, %0" : : "r" (x));
}

static inline uint64
r_stvec()
{
  uint64 x;
  asm volatile("csrr %0, stvec" : "=r" (x) );
  return x;
}

// Machine-mode interrupt vector
//机器模式中断向量寄存器，保存机器模式中断处理程序的基地址。
static inline void 
w_mtvec(uint64 x)
{
  asm volatile("csrw mtvec, %0" : : "r" (x));
}

// use riscv's sv39 page table scheme.
#define SATP_SV39 (8L << 60)

#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12))

// supervisor address translation and protection;
// holds the address of the page table.
//管理模式地址转换和保护寄存器，保存页表的基地址。
static inline void 
w_satp(uint64 x)
{
  asm volatile("csrw satp, %0" : : "r" (x));
}

//记录页表
static inline uint64
r_satp()
{
  uint64 x;
  asm volatile("csrr %0, satp" : "=r" (x) );
  return x;
}

// Supervisor Scratch register, for early trap handler in trampoline.S.
//管理模式暂存寄存器，用于保存临时值，通常在陷阱处理程序中使用。
static inline void 
w_sscratch(uint64 x)
{
  asm volatile("csrw sscratch, %0" : : "r" (x));
}

//机器模式暂存寄存器，用于保存临时值，通常在陷阱处理程序中使用。
static inline void 
w_mscratch(uint64 x)
{
  asm volatile("csrw mscratch, %0" : : "r" (x));
}

// Supervisor Trap Cause
//管理模式陷阱原因寄存器，保存导致陷阱的原因。
static inline uint64
r_scause()
{
  uint64 x;
  asm volatile("csrr %0, scause" : "=r" (x) );
  return x;
}

// Supervisor Trap Value
//管理模式陷阱值寄存器，保存导致陷阱的相关值(如地址)。
static inline uint64
r_stval()
{
  uint64 x;
  asm volatile("csrr %0, stval" : "=r" (x) );
  return x;
}

// Machine-mode Counter-Enable
// 机器模式计数器使能寄存器，控制哪些性能计数器在管理模式和用户模式可用。
static inline void 
w_mcounteren(uint64 x)
{
  asm volatile("csrw mcounteren, %0" : : "r" (x));
}

static inline uint64
r_mcounteren()
{
  uint64 x;
  asm volatile("csrr %0, mcounteren" : "=r" (x) );
  return x;
}

// machine-mode cycle counter
//机器模式时间寄存器，保存当前的时间计数值。
static inline uint64
r_time()
{
  uint64 x;
  asm volatile("csrr %0, time" : "=r" (x) );
  return x;
}

// enable device interrupts
static inline void
intr_on()
{
  w_sstatus(r_sstatus() | SSTATUS_SIE);
}

// disable device interrupts
static inline void
intr_off()
{
  w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

// are device interrupts enabled?
static inline int
intr_get()
{
  uint64 x = r_sstatus();
  return (x & SSTATUS_SIE) != 0;
}
//栈指针寄存器，保存当前栈的地址。
static inline uint64
r_sp()
{
  uint64 x;
  asm volatile("mv %0, sp" : "=r" (x) );
  return x;
}

// 读取和写入 tp（线程指针），它保存
// 该核心的 hartid（核心编号），即 cpus[] 的索引。
//线程指针寄存器。保存当前硬件线程ID
static inline uint64
r_tp()
{
  uint64 x;
  //内联汇编指令。volatile关键字告诉编译器不应该优化这段代码。
  //"mv %0, tp"  RISC-V汇编指令，将tp寄存器的值移动到输出操作数%0中。
  //:"=r"(x)：输出操作数，=r表示将结果存储在一个通用寄存器中，并将其赋值给变量x
  asm volatile("mv %0, tp" : "=r" (x) );
  return x;
}

static inline void 
w_tp(uint64 x)
{
  asm volatile("mv tp, %0" : : "r" (x));
}
//返回地址寄存器，保存函数调用返回地址。
static inline uint64
r_ra()
{
  uint64 x;
  asm volatile("mv %0, ra" : "=r" (x) );
  return x;
}

// flush the TLB.
static inline void
sfence_vma()
{
  // the zero, zero means flush all TLB entries.
  asm volatile("sfence.vma zero, zero");
}


#define PGSIZE 4096 // 一页大小
#define PGSHIFT 12  // 页内偏移的位数

//将输入的sz地址转换到到下一个页面的0号偏移 ---->向上对齐
#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
//将a地址转换到当前页面的0号偏移  ---->向下对齐
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))

//PTE中的flags，PTE是由PPN和flags组成的
//1L表示一个长整型常量 1 ，L后缀用于指定该常量为长整型,并确保位移操作后的结果是一个长整型值。
//这些标志位组合在一起，用于控制页表项的各种属性和权限。
#define PTE_V (1L << 0) // valid
#define PTE_R (1L << 1) // 1 -> 可读
#define PTE_W (1L << 2) // 1 -> 可写
#define PTE_X (1L << 3) // 1 -> 可执行
#define PTE_U (1L << 4) // 1 -> 用户模式的访问权限位

//将物理地址变为一个PTE页表项。
//将物理地址右移12位，去掉低12位的offset，然后左移10位，空出PTE中的flags，进行保留了PPN
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)

//从PTE中提取物理地址 >>10是去掉低10位的flags,然后左移12位是为了将物理地址中的低12位空出来，代表偏移量
//用于获取下一级页表的物理地址
#define PTE2PA(pte) (((pte) >> 10) << 12)
//从pte中提取标志位
//0x3FF是二进制1111111111
#define PTE_FLAGS(pte) ((pte) & 0x3FF)

// 从虚拟地址中提取三个9位的页表索引。
// 9位的掩码二进制为111111111 用于提取虚拟地址中的9位索引
#define PXMASK          0x1FF 
//计算给定级别的的页表索引在虚拟地址中的位移量,level为页表级别(0,1或2)
#define PXSHIFT(level)  (PGSHIFT+(9*(level))) 
//PX用于从虚拟地址va中提取指定级别的页表索引
//va >> 将右侧目前不用的位数晒出去，然后&PXMASK使用掩码留下9位，最后得到9位的页表索引
#define PX(level, va) ((((uint64) (va)) >> PXSHIFT(level)) & PXMASK)

// 超过最高可能的虚拟地址的一个。
// MAXVA 实际上比 Sv39 允许的最大值少一位，以避免必须对高位设置的虚拟地址进行符号扩展。
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))

typedef uint64 pte_t;
typedef uint64 *pagetable_t; // 512 个 PTE
