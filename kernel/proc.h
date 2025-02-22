// 内核上下文切换保存的寄存器。
struct context {
  uint64 ra;
  uint64 sp;

  // 被调用者保存的寄存器
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

// 每个 CPU 的状态。
struct cpu {
  struct proc *proc;          // 在此 CPU 上运行的进程，或为空。
  struct context context;     // swtch() 到这里进入调度程序。
  int noff;                   // push_off() 嵌套的深度。
  int intena;                 // 在 push_off() 之前中断是否已启用？
};

extern struct cpu cpus[NCPU];

// trap 处理代码在 trampoline.S 中的每个进程的数据。
// 位于用户页表中 trampoline 页下方的一个页面中。
// sscratch 寄存器指向这里。
// trampoline.S 中的 uservec 保存用户寄存器到 trapframe 中，
// 然后从 trapframe 的 kernel_sp、kernel_hartid、kernel_satp 初始化寄存器，并跳转到 kernel_trap。
// trampoline.S 中的 usertrapret() 和 userret 设置 trapframe 的 kernel_*，
// 从 trapframe 恢复用户寄存器，切换到用户页表，并进入用户空间。
// trapframe 包括被调用者保存的用户寄存器，如 s0-s11，因为通过 usertrapret() 返回用户路径不通过整个内核调用栈。

//当进程从用户态陷入内核态时，内核会将用户态的寄存器保存到 trapframe 中，然后将 trapframe 的地址保存到 sscratch 寄存器中。
//当进程从内核态返回用户态时，内核会从 trapframe 中恢复用户态的寄存器，然后将 sscratch 寄存器中保存的 trapframe 地址恢复到 tp 寄存器中。
//可以从trapframe中获取用户态寄存器的值，如 a0-a7、s0-s11、ra、sp、epc等。
struct trapframe {
  /*   0 */ uint64 kernel_satp;   // 内核页表
  /*   8 */ uint64 kernel_sp;     // 进程的内核栈顶
  /*  16 */ uint64 kernel_trap;   // usertrap()
  /*  24 */ uint64 epc;           // 保存的用户程序计数器
  /*  32 */ uint64 kernel_hartid; // 保存的内核 tp ,又叫做硬件线程ID，也就是CPU的编号
  /*  40 */ uint64 ra;            // 返回地址
  /*  48 */ uint64 sp;            // 栈指针
  /*  56 */ uint64 gp;            // 全局指针
  /*  64 */ uint64 tp;            // 线程指针
  /*  72 */ uint64 t0;            // 临时寄存器
  /*  80 */ uint64 t1;            //  ...
  /*  88 */ uint64 t2;
  /*  96 */ uint64 s0;            // 保存寄存器
  /* 104 */ uint64 s1;            // 保存寄存器
  /* 112 */ uint64 a0;            // 函数参数/返回值
  /* 120 */ uint64 a1;            // 函数参数
  /* 128 */ uint64 a2;            // 函数参数
  /* 136 */ uint64 a3;            // 函数参数
  /* 144 */ uint64 a4;            // 函数参数
  /* 152 */ uint64 a5;            // 函数参数
  /* 160 */ uint64 a6;            // 函数参数
  /* 168 */ uint64 a7;            // 函数参数
  /* 176 */ uint64 s2;            // 保存寄存器
  /* 184 */ uint64 s3;            // 保存寄存器
  /* 192 */ uint64 s4;            // 保存寄存器
  /* 200 */ uint64 s5;            // 保存寄存器
  /* 208 */ uint64 s6;            // 保存寄存器
  /* 216 */ uint64 s7;            // 保存寄存器
  /* 224 */ uint64 s8;            // 保存寄存器
  /* 232 */ uint64 s9;            // 保存寄存器
  /* 240 */ uint64 s10;           // 保存寄存器
  /* 248 */ uint64 s11;           // 保存寄存器
  /* 256 */ uint64 t3;            // 临时寄存器
  /* 264 */ uint64 t4;            // 临时寄存器
  /* 272 */ uint64 t5;            // 临时寄存器
  /* 280 */ uint64 t6;            // 临时寄存器
};

enum procstate { UNUSED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// 每个进程的状态
struct proc {
  struct spinlock lock;

  // 使用这些时必须持有 p->lock：
  enum procstate state;        // 进程的状态：未使用、睡眠、可运行、运行中、僵尸
  struct proc *parent;         // 父进程
  void *chan;                  // 如果非零，表示在 chan 上睡眠
  int killed;                  // 如果非零，表示已被杀死
  int xstate;                  // 退出状态，将返回给父进程的等待
  int pid;                     // 进程 ID

  // 这些是进程私有的，因此不需要持有 p->lock。
  uint64 kstack;               // 内核栈的虚拟地址
  uint64 sz;                   // 进程内存大小（字节）
  pagetable_t pagetable;       // 用户页表
  struct trapframe *trapframe; // trampoline.S 的数据页

  //上下文切换就是指一个进程切换到另一个进程的过程，上下文切换需要保存当前进程的状态，并恢复到下一个进程的状态，
  //context结构体，用于保存进程的寄存器状态，包括ra和sp寄存器，ra记录了函数调用返回地址，sp保存的是当前栈顶的地址。
  struct context context;      // swtch() 到这里运行进程
  struct file *ofile[NOFILE];  // 打开的文件
  struct inode *cwd;           // 当前目录
  char name[16];               // 进程名称（用于调试）
  pagetable_t kama_kernelpgtbl;   // 存储进程独享的内核态页表
};
