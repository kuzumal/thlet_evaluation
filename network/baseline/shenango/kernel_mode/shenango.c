#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>


static int __init shenango_init(void) {
  printk(KERN_INFO "shenango module loaded\n");
  return 0;
}

static void __exit shenango_exit(void) {
  printk(KERN_INFO "shenango module unloaded\n");
}

module_init(shenango_init);
module_exit(shenango_exit);
MODULE_LICENSE("GPL");