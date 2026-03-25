#include "types.h"
#include "spinlock.h"
#include "mutex.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "sleeplock.h"
#include "proc.h"
#include "file.h"

int mutexalloc(struct file **f) {
  struct mutex *m;

  *f = 0;
  if((*f = filealloc()) == 0)
      return -1;

  if((m = (struct mutex*)kalloc()) == 0){
    fileclose(*f);
    *f = 0;
    return -1;
  }

  initsleeplock(&m->lk, "mutex");

  printf("mutexalloc: m=%p\n", m);

  (*f)->type = FD_MUTEX;
  (*f)->readable = 0;
  (*f)->writable = 0;
  (*f)->mtx = m;
  return 0;
}

void mutexclose(struct mutex *m) {
  printf("mutexclose: m=%p\n", m);
  kfree((char*)m);
}

int mutexlock(struct mutex *m) {
  if(holdingsleep(&m->lk))
    return -1;

  acquiresleep(&m->lk);
  return 0;
}

int mutexunlock(struct mutex *m) {
  if(!holdingsleep(&m->lk))
    return -1;

  releasesleep(&m->lk);
  return 0;
}