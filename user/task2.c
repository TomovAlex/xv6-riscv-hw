#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void print_without_mutex(int argc, char *argv[]) {
  int pid = getpid();

  for(int i = 1; i < argc; i++){
    char *s = argv[i];
    for(int j = 0; s[j] != 0; j++)
      printf("%d: arg %d, char '%c'\n", pid, i, s[j]);
  }
}

void print_with_mutex(int mtx, int argc, char *argv[]) {
  int pid = getpid();

  for(int i = 1; i < argc; i++){
    char *s = argv[i];
    for(int j = 0; s[j] != 0; j++) {
      if(mutex_lock(mtx) < 0) {
        fprintf(2, "mutex_lock failed\n");
        exit(1);
      }

      printf("%d: arg %d, char '%c'\n", pid, i, s[j]);

      if(mutex_unlock(mtx) < 0) {
        fprintf(2, "mutex_unlock failed\n");
        exit(1);
      }
    }
  }
}

int main(int argc, char *argv[]) {
  int pid;
  int mtx;

  if(argc < 2) {
    fprintf(2,"no parameters\n");
    exit(1);
  }

  printf("=== without mutex ===\n");

  pid = fork();
  if(pid < 0){
    fprintf(2, "fork failed\n");
    exit(1);
  } else if(pid == 0) {
    print_without_mutex(argc, argv);
    exit(0);
  } else {
    print_without_mutex(argc, argv);
    wait(0);
  }

  printf("=== with mutex ===\n");
  mtx = mutex();
  if(mtx < 0) {
    fprintf(2, "mutex failed\n");
    exit(1);
  }

  pid = fork();
  if(pid < 0){
    fprintf(2, "fork failed\n");
    close(mtx);
    exit(1);
  }

  if(pid == 0){
    print_with_mutex(mtx, argc, argv);
    exit(0);
  } else {
    print_with_mutex(mtx, argc, argv);
    wait(0);
  }

  close(mtx);
  exit(0);
}