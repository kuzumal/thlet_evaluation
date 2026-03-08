#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/percpu.h>

#define THLET_SWITCH_ITER    100
#define THLET_SWITCH_THREADS 8
#define THLET_LOOPS          1000
#define THLET_RUNNING_CORE   20

struct thlet_module_stats {
  u64 finished;
  u64 start;
  u64 end;
  u64 total;
  u64 pure_computation;
} __attribute__((__aligned__(64)));

volatile u64 finish_cnt;
volatile bool thlet_switch_start;
struct task_struct* thlet_switch[THLET_SWITCH_THREADS];
struct thlet_module_stats stats[THLET_SWITCH_THREADS];

static inline u64 get_possion(void) {
  u64 u = get_random_u64();
  return (u % (THLET_LOOPS)) + THLET_LOOPS;
}

int thlet_switch_fn(void *arg) {
  int id = (int) arg;
  u64 cnt = 0, pure = 0;
  u64 iter_cnt[THLET_SWITCH_ITER];
  for (int i = 0; i < THLET_SWITCH_ITER; i ++) {
    iter_cnt[i] = i == 0 ? get_possion() : iter_cnt[i - 1] + get_possion();
  }

  // waiting for start
  while (!thlet_switch_start) cpu_relax();

  per_cpu(cpu_thlet_switch_start, THLET_RUNNING_CORE) = true;

  u64 start_cycle = rdtsc();
  for (int i = 0; i < THLET_SWITCH_ITER; i ++) {
    u64 pure_start = rdtsc();
    do {
      asm volatile ("nop");
    } while (likely(cnt ++ <= iter_cnt[i]));

    u64 pure_end = rdtsc();
    pure += pure_end - pure_start;
    yield();
    // schedule();
  }
  u64 end_cycle = rdtsc();

  BUG_ON(id >= THLET_SWITCH_THREADS);
  // commit
  finish_cnt ++;
  stats[id].start = start_cycle;
  stats[id].end = end_cycle;
  stats[id].total = iter_cnt[THLET_SWITCH_ITER - 1];
  stats[id].pure_computation = pure;
  stats[id].finished = true;
  wmb();

  if (finish_cnt == THLET_SWITCH_THREADS) {
    per_cpu(cpu_thlet_switch_start, THLET_RUNNING_CORE) = false;
  }

  return 0;
}

static int thlet_switch_init(void) {
  sched_set_fifo(current);

  per_cpu(cpu_thlet_switch_start, THLET_RUNNING_CORE) = false;

  for (int i = 0; i < THLET_SWITCH_THREADS; i ++) {
    stats[i].finished = false;

    thlet_switch[i] = kthread_create(thlet_switch_fn, (void *)i, "thlet-switch-tester");
    kthread_bind(thlet_switch[i], THLET_RUNNING_CORE);
    wake_up_process(thlet_switch[i]);
  }

  thlet_switch_start = true;
  wmb();
  return 0;
}

static void thlet_switch_exit(void) {
  u64 sum = 0, pure = 0;
  u64 start_cycle = 0, end_cycle = 0;
  for (int i = 0; i < THLET_SWITCH_THREADS; i ++) {
    while (!stats[i].finished) cpu_relax();

    sum += stats[i].total;
    pure += stats[i].pure_computation;

    start_cycle = i == 0 ? stats[i].start : MIN(start_cycle, stats[i].start);
    end_cycle = i == 0 ? stats[i].end : MAX(start_cycle, stats[i].end);
  }

  u64 cost = end_cycle - start_cycle;

  printk("[thlet-switch] thread: %lu, cost %llu cycles, pure %llu%%, sched %llu%%, cs %llu%%, pick: %llu%%\n", THLET_SWITCH_THREADS, 
    cost, pure * 100 / cost, per_cpu(cpu_thlet_switch_stats_sum, THLET_RUNNING_CORE).sched * 100 / cost, 
    per_cpu(cpu_thlet_switch_stats_sum, THLET_RUNNING_CORE).cs * 100 / cost, 
  per_cpu(cpu_thlet_switch_stats_sum, THLET_RUNNING_CORE).pick);

  per_cpu(cpu_thlet_switch_start, THLET_RUNNING_CORE) = false;
  per_cpu(cpu_thlet_switch_stats_sum, THLET_RUNNING_CORE).sched = 0;
  per_cpu(cpu_thlet_switch_stats_sum, THLET_RUNNING_CORE).cs = 0;
  per_cpu(cpu_thlet_switch_stats_sum, THLET_RUNNING_CORE).pick = 0;
  per_cpu(cpu_thlet_switch_stats_sum, THLET_RUNNING_CORE).cs_mm = 0;
  per_cpu(cpu_thlet_switch_stats_sum, THLET_RUNNING_CORE).cs_reg = 0;
}

module_init(thlet_switch_init);
module_exit(thlet_switch_exit);
MODULE_LICENSE("GPL");
