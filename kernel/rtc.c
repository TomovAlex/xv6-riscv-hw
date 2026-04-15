#include "types.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "defs.h"

struct spinlock rtc_lock;

void rtcinit(void) {
  initlock(&rtc_lock, "rtc");
}

uint32 rtc_read_low(void) {
  return *(volatile uint32 *)RTC_LOW;
}
uint32 rtc_read_high(void) { 
  return *(volatile uint32 *)RTC_HIGH;
}

uint64 rtc_read_time(void) {
  uint32 low, high;

  acquire(&rtc_lock);
  low = rtc_read_low();
  high = rtc_read_high();
  release(&rtc_lock);

  return ((uint64)high << 32) | low;
}