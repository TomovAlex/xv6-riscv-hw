#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/riscv.h"
#include "user/user.h"

#define HEAP_BYTES (3 * PGSIZE)
#define STACK_ARR_SIZE 128

int global = 123;
int dummy = 0;

static void show_flags(char *name, void *addr, int len) {
  int a = pgtcheck_flags(addr, len, PTE_A);
  int d = pgtcheck_flags(addr, len, PTE_D);

  printf("%s: addr=%p A=%d D=%d\n", name, addr, a, d);
}

static int clear_flags(char *name, void *addr, int len) {
  int r = pgtclear_flags(addr, len, PTE_A | PTE_D);
  if(r < 0) {
    printf("clear failed for %s\n", name);
    return -1;
  }
  return 0;
}

static void show_stage(char *title, int *stack_single, char *stack_arr, char *heap) {
  printf("\n================ %s ================\n", title);

  printf("\nflags status:\n");
  show_flags("global var", (void*)&global, sizeof(global));
  show_flags("stack var", (void*)stack_single, sizeof(*stack_single));
  show_flags("stack array elem", (void*)&stack_arr[67], 1);
  if(heap) {
    show_flags("heap page 0 elem", (void*)&heap[0], 1);
    show_flags("heap page 1 elem", (void*)&heap[PGSIZE], 1);
    show_flags("heap page 2 elem", (void*)&heap[2 * PGSIZE], 1);
    show_flags("heap whole range", (void*)heap, HEAP_BYTES);
  }
  printf("\n");
  pagetableprint();
}

int main(void) {
  int stack_single = 456;
  char stack_arr[STACK_ARR_SIZE];
  char *heap = 0;
  int i;

  for(i = 0; i < STACK_ARR_SIZE; i++)
    stack_arr[i] = 0;
  stack_arr[67] = 25;

  show_stage("START", &stack_single, stack_arr, heap);

  heap = sbrk(HEAP_BYTES);
  if(heap == SBRK_ERROR) {
    printf("sbrk failed\n");
    exit(1);
  }

  show_stage("AFTER HEAP ALLOCATION", &stack_single, stack_arr, heap);

  heap[0] = 10;
  heap[PGSIZE] = 20;
  heap[2 * PGSIZE] = 30;

  if(clear_flags("global int", (void*)&global, sizeof(global)) < 0)
    exit(1);
  if(clear_flags("stack int", (void*)&stack_single, sizeof(stack_single)) < 0)
    exit(1);
  if(clear_flags("stack array elem", (void*)&stack_arr[67], 1) < 0)
    exit(1);
  if(clear_flags("heap whole range", (void*)heap, HEAP_BYTES) < 0)
    exit(1);

  show_stage("AFTER CLEAR A/D FLAGS", &stack_single, stack_arr, heap);

  dummy += global;
  dummy += stack_single;
  dummy += stack_arr[67];
  dummy += heap[0];
  dummy += heap[PGSIZE];
  dummy += heap[2 * PGSIZE];

  show_stage("AFTER READ", &stack_single, stack_arr, heap);

  global += 1;
  stack_single += 1;
  stack_arr[67] += 1;
  heap[0] += 1;
  heap[PGSIZE] += 1;
  heap[2 * PGSIZE] += 1;

  show_stage("AFTER WRITE", &stack_single, stack_arr, heap);

  if(sbrk(-HEAP_BYTES) == SBRK_ERROR) {
    printf("sbrk free failed\n");
    exit(1);
  }
  heap = 0;

  show_stage("AFTER HEAP FREE", &stack_single, stack_arr, heap);
  exit(0);
}