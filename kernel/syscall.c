#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"

// 从当前进程中获取addr处的uint64。
int
fetchaddr(uint64 addr, uint64 *ip)
{
  struct proc *p = myproc();
  if(addr >= p->sz || addr+sizeof(uint64) > p->sz)
    return -1;
  if(copyin(p->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
    return -1;
  return 0;
}

// 从当前进程中获取addr处的以nul结尾的字符串。
// 返回字符串长度，不包括nul，或错误时返回-1。
int
fetchstr(uint64 addr, char *buf, int max)
{
  struct proc *p = myproc();
  int err = copyinstr(p->pagetable, buf, addr, max);
  if(err < 0)
    return err;
  return strlen(buf);
}

static uint64
argraw(int n)
{
  struct proc *p = myproc();

  //从当前进程的trapframe中获取系统调用参数
  //a0-a5是系统调用参数代表第0到第5个参数，a7是系统调用号
  switch (n) {
  case 0:
    return p->trapframe->a0;
  case 1:
    return p->trapframe->a1;
  case 2:
    return p->trapframe->a2;
  case 3:
    return p->trapframe->a3;
  case 4:
    return p->trapframe->a4;
  case 5:
    return p->trapframe->a5;
  }
  panic("argraw");
  return -1;
}

// 获取第n个32位系统调用参数。从当前进程的陷阱帧中获取。
int
argint(int n, int *ip)
{
  *ip = argraw(n);
  return 0;
}

// 将参数作为指针检索。
// 不检查合法性，因为copyin/copyout会进行检查。
int
argaddr(int n, uint64 *ip)
{
  *ip = argraw(n);
  return 0;
}

// 将第n个字大小的系统调用参数作为以null结尾的字符串获取。
// 复制到buf中，最多max个字符。
// 如果成功返回字符串长度（包括nul），错误返回-1。
int
argstr(int n, char *buf, int max)
{
  uint64 addr;
  if(argaddr(n, &addr) < 0)
    return -1;
  return fetchstr(addr, buf, max);
}

extern uint64 sys_chdir(void);
extern uint64 sys_close(void);
extern uint64 sys_dup(void);
extern uint64 sys_exec(void);
extern uint64 sys_exit(void);
extern uint64 sys_fork(void);
extern uint64 sys_fstat(void);
extern uint64 sys_getpid(void);
extern uint64 sys_kill(void);
extern uint64 sys_link(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_mknod(void);
extern uint64 sys_open(void);
extern uint64 sys_pipe(void);
extern uint64 sys_read(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_sleep(void);
extern uint64 sys_unlink(void);
extern uint64 sys_wait(void);
extern uint64 sys_write(void);
extern uint64 sys_uptime(void);
extern uint64 sys_trace(void);  // 全局声明trace系统调用处理函数
extern uint64 sys_sysinfo(void); // 全局声明sysinfo系统调用函数

// 函数指针数组，里面存放系统调用号和对应的处理函数
static uint64 (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
};

// 定义系统调用名称的字符串数组
const char* kama_syscall_names[]=
{
  [SYS_fork]    "fork",
  [SYS_exit]    "exit",
  [SYS_wait]    "wait",
  [SYS_pipe]    "pipe",
  [SYS_read]    "read",
  [SYS_kill]    "kill",
  [SYS_exec]    "exec",
  [SYS_fstat]   "fstat",
  [SYS_chdir]   "chdir",
  [SYS_dup]     "dup",
  [SYS_getpid]  "getpid",
  [SYS_sbrk]    "sbrk",
  [SYS_sleep]   "sleep",
  [SYS_uptime]  "uptime",
  [SYS_open]    "open",
  [SYS_write]   "write",
  [SYS_mknod]   "mknod",
  [SYS_unlink]  "unlink",
  [SYS_link]    "link",
  [SYS_mkdir]   "mkdir",
  [SYS_close]   "close",
};

// 所有的系统调用都会在syscall()函数进行处理。因此可以在这里打印跟踪信息。
void
syscall(void)
{
  int num;
  struct proc *p = myproc();
  // 获取系统调用号
  num = p->trapframe->a7;
  // 如果系统调用号有效（大于0且小于syscalls数组的长度，并且对应的处理函数存在）
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    // 调用对应的处理函数，并将返回值存储在a0寄存器中
    p->trapframe->a0 = syscalls[num]();
    // 如果当前进程启动了trace跟踪，则打印信息
    // 这里将kama_syscall_trace >>num &1是为了判断当前进程是否跟踪系统调用号为num的系统调用
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
