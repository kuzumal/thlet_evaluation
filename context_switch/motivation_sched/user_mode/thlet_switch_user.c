#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <time.h>
#include <stdatomic.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/user.h>
#include <x86intrin.h>
#include "thlet_switch_helper.h"

struct thlet_module_stats {
  atomic_bool finished;
  uint64_t start;
  uint64_t end;
  uint64_t total;
  uint64_t pure_computation;
} __attribute__((__aligned__(64)));

struct kernel_stats {
	uint64_t pick;
	uint64_t cs_mm;
	uint64_t cs_reg;
	uint64_t cs_exit;
	uint64_t cs;
	uint64_t sched;
};

int helper_fd;
struct kernel_stats *kstats;
atomic_size_t finish_cnt = 0;
atomic_bool thlet_switch_start = false;
struct thlet_module_stats stats[THLET_SWITCH_THREADS];

static inline uint64_t get_poisson(void) {
  return (rand() % THLET_LOOPS) + THLET_LOOPS;
}

void* thlet_switch_fn(void *arg) {
  int id = (int)(long)arg;
  uint64_t cnt = 0, pure = 0;
  uint64_t iter_cnt[THLET_SWITCH_ITER];

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(THLET_RUNNING_CORE, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

  struct sched_param param;
  param.sched_priority = 10;
  pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

  for (int i = 0; i < THLET_SWITCH_ITER; i++) {
    iter_cnt[i] = (i == 0) ? get_poisson() : iter_cnt[i - 1] + get_poisson();
  }

  while (!atomic_load_explicit(&thlet_switch_start, memory_order_acquire)) {
    __builtin_ia32_pause();
  }

  uint64_t start_cycle = _rdtsc();
  for (int i = 0; i < THLET_SWITCH_ITER; i++) {
    uint64_t pure_start = _rdtsc();
    do {
        asm volatile ("nop");
    } while (cnt ++ <= iter_cnt[i]);
    uint64_t pure_end = _rdtsc();
    pure += (pure_end - pure_start);

    sched_yield();
  }
  uint64_t end_cycle = _rdtsc();

  // commit
  atomic_store_explicit(&stats[id].finished, true, memory_order_release);
  stats[id].start = start_cycle;
  stats[id].end = end_cycle;
  stats[id].total = iter_cnt[THLET_SWITCH_ITER - 1];
  stats[id].pure_computation = pure;
  atomic_fetch_add(&finish_cnt, 1);

  if (atomic_load_explicit(&finish_cnt, memory_order_acquire) == THLET_SWITCH_THREADS) {
    ioctl(helper_fd, THLET_IOCTL_FINISH, 0);
  }
  return NULL;
}

int main() {
  helper_fd = open("/dev/thlet_switch_helper", O_RDWR);

  // init thlet_switch_helper
  if (helper_fd < 0) {
    perror("Failed to open thlet_switch_helper");
    exit(1);
  }

  kstats = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, helper_fd, 0);
  if (!kstats) {
    perror("mmap failed");
    close(helper_fd);
    exit(1);
  }

  pthread_t threads[THLET_SWITCH_THREADS];
  srand(time(NULL));

  for (int i = 0; i < THLET_SWITCH_THREADS; i++) {
      stats[i].finished = false;
      if (pthread_create(&threads[i], NULL, thlet_switch_fn, (void *)(long)i) != 0) {
          perror("pthread_create");
          return 1;
      }
  }

  usleep(100);
  ioctl(helper_fd, THLET_IOCTL_START, 0);
  atomic_store_explicit(&thlet_switch_start, true, memory_order_release);

  uint64_t pure_sum = 0;
  uint64_t start = 0, end = 0;

  for (int i = 0; i < THLET_SWITCH_THREADS; i++) {
      pthread_join(threads[i], NULL);
      
      pure_sum += stats[i].pure_computation;
      if (i == 0 || stats[i].start < start) start = stats[i].start;
      if (i == 0 || stats[i].end > end) end = stats[i].end;
  }

  uint64_t cost = end - start;
  ioctl(helper_fd, THLET_IOCTL_FINISH, 0);

  if (cost > 0) {
      printf("Threads: %d\n", THLET_SWITCH_THREADS);
      printf("Total Cost: %lu cycles\n", cost);
      printf("Pure Computation: %lu cycles (%lu%%)\n", 
              pure_sum, (pure_sum * 100) / cost);
      printf("Sched Overhead: %lu%%\n", (kstats->sched * 100) / cost);
      printf("CS Overhead: %lu%%\n", (kstats->cs * 100) / cost);
  }

  return 0;
}