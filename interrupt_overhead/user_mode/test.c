#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>

#include <errno.h>
#include <sys/user.h>
#define __USE_GNU

#include <pthread.h>
#include <sched.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <x86intrin.h>
#include "../kernel_mode/thlet_intr.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define NUM_ITERS 400000

int driver_fd = -1;
volatile int finished = 0;
volatile int received = 0;
volatile int start = 0;
volatile uint64_t t1, t2, t3, t4, t5, t6, t7, t8, sum, back;
volatile int state = 0;
pipe_ *info;

void receiver() {
  start = 1;

  asm ("sfence");
  while (!finished) {
    t1 = _rdtsc();
    t2 = _rdtsc();
    t3 = _rdtsc();
    t4 = _rdtsc();
    t5 = _rdtsc();
    t6 = _rdtsc();

    if (info->ts.intr != 0) {
      received = 1;
      while (received) {
        asm ("lfence");
      }
    }
  }
}

void sender_listener() {

  int check = 0, count = 0;
  while (!start) {
    asm ("lfence");
  }

  for (int i = 0; i < NUM_ITERS; i++) {
    // trigger interrupt
    if (ioctl(driver_fd, IOCTL_SENDIPI, getpid()) < 0) {
      perror("IOCTL_SENDIPI failed");
      finished = 1;
      exit(1);
    }

    // wait for receiver
    while (!received) {
      asm ("lfence");
    }

    if (t2 - t1 > 100) {
      t7 = t1;
      t8 = t2;
      check = 1;
    } else if (t3 - t2 > 100) {
      t7 = t2;
      t8 = t3;
      check = 1;
    } else if (t4 - t3 > 100) {
      t7 = t3;
      t8 = t4;
      check = 1;
    } else if (t5 - t4 > 100) {
      t7 = t4;
      t8 = t5;
      check = 1;
    } else if (t6 - t5 > 100) {
      t7 = t5;
      t8 = t6;
      check = 1;
    } else {
      check = 0;
    }

    if (check) {
      sum += info->ts.intr - t7;
      back += t8 - info->ts.back;
      count ++;
    }

    info->ts.intr = 0;
    received = 0;
  }

  finished = 1;

  printf("intr cost: %llu, back: %llu\n", sum / count, back / count);
  printf("t8 %llu back %llu\n", t8, info->ts.back);

  exit(1);
}

int main() {
  driver_fd = open("/dev/thlet_intr", O_RDWR);

  // map shared info from kernel
  info = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, driver_fd, 0);
  if (!info) {
    perror("mmap failed");
    close(driver_fd);
    exit(1);
  }

  // performance varies when sender and receiver are on different numa nodes
  int sender_core = 3;

  // bind sender
  pthread_attr_t attrs;
  memset(&attrs, 0, sizeof(attrs));
  pthread_attr_init(&attrs);
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(sender_core, &set);
  pthread_attr_setaffinity_np(&attrs, sizeof(set), &set);
  pthread_t pt;

  // bind receiver to another core (sibling)
  CPU_ZERO(&set);
  CPU_SET(RECV_CORE, &set);
  pthread_setaffinity_np(pthread_self(), sizeof(set), &set);

  if (pthread_create(&pt, &attrs, &sender_listener, NULL))
    exit(-1);
  receiver();
  pthread_join(pt, NULL);

  close(driver_fd);
  return 0;
}