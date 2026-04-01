#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int hexvalue(char c) {
  if(c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if(c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  if(c >= '0' && c <= '9')
    return c - '0';
  return -1;
}

int main(int argc, char *argv[]) {
  int fd, len, i;
  unsigned char *buf;

  if(argc != 3){
    fprintf(2, "wrong number of arguments\n");
    exit(1);
  }

  len = strlen(argv[1]);
  if(len % 2 != 0){
    fprintf(2, "length must be even\n");
    exit(1);
  }

  buf = malloc(len / 2);
  if(buf == 0){
    fprintf(2, "malloc failed\n");
    exit(1);
  }

  for(i = 0; i < len; i += 2) {
    int h = hexvalue(argv[1][i]);
    int l = hexvalue(argv[1][i + 1]);

    if(h < 0 || l < 0){
      fprintf(2, "invalid hex string\n");
      free(buf);
      exit(1);
    }

    buf[i / 2] = (h << 4) | l;
  }

  fd = open(argv[2], O_WRONLY);
  if(fd < 0) {
    fprintf(2, "cannot open %s\n", argv[2]);
    free(buf);
    exit(1);
  }

  int k = write(fd, buf, len / 2);
  if(k != len / 2) {
    printf("write error\n");
    free(buf);
    close(fd);
    exit(1);
  }

  free(buf);
  close(fd);
  exit(0);
}