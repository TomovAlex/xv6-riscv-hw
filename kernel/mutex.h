#ifndef KERNEL_MUTEX_H
#define KERNEL_MUTEX_H

#include "sleeplock.h"
struct mutex {
  struct sleeplock lk;
};

#endif