#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

//trampoline 是一个标签，指向 trampoline.S 文件的起始地址，trampoline区域用于在内核态返回用户态进行页表切换和寄存器恢复。
//uservec是一个标签，指向trampoline.S 文件中处理用户态陷阱入口地址。处理从用户态进入内核态的中断和异常。他保存用户态寄存器，并跳转到kerneltrap 进行进一步处理。
//userret 是一个标签，指向 trampoline.S 文件中处理从内核态返回到用户态的函数入口地址。userret 负责恢复用户寄存器状态，并使用 sret 指令返回到用户模式。 
extern char trampoline[], uservec[], userret[];

// 在 KernelVec.S，调用 kerneltrap（）.
void kernelvec();//指向中断和异常处理的入口地址，进行保存寄存器状态，调用中断处理函数kerneltrap，中断完成恢复寄存器状态并返回。


extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// 设置为在内核中获取异常和陷阱.
// stvec 寄存器设置为 kernelvec 的地址，这意味着当发生中断或异常时，CPU 将跳转到 kernelvec 处执行。
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// 处理来自用户空间的中断、异常或系统调用。
// called from trampoline.S
//
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // 将中断和异常发送到 kerneltrap(),
  // 因为我们现在在内核中.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
    
  // save user program counter.
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call

    if(p->killed)
      exit(-1);

    //sepc 指向 eCall 指令,
    //但我们想返回到下一条指令.
    p->trapframe->epc += 4;

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
  } else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }

  if(p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
    yield();

  usertrapret();
}

//
// return to user space
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // 我们即将将 trap 的目的地从kerneltrap()设置为usertrap(),切换的过程要关闭中断。
  // 直到我们回到了用户空间，其中 usertrap（） 是正确的。
  //这段代码的作用是确保在从内核态返回到用户态的过程中，中断是关闭的，以避免在切换过程中发生中断，导致系统状态不一致或出现其他问题
  intr_off();

  //这行代码的作用是将 stvec 寄存器设置为 uservec 的地址，以便在从内核态返回到用户态时，系统调用、中断和异常能够正确地跳转到 trampoline.S 中的 uservec 处理程序。
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // 设置 trapframe 的值，当进程下次重新进入内核时，uservec 将需要这些值。
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // 设置 trampoline.S 的 sret 将使用的寄存器
  // 以进入用户空间。
  
  // 设置 S 上一个特权模式为用户模式。
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // 具体来说，它将当前进程的 trapframe 结构体中的 epc 值写入 sepc 寄存器。
  //  这样，当从内核返回到用户空间时，CPU 将从 epc 保存的地址继续执行用户程序.通过这种方式，操作系统能够正确地恢复用户程序的执行状态。
  //RISC-V 架构中，sret 指令用于从管理模式返回到用户模式。sret 指令会从 sepc 寄存器中读取返回地址，并跳转到该地址继续执行
  w_sepc(p->trapframe->epc);

  // MAKE_SATP宏用于生成satp寄存器的值，satp寄存器设置页表基地址。
  uint64 satp = MAKE_SATP(p->pagetable);

  // 跳转到内存顶部的 trampoline.S，
  // 它切换到用户页表，恢复用户寄存器，
  // 并使用 sret 切换到用户模式。
  //TRAMPOLINE是trampoline.S文件在内存中的基地址。
  //userret:trampoline.S 文件中处理从内核态返回到用户态的函数入口地址。
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64,uint64))fn)(TRAPFRAME, satp);
}

// 内核代码中的中断和异常通过 kernelvec 发送到此处,
// 在当前内核堆栈上.
// 用于处理在内核态发生中断和异常时，首先保存当前状态，然后检查中断或异常是否从管理模式进入，并确保中断未启用。
//接着调用devintr函数处理设备中断。如果定时器中断且当前进程处于运行状态，则调用yield函数让出CPU。
//最后恢复之前状态以继续执行被中断的代码。
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// 检查是外部中断还是软件中断，并处理它。
// 如果是定时器中断，返回 2；
// 如果是其他设备中断，返回 1；
// 如果未识别，返回 0。
int
devintr()
{
  uint64 scause = r_scause();

  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // 这是一个通过 PLIC 的主管外部中断。

    // irq 表示哪个设备中断了。
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // PLIC 允许每个设备一次最多触发一个中断；
    // 告诉 PLIC 该设备现在可以再次触发中断。
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000001L){
    // 来自机器模式定时器中断的软件中断，
    // 由 kernelvec.S 中的 timervec 转发。

    if(cpuid() == 0){
      clockintr();
    }
    
    // 通过清除来确认软件中断
    // sip 中的 SSIP 位
    w_sip(r_sip() & ~2);

    return 2;
  } else {
    return 0;
  }
}

