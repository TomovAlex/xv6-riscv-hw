#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "kernel/riscv.h"
#include "user/user.h"

char buf[NMSGBUFPG * PGSIZE + 1];

int main(int argc, char *argv[]) {
  int n;
  if(argc != 1)
    exit(1);

  n = dmesg(buf, sizeof(buf));
  if(n < 0) {
    printf("dmesg failed\n");
    exit(1);
  }

  printf("%s", buf);
  exit(0);
}
