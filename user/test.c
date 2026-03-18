#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/procinfo.h"
#include "user/user.h"

void print_test_result(char *name, int pass) {
  if(pass)
    printf("PASS: %s\n", name);
  else
    printf("FAIL: %s\n", name);
}

int find_pid(struct procinfo *buf, int n, int pid) {
  int i;
  for(i = 0; i < n; i++) {
    if(buf[i].pid == pid)
      return i;
  }
  return -1;
}

int get_list(struct procinfo **out_buf, int *out_n) {
  int lim, r;
  struct procinfo *buf;

  lim = ps_listinfo(0, 0);
  if(lim < 1)
    lim = 1;

  lim += 4;

  buf = 0;
  r = lim + 1;

  while(r > lim) {
    buf = malloc(lim * sizeof(struct procinfo));
    if(buf == 0)
      return -1;

    r = ps_listinfo(buf, lim);
    if(r < 0) {
      free(buf);
      return -1;
    }

    if(r > lim) {
      free(buf);
      lim = lim * 2;
    }
  }

  *out_buf = buf;
  *out_n = r;
  return 0;
}

int main(int argc, char *argv[]) {
  int n1, n2, r, idx, me, pid;
  struct procinfo *buf;
  struct procinfo one;
  char *end;

  n1 = ps_listinfo(0, 0);
  print_test_result("count_nonzero", n1 > 0);

  buf = malloc(sizeof(struct procinfo));
  if(buf == 0) {
    fprintf(2, "malloc failed\n");
    exit(1);
  }

  r = ps_listinfo(buf, 0);
  print_test_result("small_buf", r > 0);
  free(buf);

  r = ps_listinfo(&one, -1);
  print_test_result("neg_lim", r < 0);

  end = sbrk(0);
  r = ps_listinfo((struct procinfo *)(end + 4096), 1);
  print_test_result("bad_addr", r < 0);

  buf = 0;
  r = get_list(&buf, &n2);
  print_test_result("get_list", r == 0 && buf != 0 && n2 > 0);

  if(r == 0) {
    me = getpid();
    idx = find_pid(buf, n2, me);
    print_test_result("self_found", idx >= 0);

    if(idx >= 0) {
      print_test_result("self_name_not_empty", buf[idx].name[0] != 0);
      print_test_result("self_ppid_correct", buf[idx].parent_pid >= 0);
    } else {
      print_test_result("self_name_not_empty", 0);
      print_test_result("self_ppid_correct", 0);
    }

    free(buf);
  } else {
    print_test_result("self_found", 0);
    print_test_result("self_name_not_empty", 0);
    print_test_result("self_ppid_correct", 0);
  }

  pid = fork();
  if(pid < 0) {
    fprintf(2, "fork failed\n");
  } else if(pid == 0) {
    pause(10);
    exit(0);
  } else {
    buf = 0;
    r = get_list(&buf, &n2);
    if(r == 0) {
      idx = find_pid(buf, n2, pid);
      print_test_result("child_found", idx >= 0);

      if(idx >= 0) {
        print_test_result("child_name_not_empty", buf[idx].name[0] != 0);
        print_test_result("child_ppid_correct", buf[idx].parent_pid == getpid());
      } else {
        print_test_result("child_name_not_empty", 0);
        print_test_result("child_ppid_correct", 0);
      }
      free(buf);
    } else {
      print_test_result("child_found", 0);
      print_test_result("child_ppid_correct", 0);
    }

    wait(0);
  }

  exit(0);
}