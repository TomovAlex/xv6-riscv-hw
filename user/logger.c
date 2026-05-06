#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/logger.h"
#include "user/user.h"

int is_num(char *s) {
  if(*s == 0)
    return 0;
  while(*s) {
    if(*s < '0' || *s > '9')
      return 0;
    s++;
  }
  return 1;
}

int main(int argc, char *argv[]) {
  int mask = 0;
  int nticks = 0;

  if(argc < 2) {
    printf("usage: logger off|all|syscall|intr|proc|exec [ticks]\n");
    exit(1);
  }

  for(int i = 1; i < argc; i++){
    if(is_num(argv[i]) && i == argc - 1)
      nticks = atoi(argv[i]);
    else if(strcmp(argv[i], "off") == 0)
      mask = 0;
    else if(strcmp(argv[i], "all") == 0)
      mask |= LOG_ALL;
    else if(strcmp(argv[i], "syscall") == 0)
      mask |= LOG_SYSCALL;
    else if(strcmp(argv[i], "intr") == 0)
      mask |= LOG_INTR;
    else if(strcmp(argv[i], "proc") == 0)
      mask |= LOG_PROC;
    else if(strcmp(argv[i], "exec") == 0)
      mask |= LOG_EXEC;
    else {
      printf("unknown log class: %s\n", argv[i]);
      exit(1);
    }
  }

  if(logger(mask, nticks) < 0){
    printf("logger failed\n");
    exit(1);
  }

  exit(0);
}
