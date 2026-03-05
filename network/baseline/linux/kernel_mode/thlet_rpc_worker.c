#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/thlet_rpc.h>
#include <linux/thlet_hash.h>

struct task_struct *thlet_rpc_worker;

static inline const char * get_rpc_type(uint32_t type) {
  switch (type) {
    case THLET_PUT: return "PUT";
    case THLET_GET: return "GET";
    case THLET_FINISH: return "FINISH";
    default: return "Unknown";
  }
}

static int thlet_rpc_worker_fn(void *data) {
  Rpc rpc;
  uint32_t bit = 0, nc = 0;
  while (1) {
    while ((!thlet_rpc_get(&rpc)));

    rpc.id = be32_to_cpu(rpc.id);
    rpc.type = be32_to_cpu(rpc.type);

    // printk(KERN_INFO "thlet_rpc_worker: recv rpc id %lu, type %s\n", rpc.id, get_rpc_type(rpc.type));
    // printk(KERN_INFO "thlet_rpc_worker: driver_ts %lu, now %lu\n", thlet_stats_get_driver(rpc.id), thlet_get_cycles());

    if (rpc.type == THLET_GET) {
      thlet_kv_get(rpc.key, rpc.value);
      thlet_stats_add(rpc.id);
      nc ++;
    } else if (rpc.type == THLET_PUT) {
      thlet_kv_put(rpc.key, rpc.value);
      thlet_stats_add(rpc.id);
      nc ++;
    } else if (rpc.type == THLET_SCAN) {
      thlet_kv_scan(rpc.key, rpc.value);
      thlet_stats_add(rpc.id);
      nc ++;
    } else if (rpc.type == THLET_FINISH) {
      thlet_stats_report();
      thlet_stats_start(true);
      nc = 0;
    }

    if (nc == THLET_MAX_SAMPLES) {
      thlet_stats_report();
    }
  }

  return 0;
}

static int __init thlet_rpc_init(void) {
  thlet_rpc_worker = kthread_create(thlet_rpc_worker_fn, "thlet_rpc_worker", "thlet_rpc_worker");
  wake_up_process(thlet_rpc_worker);

  pr_info("Install thlet rpc worker\n");
  return 0;
}

static void __exit thlet_rpc_exit(void) {
  kthread_stop(thlet_rpc_worker);

  pr_info("Remove thlet rpc worker\n");
}

module_init(thlet_rpc_init);
module_exit(thlet_rpc_exit);
MODULE_LICENSE("GPL");