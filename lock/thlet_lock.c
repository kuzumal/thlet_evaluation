#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/thlet_rpc.h>
#include <linux/thlet_hash.h>
#include <linux/thlet_lock_stats.h>

static struct mutex test_mutex;
static struct task_struct *task_a, *task_b;
static int a = 0, b = 0;

static int thread_b_fn(void *data) {
  mutex_lock(&test_mutex);

  thlet_lock_stats_record_acquire_lock();
  thlet_lock_stats_report();
  
  mutex_unlock(&test_mutex);
  return 0;
}

static int thread_a_fn(void *data) {
  mutex_lock(&test_mutex);
  
  // udelay(1);
  thlet_lock_stats_record_unlock(); 
  barrier();
  mutex_unlock(&test_mutex);

  return 0;
}

static int __init mutex_bench_init(void) {
  mutex_init(&test_mutex);
  a = 0;
  b = 0;
  task_a = kthread_create(thread_a_fn, NULL, "mutex_bench_a");
  kthread_bind(task_a, 0);
  
  task_b = kthread_create(thread_b_fn, NULL, "mutex_bench_b");
  kthread_bind(task_b, 1);

  wake_up_process(task_a);
  wake_up_process(task_b);
  return 0;
}

static void __exit mutex_bench_exit(void) {
  // if (task_a) {
  //   kthread_stop(task_a);
  // }
  // if (task_b) {
  //   kthread_stop(task_b);
  // }
}

module_init(mutex_bench_init);
module_exit(mutex_bench_exit);
MODULE_LICENSE("GPL");