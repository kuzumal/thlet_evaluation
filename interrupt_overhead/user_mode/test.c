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

int driver_fd = -1;
volatile int received = 0;
volatile int start = 0;
volatile uint64_t t1, t2, t3, t4;
pipe_ *info;

void receiver() {
  start = 1;
  while (!received) {
    t1 = _rdtsc();
    t2 = _rdtsc();
    t3 = _rdtsc();
    t4 = _rdtsc();
  }
}

void sender_listener() {
  while (!start) {
    asm ("lfence");
  }

  // trigger interrupt
  if (ioctl(driver_fd, IOCTL_SENDIPI, getpid()) < 0) {
    perror("IOCTL_SENDIPI failed");
    close(driver_fd);
    exit(1);
  }

  uint64_t t_start = MAX(t1, MAX(t2, MAX(t3, t4)));
  printf("intr cost: %llu\n", info->ts.intr - t_start);

  received = 1;
}

int main() {
  driver_fd = open("/dev/thlet_intr", O_RDWR);

  if (ioctl(driver_fd, IOCTL_SENDIPI, getpid()) < 0) {
    perror("IOCTL_SENDIPI failed");
    close(driver_fd);
    exit(1);
  }

  // map shared info from kernel
  info = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, driver_fd, 0);
  if (!info) {
    perror("mmap failed");
    close(driver_fd);
    exit(1);
  }

  int sender_core = 20;

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