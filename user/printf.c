#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#include <stdarg.h>

static char digits[] = "0123456789ABCDEF";

static void
putc(int fd, char c)
{
  write(fd, &c, 1);
}

static void
printint(int fd, int xx, int base, int sgn)
{
  char buf[16];
  int i, neg;
  uint x;

  neg = 0;
  if(sgn && xx < 0){
    neg = 1;
    x = -xx;
  } else {
    x = xx;
  }

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);
  if(neg)
    buf[i++] = '-';

  while(--i >= 0)
    putc(fd, buf[i]);
}

static void
printptr(int fd, uint64 x) {
  int i;
  putc(fd, '0');
  putc(fd, 'x');
  for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
    putc(fd, digits[x >> (sizeof(uint64) * 8 - 4)]);
}

// Print to the given fd. Only understands %d, %x, %p, %s.
//vprintf 函数通过遍历格式字符串，根据不同的状态处理普通字符和格式化占位符.
//从可变参数列表中获取相应的参数，并将格式化后的字符串输出到指定的文件描述符中。这样就实现了简单的格式化输出功能。
void
vprintf(int fd, const char *fmt, va_list ap)
{
  char *s;
  int c, i, state;
  //state是用来记录当前的状态的，是否遇到了'%'字符
  state = 0;
  for(i = 0; fmt[i]; i++){
    c = fmt[i] & 0xff; //将c变为无符号字符
    if(state == 0){
      if(c == '%'){
        state = '%';
      } else {
        putc(fd, c);
      }
    } else if(state == '%'){
      if(c == 'd'){//整形 int
        printint(fd, va_arg(ap, int), 10, 1);
      } else if(c == 'l') {//uint64 整形
        printint(fd, va_arg(ap, uint64), 10, 0);
      } else if(c == 'x') {//十六进制整形
        printint(fd, va_arg(ap, int), 16, 0);
      } else if(c == 'p') {//uint64 地址
        printptr(fd, va_arg(ap, uint64));
      } else if(c == 's'){//获取一个字符串s
        s = va_arg(ap, char*);
        if(s == 0)
          s = "(null)";
        while(*s != 0){
          putc(fd, *s);
          s++;
        }
      } else if(c == 'c'){
        putc(fd, va_arg(ap, uint));
      } else if(c == '%'){
        putc(fd, c);
      } else {
        // Unknown % sequence.  Print it to draw attention.
        putc(fd, '%');
        putc(fd, c);
      }
      state = 0;
    }
  }
}

void
fprintf(int fd, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vprintf(fd, fmt, ap);
}

void
printf(const char *fmt, ...)
{
  va_list ap;//可变参

  va_start(ap, fmt);
  vprintf(1, fmt, ap);
}
