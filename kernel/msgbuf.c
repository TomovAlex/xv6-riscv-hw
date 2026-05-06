#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "msgbuf.h"
#include "logger.h"

#define MSGBUF_SIZE (NMSGBUFPG * PGSIZE)

struct logger {
  struct spinlock lock;
  int mask;
  uint until;
};

static struct msgbuf msgbuf;
static struct logger logger;
static char digits[] = "0123456789abcdef";

void msgbuf_init(void) {
  initlock(&msgbuf.lock, "msgbuf");
  initlock(&logger.lock, "logger");
  msgbuf.head = 0;
  msgbuf.tail = 0;
  msgbuf.full = 0;
  logger.mask = 0;
  logger.until = 0;
}

static void msgbuf_putchar_locked(char c) {
  msgbuf.data[msgbuf.tail] = c;
  msgbuf.tail = (msgbuf.tail + 1) % MSGBUF_SIZE;

  if(msgbuf.full)
    msgbuf.head = (msgbuf.head + 1) % MSGBUF_SIZE;
  else if(msgbuf.tail == msgbuf.head)
    msgbuf.full = 1;
}

void msgbuf_putchar(char c) {
  acquire(&msgbuf.lock);
  msgbuf_putchar_locked(c);
  release(&msgbuf.lock);
}

int msgbuf_copyout(uint64 dst, int max) {
  uint pos, n, i;
  char c;

  if(max < 1)
    return -1;

  acquire(&msgbuf.lock);

  if(msgbuf.full)
    n = MSGBUF_SIZE;
  else if(msgbuf.tail >= msgbuf.head)
    n = msgbuf.tail - msgbuf.head;
  else
    n = MSGBUF_SIZE - msgbuf.head + msgbuf.tail;

  pos = msgbuf.head;
  while(n > 0 && msgbuf.data[pos] != '[') {
    pos = (pos + 1) % MSGBUF_SIZE;
    n--;
  }

  if(n > max - 1)
    n = max - 1;

  for(i = 0; i < n; i++) {
    c = msgbuf.data[pos];
    if(either_copyout(1, dst + i, &c, 1) < 0) {
      release(&msgbuf.lock);
      return -1;
    }
    pos = (pos + 1) % MSGBUF_SIZE;
  }

  c = 0;
  if(either_copyout(1, dst + i, &c, 1) < 0) {
    release(&msgbuf.lock);
    return -1;
  }

  release(&msgbuf.lock);
  return i;
}

int logger_set(int mask, int nticks) {
  if(nticks < 0)
    return -1;
  acquire(&tickslock);
  uint now = ticks;
  release(&tickslock);

  acquire(&logger.lock);
  logger.mask = mask;
  logger.until = nticks == 0 ? 0 : now + nticks;
  release(&logger.lock);

  return 0;
}

int log_enabled(int cls) {
  acquire(&tickslock);
  uint now = ticks;
  release(&tickslock);

  acquire(&logger.lock);
  if(logger.until != 0 && now >= logger.until)
    logger.mask = 0;
  int enabled = (logger.mask & cls) != 0;
  release(&logger.lock);

  return enabled;
}

static void msgbuf_printint(long long xx, int base, int sign) {
  char buf[20];
  unsigned long long x;

  if(sign && (sign = (xx < 0)))
    x = -xx;
  else
    x = xx;

  int i = 0;
  do {
    buf[i++] = digits[x % base];
  } while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    msgbuf_putchar_locked(buf[i]);
}

static void msgbuf_printptr(uint64 x) {
  msgbuf_putchar_locked('0');
  msgbuf_putchar_locked('x');
  for(int i = 0; i < sizeof(uint64) * 2; i++, x <<= 4)
    msgbuf_putchar_locked(digits[x >> (sizeof(uint64) * 8 - 4)]);
}

static void msgbuf_printf(const char *fmt, va_list ap) {
  int i, cx, c0, c1, c2;
  char *s;

  for(i = 0; (cx = fmt[i] & 0xff) != 0; i++){
    if(cx != '%'){
      msgbuf_putchar_locked(cx);
      continue;
    }
    i++;
    c0 = fmt[i+0] & 0xff;
    c1 = c2 = 0;
    if(c0) c1 = fmt[i+1] & 0xff;
    if(c1) c2 = fmt[i+2] & 0xff;
    if(c0 == 'd'){
      msgbuf_printint(va_arg(ap, int), 10, 1);
    } else if(c0 == 'l' && c1 == 'd'){
      msgbuf_printint(va_arg(ap, uint64), 10, 1);
      i += 1;
    } else if(c0 == 'l' && c1 == 'l' && c2 == 'd'){
      msgbuf_printint(va_arg(ap, uint64), 10, 1);
      i += 2;
    } else if(c0 == 'u'){
      msgbuf_printint(va_arg(ap, uint32), 10, 0);
    } else if(c0 == 'l' && c1 == 'u'){
      msgbuf_printint(va_arg(ap, uint64), 10, 0);
      i += 1;
    } else if(c0 == 'l' && c1 == 'l' && c2 == 'u'){
      msgbuf_printint(va_arg(ap, uint64), 10, 0);
      i += 2;
    } else if(c0 == 'x'){
      msgbuf_printint(va_arg(ap, uint32), 16, 0);
    } else if(c0 == 'l' && c1 == 'x'){
      msgbuf_printint(va_arg(ap, uint64), 16, 0);
      i += 1;
    } else if(c0 == 'l' && c1 == 'l' && c2 == 'x'){
      msgbuf_printint(va_arg(ap, uint64), 16, 0);
      i += 2;
    } else if(c0 == 'p'){
      msgbuf_printptr(va_arg(ap, uint64));
    } else if(c0 == 'c'){
      msgbuf_putchar_locked(va_arg(ap, uint));
    } else if(c0 == 's'){
      if((s = va_arg(ap, char*)) == 0)
        s = "(null)";
      for(; *s; s++)
        msgbuf_putchar_locked(*s);
    } else if(c0 == '%'){
      msgbuf_putchar_locked('%');
    } else if(c0 == 0){
      break;
    } else {
      msgbuf_putchar_locked('%');
      msgbuf_putchar_locked(c0);
    }
  }
}

void pr_msg(const char *fmt, ...) {
  va_list ap;
  uint t;

  acquire(&tickslock);
  t = ticks;
  release(&tickslock);

  acquire(&msgbuf.lock);
  msgbuf_putchar_locked('[');
  msgbuf_printint(t, 10, 0);
  msgbuf_putchar_locked(']');
  msgbuf_putchar_locked(' ');

  va_start(ap, fmt);
  msgbuf_printf(fmt, ap);
  va_end(ap);

  msgbuf_putchar_locked('\n');
  release(&msgbuf.lock);
}
