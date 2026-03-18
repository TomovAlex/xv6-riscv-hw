#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/procinfo.h"
#include "user/user.h"

char* state_name(int state) {
  switch(state) {
  case 0:
    return "UNUSED";
  case 1:
    return "USED";
  case 2:
    return "SLEEPING";
  case 3:
    return "RUNNABLE";
  case 4:
    return "RUNNING";
  case 5:
    return "ZOMBIE";
  default:
    return "UNKNOWN";
  }
}

int int_len(int x) {
  int n = 0;
  if(x <= 0)
    return 1;
  while(x > 0) {
    n++;
    x /= 10;
  }
  return n;
}

void
print_spaces(int n) {
  while(n > 0) {
    printf(" ");
    n--;
  }
}

void print_str_with_space(char *s, int w) {
  int n = strlen(s);
  printf("%s", s);
  if(n < w)
    print_spaces(w - n);
  else
    printf(" ");
}

void print_int_with_space(int x, int w) {
  int n = int_len(x);
  printf("%d", x);
  if(n < w)
    print_spaces(w - n);
  else
    printf(" ");
}



char* parent_name(struct procinfo *buf, int n, int parent_pid) {
  int i;
  if(parent_pid == 0)
    return "-";

  for(i = 0; i < n; i++) {
    if(buf[i].pid == parent_pid)
      return buf[i].name;
  }

  return "?";
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
  struct procinfo *buf;
  int n, i;

  buf = 0;
  if(get_list(&buf, &n) < 0) {
    fprintf(2, "failed to get process list\n");
    exit(1);
  }
  print_str_with_space("pid", 6);
  print_str_with_space("name", 18);
  print_str_with_space("state", 10);
  print_str_with_space("ppid", 9);
  print_str_with_space("pname", 18);
  printf("\n");

  for(i = 0; i < n; i++) {
    print_int_with_space(buf[i].pid, 6);
    print_str_with_space(buf[i].name, 18);
    print_str_with_space(state_name(buf[i].status), 10);
    print_int_with_space(buf[i].parent_pid, 9);
    print_str_with_space(parent_name(buf, n, buf[i].parent_pid), 18);
    printf("\n");
  }

  free(buf);
  exit(0);
}