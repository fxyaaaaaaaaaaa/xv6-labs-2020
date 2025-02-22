// Mutual exclusion spin locks.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"

void
initlock(struct spinlock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
}

// 获取锁。
// 循环（自旋）直到获取到锁。
void
acquire(struct spinlock *lk)
{
  if(holding(lk))
  push_off(); // 禁用中断以避免死锁。
    panic("acquire");


  //GCC的内置函数，直接由编译器生成相应的机器指令，无法看到源码。他们在多处理器下确保了操作的原子性和内存访问的顺序，从而实现线程安全的锁机制。
  
  // 在 RISC-V 上，sync_lock_test_and_set 转换为一个原子交换：
  //   a5 = 1
  //   s1 = &lk->locked
  //   amoswap.w.aq a5, a5, (s1)
  //  这是一个原子操作.用于实现自旋锁，他将指定的变量设置位1.并返回旧值，这个操作是原子的，意味着在多核处理环境中不会被打断。
  // 不断循环等待，不断自旋，知道获取到锁。

  
  while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
    ;

  //内存屏障(Memory Barrier)用于控制内存操作的顺序，防止编译器和处理器对内存操作进行重新排序。
  //确保内存访问的顺序符合预期。

  //为什么需要内存屏障？
  //在多处理器系统中，编译器和处理器可能会位了优化性能而对内存操作进行重新排序，这种重新排序可能会导致并发程序中的竞态条件(race condition)和其他同步问题。
  //内存屏障可以可以防止这种重新排序，确保内存操作的顺序符合程序的逻辑要求。
  __sync_synchronize();

  // 记录有关锁获取的信息以供 holding() 和调试使用。
  lk->cpu = mycpu();
}

// Release the lock.
void
release(struct spinlock *lk)
{
  if(!holding(lk))
    panic("release");

  lk->cpu = 0;

  // 告诉 C 编译器和处理器不要移动此点之后的加载或存储，
  // 以确保临界区的所有存储在锁释放之前对其他 CPU 可见，
  // 并且临界区的加载严格发生在锁释放之前。
  // 在 RISC-V 上，这会发出一个 fence 指令。
  __sync_synchronize();

  // 释放锁，相当于 lk->locked = 0。
  // 这段代码不使用 C 赋值，因为 C 标准暗示赋值可能会
  // 使用多个存储指令实现。
  // 在 RISC-V 上，sync_lock_release 转换为一个原子交换：
  //   s1 = &lk->locked
  //   amoswap.w zero, zero, (s1)
  __sync_lock_release(&lk->locked);

  pop_off();
}

// 检查此 CPU 是否持有锁。
// 中断必须关闭。
int
holding(struct spinlock *lk)
{
  int r;
  r = (lk->locked && lk->cpu == mycpu());
  return r;
}
// push_off/pop_off 类似于 intr_off()/intr_on()，只是它们是成对出现的：
// 需要两个 pop_off() 来撤销两个 push_off()。此外，如果中断
// 最初是关闭的，那么 push_off 和 pop_off 会保持它们关闭。

void
push_off(void)
{
  int old = intr_get();
  intr_off();
  if(mycpu()->noff == 0)
    mycpu()->intena = old;
  mycpu()->noff += 1;
}


void
pop_off(void)
{
  struct cpu *c = mycpu();
  if(intr_get())
    panic("pop_off - interruptible");
  if(c->noff < 1)
    panic("pop_off");
  c->noff -= 1;
  if(c->noff == 0 && c->intena)
    intr_on();
}
