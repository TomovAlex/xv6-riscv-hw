struct msgbuf {
  struct spinlock lock;
  char data[NMSGBUFPG * PGSIZE];
  uint head;
  uint tail;
  uint full;
};
