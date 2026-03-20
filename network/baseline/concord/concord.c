#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include "dispatcher.h"
#include "worker.h"

struct task_struct *dispatcher;

static int __init concord_init(void) {
  printk(KERN_INFO "concord module loaded\n");

  dispatcher = kthread_create(do_dispatching, "concord", "concord");
  if (!IS_ERR(dispatcher)) {
    kthread_bind(dispatcher, 1);
    wake_up_process(dispatcher);
  } else {
    printk(KERN_ERR "Failed to create concord dispatcher thread\n");
    return PTR_ERR(dispatcher);
  }

  return 0;
}

static void __exit concord_exit(void) {
  printk(KERN_INFO "concord module unloaded\n");
}

module_init(concord_init);
module_exit(concord_exit);
MODULE_LICENSE("GPL");