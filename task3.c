#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>


int write_pipe(int fd, char* buf, int n) {
  int k = 0;
  while(k < n) {
    ssize_t r = write(fd, buf + k, n - k);
    if (r < 0) {
      if(errno == EINTR)
        continue;
      return -1;
    }
    if(r == 0)
      return -1;
    k += (int)r;
  }
  return 0;
}

int main(int argc, char* argv[]) {
  int pipefd[2];
  if(pipe(pipefd) < 0){
    perror("pipe fail");
    exit(1);
  }
  
  int buf_size = 16384;
  pid_t pid = fork();
  if(pid < 0) {
    perror("fork fail");
    close(pipefd[0]);
    close(pipefd[1]);
    exit(1);
  }
  else if(pid == 0) {
    if(close(pipefd[1]) < 0) {
      perror("child close(pipefd[1]) fail");
      exit(1);
    }

    char in_buf[16384];
    ssize_t r;
    while ((r = read(pipefd[0], in_buf, buf_size)) > 0) {
      if (write_pipe(1, in_buf, r) < 0) {
        perror("write stdout fail");
        exit(1);
      }
    }
    if (r < 0) {
      perror("read fail");
      exit(1);
    }

    if(close(pipefd[0]) < 0) {
      perror("child close(pipefd[0]) fail");
      exit(1);
    }
    exit(0);
  }

  if(close(pipefd[0]) < 0) {
    perror("parent close(pipefd[0]) fail");
    close(pipefd[1]);
    wait(0);
    exit(1);
  }
  
  char out_buf[16384];
  int k = 0;

  for(int i = 1; i < argc; ++i){
    int len = strlen(argv[i]);
    int idx = 0;

    while(idx < len){
      if(k == buf_size){
        if(write_pipe(pipefd[1], out_buf, k) < 0){
          perror("write pipe fail");
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
        perror("write pipe fail");
        close(pipefd[1]);
        wait(0);
        exit(1);
      }
      k = 0;
    }
    out_buf[k++] = '\n';
  }

  if(k > 0){
    if(write_pipe(pipefd[1], out_buf, k) < 0){
      perror("write pipe fail");
      close(pipefd[1]);
      wait(0);
      exit(1);
    }
  }

  if(close(pipefd[1]) < 0) {
    perror("parent close(pipefd[1]) fail");
    wait(0);
    exit(1);
  }
  int status;
  waitpid(pid, &status, 0);
  return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}