#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int main(int argc, char *argv[]) {
  int fd, n, i;
  uchar *buf;

  if(argc != 3) {
    fprintf(2, "wrong number of arguments\n");
    exit(1);
  }

  n = atoi(argv[1]);
  if(n < 0){
    fprintf(2, "invalid count\n");
    exit(1);
  }

  fd = open(argv[2], O_RDONLY);
  if(fd < 0) {
    fprintf(2, "cannot open %s\n", argv[2]);
    exit(1);
  }

  buf = malloc(n);
  if(buf == 0) {
    fprintf(2, "malloc failed\n");
    close(fd);
    exit(1);
  }

  int r = read(fd, buf, n);
  if(r < 0){
    fprintf(2, "read error\n");
    free(buf);
    close(fd);
    exit(1);
  }

  char digits[] = "0123456789ABCDEF";

  for(i = 0; i < r; i++) {
    char c1 = digits[(buf[i] >> 4) & 0xF];
    char c2 = digits[buf[i] & 0xF];
    printf("%c%c", c1, c2);
    if(i + 1 < r)
      printf(" ");
  }

  printf("\n");

  free(buf);
  close(fd);
  exit(0);
}