#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"
#include "file.h"

static struct spinlock driver_lock;
static uint64 seed = 12334423665ULL;
static uint64 nullstat_count = 0;

void driverinit(void) {
  initlock(&driver_lock, "driver");

  devsw[PSEUDO].read = pseudoread;
  devsw[PSEUDO].write = pseudowrite;
}

uint64 lcg(uint64 x) {
  return (x * 368423474953ULL + 1323445) % 33654231;
}

int pseudoread(short minor, int user_dst, uint64 dst, int n) {
  char buf[64];
  int m, count, i;
  uint64 value;

  switch(minor){
  case DEV_NULL:
    return 0;
  case DEV_ZERO:
    memset(buf, 0, sizeof(buf));
    count = 0;
    while(count < n){
      m = n - count;
      if(m > 64)
        m = 64;
      if(either_copyout(user_dst, dst + count, buf, m) < 0)
        return -1;
      count += m;
    }
    return n;
  case DEV_URANDOM:
    count = 0;
    while(count < n){
      m = n - count;
      if(m > 64)
        m = 64;

      acquire(&driver_lock);
      for(i = 0; i < m; i++) {
        seed = lcg(seed);
        buf[i] = (char)(seed & 0xFF);
      }
      release(&driver_lock);

      if(either_copyout(user_dst, dst + count, buf, m) < 0)
        return -1;

      count += m;
    }
    return n;
  case DEV_NULLSTAT:
    if(n != sizeof(uint64))
      return -1;

    acquire(&driver_lock);
    value = nullstat_count;
    release(&driver_lock);

    if(either_copyout(user_dst, dst, (char *)&value, sizeof(uint64)) < 0)
      return -1;

    return sizeof(uint64);
  default:
    return -1;
  }
}

int pseudowrite(short minor, int user_src, uint64 src, int n) {
  uint64 new_seed;
  switch(minor){
  case DEV_NULL:
    return n;
  case DEV_ZERO:
    return -1;
  case DEV_URANDOM:
    if(n != sizeof(uint64))
      return -1;

    if(either_copyin((char *)&new_seed, user_src, src, sizeof(uint64)) < 0)
      return -1;

    acquire(&driver_lock);
    seed = new_seed;
    release(&driver_lock);

    return sizeof(uint64);

  case DEV_NULLSTAT:
    acquire(&driver_lock);
    nullstat_count += n;
    release(&driver_lock);
    return n;
  default:
    return -1;    
  }
}