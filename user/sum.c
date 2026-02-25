#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int
is_int(const char *s) {
  int i = 0;
  if(s[i] == '-' || s[i] == '+')
    ++i;
  int f = 0;
  for(; s[i]; ++i) {
    if(s[i] < '0' || s[i] > '9')
      return 0;
    ++f;
  }
  if(f > 0)
    return 1;
  else
    return 0;
}

int
main(int argc, char *argv[])
{
  int n = 0;
  char buf[128];
  while(n < (int)sizeof(buf) - 1) {
    char c;
    int r = read(0, &c, 1);
    if(r < 0) {
      fprintf(2, "read error\n");
      exit(1);
    }
    if(r == 0)
      break;
    if(c == '\n')
      break;
    buf[n] = c;
    ++n;
  }
  buf[n] = '\0';
  printf("|%s|\n", buf);

  if(n == (int)sizeof(buf) - 1) {
    fprintf(2, "buffer overflow\n");
    exit(1);
  }

  int i = 0;
  while(buf[i] && (buf[i] == ' ' || buf[i] == '\r' || buf[i] == '\t')) {
    ++i;
  }
  if(buf[i] == '\0') {
    fprintf(2, "empty line\n");
    exit(1);
  }


  int a_st = i;
  while(buf[i] && !(buf[i] == ' ' || buf[i] == '\r' || buf[i] == '\t')) {
    ++i;
  }
  int a_end = i;
  
  while(buf[i] && (buf[i] == ' ' || buf[i] == '\r' || buf[i] == '\t')) {
    ++i;
  }
  if(buf[i] == '\0') {
    fprintf(2, "no second number\n");
    exit(1);
  }

  int b_st = i;
  while(buf[i] && !(buf[i] == ' ' || buf[i] == '\r' || buf[i] == '\t')) {
    ++i;
  }
  int b_end = i;

  while(buf[i] && (buf[i] == ' ' || buf[i] == '\r' || buf[i] == '\t')) {
    ++i;
  }
  if(buf[i] != '\0') {
    fprintf(2, "extra characters after second number\n");
    exit(1);
  }

  buf[a_end] = '\0';
  buf[b_end] = '\0';
  if(!is_int(&buf[a_st]) || !is_int(&buf[b_st])) {
    fprintf(2, "invalid arguments format\n");
    exit(1);
  }

  int a = atoi(&buf[a_st]);
  int b = atoi(&buf[b_st]);
  int c = add(a, b);
  printf("%d\n", c);
  exit(0);
}
