//
// low-level driver routines for 16550a UART.
//

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

// UART 控制寄存器是内存映射的
// 在地址 UART0 处。此宏返回
// 其中一个寄存器的地址。
#define Reg(reg) ((volatile unsigned char *)(UART0 + reg))

// UART 控制寄存器。
// 有些具有不同的含义
// 读取与写入。
// 请参阅 http://byterunner.com/16550.html
#define RHR 0                 // 接收保持寄存器 (用于输入字节)
#define THR 0                 // 发送保持寄存器 (用于输出字节)
#define IER 1                 // 中断使能寄存器
#define IER_TX_ENABLE (1<<0)
#define IER_RX_ENABLE (1<<1)
#define FCR 2                 // FIFO 控制寄存器
#define FCR_FIFO_ENABLE (1<<0)
#define FCR_FIFO_CLEAR (3<<1) // 清除两个 FIFO 的内容
#define ISR 2                 // 中断状态寄存器
#define LCR 3                 // 线控寄存器
#define LCR_EIGHT_BITS (3<<0)
#define LCR_BAUD_LATCH (1<<7) // 特殊模式设置波特率
#define LSR 5                 // 线路状态寄存器
#define LSR_RX_READY (1<<0)   // input 正在等待从 RHR 读取
#define LSR_TX_IDLE (1<<5)    // THR 可以接受另一个字符发送

#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))

//  发送输出缓冲区。
struct spinlock uart_tx_lock;
#define UART_TX_BUF_SIZE 32
char uart_tx_buf[UART_TX_BUF_SIZE];
int uart_tx_w; // write next to uart_tx_buf[uart_tx_w++]
int uart_tx_r; // read next from uart_tx_buf[uar_tx_r++]

extern volatile int panicked; // from printf.c

void uartstart();

void
uartinit(void)
{
  //printf("uartinit\n");
  // disable interrupts.
  // 禁止中断
  WriteReg(IER, 0x00);

  // special mode to set baud rate.
  WriteReg(LCR, LCR_BAUD_LATCH);

  // 波特率为 38.4K 的 LSB
  WriteReg(0, 0x03);

  // 波特率为 38.4K 的 MSB.
  WriteReg(1, 0x00);

  // leave set-baud mode,
  // and set word length to 8 bits, no parity.
  //离开 set-baud 模式，
  //并将字长设置为 8 位，无奇偶校验。
  WriteReg(LCR, LCR_EIGHT_BITS);

  // 重置和启用 FIFO
  WriteReg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);

  // 使能 TRANSMIT 和 RECEIVE INTERRUPTS。
  WriteReg(IER, IER_TX_ENABLE | IER_RX_ENABLE);

  initlock(&uart_tx_lock, "uart");
}

// add a character to the output buffer and tell the
// UART to start sending if it isn't already.
// blocks if the output buffer is full.
// because it may block, it can't be called
// from interrupts; it's only suitable for use
// by write().

// 向 Output Buffer 添加一个字符，并告诉
// UART 开始发送（如果尚未开始发送）。
// 如果输出缓冲区已满，则阻止。
// 因为它可能会阻塞，所以不能调用
// 从中断;它只适合使用
// by write（） 来获取。
void
uartputc(int c)
{
  //printf("uartputc\n");
  acquire(&uart_tx_lock);

  if(panicked){
    for(;;)
      ;
  }

  while(1){
    if(((uart_tx_w + 1) % UART_TX_BUF_SIZE) == uart_tx_r){
      // buffer is full.
      // wait for uartstart() to open up space in the buffer.
      sleep(&uart_tx_r, &uart_tx_lock);
    } else {
      uart_tx_buf[uart_tx_w] = c;
      uart_tx_w = (uart_tx_w + 1) % UART_TX_BUF_SIZE;
      uartstart();
      release(&uart_tx_lock);
      return;
    }
  }
}



// uartputc()的替代版本，它不会
// use interrupts，供内核 printf()和
// 以回显字符。它旋转等待 UART 的
// output register 为空。
void
uartputc_sync(int c)
{
  //printf("uartputc_sync\n");
  push_off();

  if(panicked){
    for(;;)
      ;
  }

  // wait for Transmit Holding Empty to be set in LSR.
  while((ReadReg(LSR) & LSR_TX_IDLE) == 0)
    ;
  WriteReg(THR, c);

  pop_off();
}

// if the UART is idle, and a character is waiting
// in the transmit buffer, send it.
// caller must hold uart_tx_lock.
// called from both the top- and bottom-half.
void
uartstart()
{
  //printf("uartstart\n");
  while(1){
    // transmit buffer is empty.
    if(uart_tx_w == uart_tx_r){
      return;
    }
    
    if((ReadReg(LSR) & LSR_TX_IDLE) == 0){
      // UART 发送保持寄存器已满，
      // 所以我们不能给它另一个字节。
      // 当它准备好新字节时，它会中断。
      return;
    }
    
    int c = uart_tx_buf[uart_tx_r];
    uart_tx_r = (uart_tx_r + 1) % UART_TX_BUF_SIZE;
    
    // 也许 uartputc() 正在等待缓冲区中的空间。
    wakeup(&uart_tx_r);
    
    WriteReg(THR, c);
  }
}

// read one input character from the UART.
// return -1 if none is waiting.
// 从 UART 读取一个输入字符。
// 如果没有等待，则返回 -1。
int
uartgetc(void)
{
  //printf("uartgetc\n");
  if(ReadReg(LSR) & 0x01){
    // input data is ready.
    return ReadReg(RHR);
  } else {
    return -1;
  }
}


// 处理一个 UART 中断，中断可能由于以下原因触发：
// 1. 输入数据到达（接收缓冲区中有新数据）。
// 2. UART 准备好发送更多数据（发送缓冲区有空闲）。
// 3. 以上两种情况同时发生。
// 该函数由 trap.c 调用。
void
uartintr(void)
{
  //printf("uartintr\n");
  //读取和处理传入字符。
  while(1){
    int c = uartgetc();
    if(c == -1)
      break;
    consoleintr(c);
  }

  // send buffered characters.
  acquire(&uart_tx_lock);
  uartstart();
  release(&uart_tx_lock);
}
