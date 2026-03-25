#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void pass(char *test_name) {
  printf("%s: PASS\n", test_name);
}

void fail(char *test_name) {
  printf("%s: FAIL\n", test_name);
}

void print_test_name(char *test_name) {
  printf("\n=== %s ===\n", test_name);
}

void read_write_test() {
  char* test_name = "read/write return  error";
  print_test_name(test_name);

  int fd = mutex();
  if(fd < 0) {
    fprintf(2, "create mutex failled");
    return;
  }
  char buf[1];
  char c = 'a';
  if((read(fd, buf, 1) < 0 && (write(fd, &c, 1)) < 0))
    pass(test_name);
  else
    fail(test_name);
  close(fd);
}

void close_mutex_by_owner() {
  char *test_name = "close mutex by owner";
  print_test_name(test_name);
  int mtx = mutex();
  int p[2];
  int pid;
  int t;
  char ready;

  if(mtx < 0) {
    fprintf(2, "create mutex failed\n");
    fail(test_name);
    return;
  }

  if(pipe(p) < 0) {
    fprintf(2, "pipe failed\n");
    close(mtx);
    fail(test_name);
    return;
  }

  if(mutex_lock(mtx) < 0) {
    fprintf(2, "mutex_lock by parent failed\n");
    close(p[0]);
    close(p[1]);
    close(mtx);
    fail(test_name);
    return;
  }
  printf("  parent locked mutex\n");

  pid = fork();
  if(pid < 0){
    fprintf(2, "fork failed\n");
    mutex_unlock(mtx);
    close(p[0]);
    close(p[1]);
    close(mtx);
    fail(test_name);
    return;
  } else if(pid == 0) {
    int start, end, r;

    close(p[0]);

    ready = 'y';
    write(p[1], &ready, 1);

    start = uptime();
    r = mutex_lock(mtx);
    end = uptime();

    t = -1;
    if(r == 0) {
      t = end - start;
      mutex_unlock(mtx);
    }

    write(p[1], &t, sizeof(t));
    close(p[1]);
    close(mtx);
    exit(0);
  } else {
    close(p[1]);

    read(p[0], &ready, 1);
    printf("  child started waiting for mutex\n");
    pause(5);
    printf("  parent closes locked mutex\n");
    close(mtx);

    t = -1;
    read(p[0], &t, sizeof(t));
    printf("  child wait time %d\n", t);
    close(p[0]);

    wait(0);

    if(t >= 3 && t < 15)
      pass(test_name);
    else
      fail(test_name);
  }
}

void close_mutex_by_nonowner() {
  char *test_name = "close mutex by non-owner";
  print_test_name(test_name);
  int mtx = mutex();
  int p[2];
  int pid1, pid2;
  int t;
  char ready;

  if(mtx < 0) {
    fprintf(2, "create mutex failed\n");
    fail(test_name);
    return;
  }

  if(pipe(p) < 0) {
    fprintf(2, "pipe failed\n");
    close(mtx);
    fail(test_name);
    return;
  }

  if(mutex_lock(mtx) < 0) {
    fprintf(2, "mutex_lock by parent failed\n");
    close(p[0]);
    close(p[1]);
    close(mtx);
    fail(test_name);
    return;
  }
  printf("  parent locked mutex\n");

  pid1 = fork();
  if(pid1 < 0) {
    fprintf(2, "fork failed\n");
    mutex_unlock(mtx);
    close(p[0]);
    close(p[1]);
    close(mtx);
    fail(test_name);
    return;
  }

  if(pid1 == 0) {
    close(p[0]);
    close(p[1]);
    pause(5);
    close(mtx); 
    exit(0);
  }

  pid2 = fork();
  if(pid2 < 0) {
    fprintf(2, "fork failed\n");
    mutex_unlock(mtx);
    close(p[0]);
    close(p[1]);
    close(mtx);
    wait(0);
    fail(test_name);
    return;
  }

  if(pid2 == 0) {
    int start, end, r;

    close(p[0]);

    ready = 'y';
    write(p[1], &ready, 1);

    start = uptime();
    r = mutex_lock(mtx);
    end = uptime();

    t = -1;
    if(r == 0) {
      t = end - start;
      mutex_unlock(mtx);
    }

    write(p[1], &t, sizeof(t));
    close(p[1]);
    close(mtx);
    exit(0);
  }

  close(p[1]);

  read(p[0], &ready, 1); 
  printf("  child started waiting for mutex\n");
  pause(15);
  printf("  parent unlocks mutex\n");
  mutex_unlock(mtx);

  t = -1;
  read(p[0], &t, sizeof(t));
  printf("  child wait time %d\n", t);
  close(p[0]);

  wait(0);
  wait(0);
  close(mtx);

  if(t >= 10)
    pass(test_name);
  else
    fail(test_name);
}


void unlock_by_nonowner_test() {
  char *test_name = "unlock by non-owner";
  print_test_name(test_name);
  int mtx = mutex();
  int pid;

  if(mtx < 0) {
    fprintf(2, "create mutex failed\n");
    fail(test_name);
    return;
  }

  if(mutex_lock(mtx) < 0) {
    fprintf(2, "mutex_lock by parent failed\n");
    close(mtx);
    fail(test_name);
    return;
  }

  pid = fork();
  if(pid < 0) {
    fprintf(2, "fork failed\n");
    mutex_unlock(mtx);
    close(mtx);
    fail(test_name);
    return;
  }

  if(pid == 0) {
    if(mutex_unlock(mtx) < 0)
      pass(test_name);
    else
      fail(test_name);

    close(mtx);
    exit(0);
  } else {
    wait(0);
    mutex_unlock(mtx);
    close(mtx);
  }
}

void exit_by_owner_test() {
  char *test_name = "exit by owner releases mutex";
  print_test_name(test_name);
  int mtx = mutex();
  int pid;

  if(mtx < 0) {
    fprintf(2, "create mutex failed\n");
    fail(test_name);
    return;
  }

  pid = fork();
  if(pid < 0) {
    fprintf(2, "fork failed\n");
    close(mtx);
    fail(test_name);
    return;
  }

  if(pid == 0) {
    if(mutex_lock(mtx) < 0)
      exit(1);
    exit(0);
  } else {
    wait(0);

    if(mutex_lock(mtx) == 0) {
      pass(test_name);
      mutex_unlock(mtx);
    } else {
      fail(test_name);
    }

    close(mtx);
  }
}

void exit_by_nonowner_test() {
  char *test_name = "exit by non-owner keeps lock";
  print_test_name(test_name);
  int mtx = mutex();
  int pid;

  if(mtx < 0) {
    fprintf(2, "create mutex failed\n");
    fail(test_name);
    return;
  }

  if(mutex_lock(mtx) < 0) {
    fprintf(2, "mutex_lock by parent failed\n");
    close(mtx);
    fail(test_name);
    return;
  }

  pid = fork();
  if(pid < 0) {
    fprintf(2, "fork failed\n");
    mutex_unlock(mtx);
    close(mtx);
    fail(test_name);
    return;
  }

  if(pid == 0) {
    exit(0);
  } else {
    wait(0);

    if(mutex_unlock(mtx) == 0)
      pass(test_name);
    else
      fail(test_name);

    close(mtx);
  }
}

int main() {
  read_write_test();
  close_mutex_by_owner();
  close_mutex_by_nonowner();
  unlock_by_nonowner_test();
  exit_by_owner_test();
  exit_by_nonowner_test();

  exit(0);
}