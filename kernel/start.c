#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

void main();
void timerinit();

// entry.S needs one stack per CPU.
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

// scratch area for timer interrupt, one per CPU.
uint64 mscratch0[NCPU * 32];

// assembly code in kernelvec.S for machine-mode timer interrupt.
extern void timervec();

// entry.S 在 machine mode 下跳转到 stack0。
void
start()
{
  // 设置 M Previous Privilege mode 为 Supervisor，以便 mret 使用。
  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);

  // 设置 M Exception Program Counter 为 main，以便 mret 使用。
  // 需要 gcc -mcmodel=medany
  w_mepc((uint64)main);

  // 现在禁用分页。
  w_satp(0);

  // 将所有中断和异常委托给 supervisor mode。
  w_medeleg(0xffff);
  w_mideleg(0xffff);
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

  // 请求时钟中断。
  timerinit();

  // 将每个 CPU 的 hartid 保存在其 tp 寄存器中，以便 cpuid() 使用。
  int id = r_mhartid();
  w_tp(id);

  // 切换到 supervisor mode 并跳转到 main()。
  asm volatile("mret");
}

//设置为在机器模式下接收定时器中断，
//Xv6 将机器模式的中断向量（mtvec）设置为kernelvec中的timervec，这是一个汇编代码实现的定时器中断处理程序。
//这将它们转化为软件中断
//在陷阱中破坏（）。
void
timerinit()
{
  // 每个CPU都有单独的定时器中断源。
  int id = r_mhartid();

  // 向CLINT请求定时器中断。
  int interval = 1000000; // 周期; qemu约1/10秒
  *(uint64*)CLINT_MTIMECMP(id) = *(uint64*)CLINT_MTIME + interval;

  //设置一个scratch区域，用于保存定时器中断处理程序需要的寄存器和CLINT寄存器的地址。
  // scratch[0..3] : space for timervec to save registers.
  // scratch[4] : address of CLINT MTIMECMP register.
  // scratch[5] : desired interval (in cycles) between timer interrupts.
  uint64 *scratch = &mscratch0[32 * id];
  scratch[4] = CLINT_MTIMECMP(id);
  scratch[5] = interval;
  w_mscratch((uint64)scratch);

  // 设置机器模式陷阱处理程序。
  w_mtvec((uint64)timervec);

  // 启用机器模式中断。
  w_mstatus(r_mstatus() | MSTATUS_MIE);

  // 启用机器模式定时器中断。
  w_mie(r_mie() | MIE_MTIE);
}
