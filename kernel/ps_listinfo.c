#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "procinfo.h"
#include "defs.h"

extern struct proc proc[NPROC];
extern struct spinlock wait_lock;

int ps_listinfo(uint64 plist, int lim) {
  struct proc *p;
  struct procinfo pi;
  struct proc *curproc = myproc();
  int n = 0;

  if(plist == 0){
    for(p = proc; p < &proc[NPROC]; p++){
      acquire(&p->lock);
      if(p->state != UNUSED)
        n++;
      release(&p->lock);
    }
    return n;
  }

  if(lim < 0)
    return -2;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&wait_lock);
    acquire(&p->lock);

    if(p->state == UNUSED){
      release(&p->lock);
      release(&wait_lock);
      continue;
    }

    if(n >= lim){
      release(&p->lock);
      release(&wait_lock);
      return lim + 1;
    }

    pi.pid = p->pid;
    pi.status = p->state;
    if(p->parent != 0)
        pi.parent_pid = p->parent->pid;
    else
        pi.parent_pid = 0;
    safestrcpy(pi.name, p->name, sizeof(pi.name));

    release(&p->lock);
    release(&wait_lock);

    if(copyout(curproc->pagetable, plist + n * sizeof(struct procinfo), (char *)&pi, sizeof(pi)) < 0)
      return -1;

    n++;
  }

  return n;
}