#include "kernel/types.h"
#include "user/user.h"

int is_leap_year(int year) {
  if(year % 400 == 0)
    return 1;
  if(year % 100 == 0)
    return 0;
  if(year % 4 == 0)
    return 1;
  return 0;
}

int days_in_year(int year) {
  return is_leap_year(year) ? 366 : 365;
}

int days_in_month(int year, int month) {
  int mdays[12] = {
    31, 28, 31, 30, 31, 30,
    31, 31, 30, 31, 30, 31
  };

  if(is_leap_year(year))
    mdays[1] = 29;

  return mdays[month - 1];
}

void print2(int x) {
  if(x < 10)
    printf("0");
  printf("%d", x);
}

void print9(uint x) {
  uint div = 100000000;

  while(div > 0){
    printf("%d", x / div);
    x %= div;
    div /= 10;
  }
}

int main(void) {
  uint64 ns = rtc();
  uint64 total_sec = ns / 1000000000ULL;
  uint frac = ns % 1000000000ULL;

  uint64 days = total_sec / 86400;
  uint64 rem = total_sec % 86400;

  int hour = rem / 3600;
  rem %= 3600;
  int min = rem / 60;
  int sec = rem % 60;

  int year = 1970;
  while(days >= (uint64)days_in_year(year)){
    days -= days_in_year(year);
    year++;
  }

  int month = 1;
  while(days >= (uint64)days_in_month(year, month)){
    days -= days_in_month(year, month);
    month++;
  }

  int day = days + 1;

  printf("%d-", year);
  print2(month);
  printf("-");
  print2(day);
  printf(" ");
  print2(hour);
  printf(":");
  print2(min);
  printf(":");
  print2(sec);
  printf(".");
  print9(frac);
  printf("\n");

  exit(0);
}