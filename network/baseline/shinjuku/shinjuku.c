#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include "dispatcher.h"
#include "worker.h"

struct task_struct *dispatcher;

static int __init shinjuku_init(void) {
  printk(KERN_INFO "Shinjuku module loaded\n");

  dispatcher = kthread_create(do_dispatching, "Shinjuku", "Shinjuku");
  if (!IS_ERR(dispatcher)) {
    kthread_bind(dispatcher, 1);
    wake_up_process(dispatcher);
  } else {
    printk(KERN_ERR "Failed to create Shinjuku dispatcher thread\n");
    return PTR_ERR(dispatcher);
  }

  return 0;
}

static void __exit shinjuku_exit(void) {
  printk(KERN_INFO "Shinjuku module unloaded\n");
}

module_init(shinjuku_init);
module_exit(shinjuku_exit);
MODULE_LICENSE("GPL");