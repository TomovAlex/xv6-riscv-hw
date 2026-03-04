#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void) {
  int pid = fork();
  if(pid < 0) { 
    printf("fork fail\n");
    exit(1);
  }
  if(pid == 0) {
    pause(7*10);
    exit(1);
  }

  printf("parent = %d child = %d\n", getpid(), pid);
  int status;
  printf("waited pid = %d status = %d\n", wait(&status), status);
  exit(0);
}