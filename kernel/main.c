#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

volatile static int started = 0;

// start() 在所有 CPU 上以监督模式跳转到这里。
//xv6操作系统是一个多核操作系统，因此在启动时需要初始化每一个CPU(或称为hart)。main函数中的else部分
//用于初始化除了主CPU(hart 0 )之外的其他CPU。


//_sync_synchronize() 是一个内存屏障（memory barrier）函数。
//用于确保在多核系统中，所有 CPU 看到的内存操作顺序是一致的。它在这里的作用是确保在主 CPU 和其他 CPU 之间的内存操作顺序正确，从而避免由于内存操作重排序导致的不一致问题。
void
main()
{
  if(cpuid() == 0){
    consoleinit();
#if defined(LAB_PGTBL) || defined(LAB_LOCK)
    statsinit();
#endif
    printfinit();
    printf("\n");
    printf("xv6 内核正在启动\n");
    printf("\n");
    kinit();         // 物理页分配器
    printf("1\n");
    kvminit();       // 创建内核页表
    printf("2\n");
    kvminithart();   // 打开分页
    printf("3\n");
    procinit();      // 进程表
    printf("4\n");
    trapinit();      // 陷阱向量
    printf("5\n");
    trapinithart();  // 安装内核陷阱向量
    printf("6\n");
    plicinit();      // 设置中断控制器
    printf("7\n");
    plicinithart();  // 请求 PLIC 设备中断
    printf("8\n");
    binit();         // 缓冲区缓存
    printf("9\n");
    iinit();         // inode 缓存
    printf("10\n");
    fileinit();      // 文件表
    printf("11\n");
    virtio_disk_init(); // 模拟硬盘
    printf("12\n");
#ifdef LAB_NET
    pci_init();
    sockinit();
#endif    
    userinit();      // 第一个用户进程
    printf("13\n");
    __sync_synchronize();
    printf("14\n");
    started = 1;
  } else {
    while(started == 0)
      ;
    __sync_synchronize();
    printf("hart %d 正在启动\n", cpuid());
    kvminithart();    // 打开分页
    plicinithart();   // 请求 PLIC 设备中断
    trapinithart();   // 安装内核陷阱向量
  }
  printf("进入调度器\n");
  scheduler();
}
