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
#define NUM_ITERS 10000

int driver_fd = -1;
int event_fd = NULL;
volatile int finished = 0;
volatile int received = 0;
volatile int start = 0;
pipe_ *info;

void receiver() {
  start = 1;
  asm ("sfence");
  int check = 0, count = 0;
  volatile uint64_t t1, t2, t3, t4, t5, t6, t7, t8;
  uint64_t irq = 0;
  uint64_t irq2sched = 0;
  uint64_t sched_prepare = 0, sched_pick = 0, sched_wakeup = 0;
  uint64_t cs_prepare = 0, cs_mm = 0, cs_reg = 0, cs_finish = 0;
  uint64_t to_irq = 0, irq_to_softirq = 0;
  uint64_t softirq = 0;

  if (ioctl(driver_fd, IOCTL_STAT_START, getpid()) < 0) {
    perror("IOCTL_STAT_START failed");
    exit(1);
  }

  while (!finished) {
    t1 = _rdtsc();
    t2 = _rdtsc();
    t3 = _rdtsc();
    t4 = _rdtsc();
    t5 = _rdtsc();
    t6 = _rdtsc();

    if (t2 - t1 > 1000) {
      t7 = t1;
      t8 = t2;
      check = 1;
    } else if (t3 - t2 > 1000) {
      t7 = t2;
      t8 = t3;
      check = 1;
    } else if (t4 - t3 > 1000) {
      t7 = t3;
      t8 = t4;
      check = 1;
    } else if (t5 - t4 > 1000) {
      t7 = t4;
      t8 = t5;
      check = 1;
    } else if (t6 - t5 > 1000) {
      t7 = t5;
      t8 = t6;
      check = 1;
    } else {
      check = 0;
    }

    if (check) {
      // printf("t1 t2 t3 t4 t5 t6: %lu %lu %lu %lu %lu %lu\n",
      //        t1, t2, t3, t4, t5, t6);
      // get info from kernel
      if (ioctl(driver_fd, IOCTL_GET_STATS_1, getpid()) < 0) {
        perror("IOCTL_GET_STATS_1 failed");
        exit(1);
      }

      count++;
      irq += info->ts.e1000e_intr_exit - info->ts.e1000e_intr_entry;
      to_irq += info->ts.e1000e_intr_entry - t7;

      if (ioctl(driver_fd, IOCTL_STAT_START, getpid()) < 0) {
        perror("IOCTL_STAT_START failed");
        exit(1);
      }

      received = 1;
      while (received) {
        asm ("lfence");
      }
    }
  }
  if (count) {
    printf("Results over %d iterations:\n", count);
    printf("Average to irq: %lu cycles\n", to_irq / count);
    printf("Average interrupt handling time: %lu cycles\n", irq / count);
    // printf("Average irq to scheduler time: %lu cycles\n", irq2sched / count);
    // printf("Average scheduler prepare time: %lu cycles\n", sched_prepare / count);
    // printf("Average scheduler pick time: %lu cycles\n", sched_pick / count);
    // printf("Average scheduler wakeup time: %lu cycles\n", sched_wakeup / count);
    // printf("Average context switch prepare time: %lu cycles\n", cs_prepare / count);
    // printf("Average context switch mm time: %lu cycles\n", cs_mm / count);
    // printf("Average context switch reg time: %lu cycles\n", cs_reg / count);
    // printf("Average context switch finish time: %lu cycles\n", cs_finish / count);
    // printf("Average irq to softirq time: %lu cycles\n", irq_to_softirq / count);
    // printf("Average softirq time: %lu cycles\n", softirq / count);
    // printf("Total %lu cycles, ours: %lu, %lf\n",
    //        (sched_prepare + sched_pick + sched_wakeup + cs_prepare + cs_mm + cs_reg) / count,
    //        (cs_prepare + cs_mm) / count,
    //        (double)(cs_prepare + cs_mm) / (sched_prepare + sched_pick + sched_wakeup + cs_prepare + cs_mm + cs_reg));
  }
}

void sender_listener() {

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

    received = 0;
  }

  finished = 1;
}

int main() {
  driver_fd = open("/dev/thlet_intr", O_RDWR);
  event_fd = open("/mnt/share/tools/event", O_WRONLY | O_CREAT | O_TRUNC, 0644);

  if (driver_fd < 0 || event_fd < 0) {
    perror("Failed to open");
    exit(1);
  }

  // map shared info from kernel
  info = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, driver_fd, 0);
  if (!info) {
    perror("mmap failed");
    close(driver_fd);
    exit(1);
  }

  // performance varies when sender and receiver are on different numa nodes
  // so put the sender on the same SMT as receiver to maximize cache locality
  int sender_core = 5;

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
  close(event_fd);
  return 0;
}