#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int write_pipe(int fd, char* buf, int n) {
  int k = 0;
  while(k < n) {
    int r = write(fd, buf + k, n - k);
    if (r <= 0)
      return -1;
    k += r;
  }
  return 0;
}

int main(int argc, char* argv[]) {
  int pipefd[2];
  if(pipe(pipefd) < 0){
    fprintf(2, "pipe fail\n");
    exit(1);
  }
  
  int pid = fork();
  if(pid < 0) {
    fprintf(2, "fork fail\n");
    close(pipefd[0]);
    close(pipefd[1]);
    exit(1);
  }
  else if(pid == 0) {
    if(close(pipefd[1]) < 0) {
      fprintf(2, "child close(pipefd[1]) fail\n");
      exit(1);
    }
    if(close(0) < 0) {
      fprintf(2, "child close(0) fail\n");
      exit(1);
    }
    if(dup(pipefd[0]) != 0) {
      fprintf(2, "dup(pipefd[0]) fail\n");
      exit(1);
    }
    if(close(pipefd[0]) < 0) {
      fprintf(2, "child close(pipefd[0]) fail\n");
      exit(1);
    }
    char *argv_[] = {"/wc", 0};
    exec("wc", argv_);
    fprintf(2, "exec wc failed\n");
    exit(1);
  }

  if(close(pipefd[0]) < 0) {
    fprintf(2, "parent close(pipefd[0]) fail\n");
    close(pipefd[1]);
    wait(0);
    exit(1);
  }
  int buf_size = 256;
  char out_buf[256];
  int k = 0;

  for(int i = 1; i < argc; ++i){
    int len = strlen(argv[i]);
    int idx = 0;

    while(idx < len){
      if(k == buf_size){
        if(write_pipe(pipefd[1], out_buf, k) < 0){
          fprintf(2, "write pipe fail\n");
          close(pipefd[1]);
          wait(0);
          exit(1);
        }
        k = 0;
      }

      int free_space = buf_size - k;
      int tail = len - idx;
      int take = tail < free_space ? tail : free_space;

      memcpy(out_buf + k, argv[i] + idx, take);
      k += take;
      idx += take;
    }

    if(k == buf_size){
      if(write_pipe(pipefd[1], out_buf, k) < 0){
        fprintf(2, "write pipe fail\n");
        if(close(pipefd[1]) < 0) 
          fprintf(2, "parent close(pipefd[1]) fail\n");
        wait(0);
        exit(1);
      }
      k = 0;
    }
    out_buf[k++] = '\n';
  }

  if(k > 0){
    if(write_pipe(pipefd[1], out_buf, k) < 0){
      fprintf(2, "write pipe fail\n");
      if(close(pipefd[1]) < 0) 
        fprintf(2, "parent close(pipefd[1]) fail\n");
      wait(0);
      exit(1);
    }
  }

  if(close(pipefd[1]) < 0) {
    fprintf(2, "parent close(pipefd[1]) fail\n");
    wait(0);
    exit(1);
  }
  wait(0);
  exit(0);
}