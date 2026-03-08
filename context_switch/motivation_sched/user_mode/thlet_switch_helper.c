#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include "thlet_switch_helper.h"

static int thlet_switch_helper_mmap(struct file *filp, struct vm_area_struct *vma);
static int thlet_switch_helper_open(struct inode *threadlet_inode, struct file *threadlet_file);
static int thlet_switch_helper_release(struct inode *threadlet_inode, struct file *threadlet_file);
static ssize_t thlet_switch_helper_read(struct file *p_file, char *u_buffer, size_t count, loff_t *ppos);
static ssize_t thlet_switch_helper_write(struct file *p_file, const char *u_buffer, size_t count, loff_t *ppos);
static long thlet_switch_helper_ioctl(struct file *file, unsigned ioctl_num, unsigned long ioctl_param);
static long thlet_switch_helper_ioctl32(struct file *file, unsigned ioctl_num, unsigned long ioctl_param);

struct file_operations thlet_switch_helper_fops = {
  .owner = THIS_MODULE,
  .read = thlet_switch_helper_read,
  .unlocked_ioctl = thlet_switch_helper_ioctl,
  .compat_ioctl = thlet_switch_helper_ioctl32,
  .write = thlet_switch_helper_write,
  .open = thlet_switch_helper_open,
  .release = thlet_switch_helper_release,
  .mmap = thlet_switch_helper_mmap,
};

struct thlet_switch_stats_sum *stats = NULL;

static int thlet_switch_helper_mmap(struct file *filp, struct vm_area_struct *vma) {
  stats = kzalloc(PAGE_SIZE, GFP_KERNEL);
  if (!stats) return -ENOMEM;
  SetPageReserved(virt_to_page(stats));

  unsigned long pfn = virt_to_phys(stats) >> PAGE_SHIFT;
  unsigned long size = vma->vm_end - vma->vm_start;

  if (remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot))
    return -EAGAIN;
  return 0;
}

static int thlet_switch_helper_open(struct inode *p_inode, struct file *p_file) {
  return 0;
}

static int thlet_switch_helper_release(struct inode *p_inode, struct file *p_file) {
  return 0;
}

static long thlet_switch_helper_ioctl32(struct file *file, unsigned ioctl_num, unsigned long ioctl_param) {
  unsigned long param = (unsigned long)((void *)(ioctl_param));

  thlet_switch_helper_ioctl(file, ioctl_num, param);
  return 0;
}

static ssize_t thlet_switch_helper_read(struct file *p_file, char *u_buffer, size_t count, loff_t *ppos) {
  return 0;
}

static ssize_t thlet_switch_helper_write(struct file *p_file, const char *u_buffer, size_t count, loff_t *ppos) {
  return 0;
}

static long thlet_switch_helper_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param) {
  if (ioctl_num == THLET_IOCTL_START) {
    // printk("[thlet-switch-helper] start\n");
    memset(&per_cpu(cpu_thlet_switch_stats_sum, THLET_RUNNING_CORE), 0, sizeof(struct thlet_switch_stats_sum));
    per_cpu(cpu_thlet_switch_start, THLET_RUNNING_CORE) = true;
  } else if (ioctl_num == THLET_IOCTL_FINISH) {
    // printk("[thlet-switch-helper] finish\n");
    per_cpu(cpu_thlet_switch_start, THLET_RUNNING_CORE) = false;
    (*stats) = per_cpu(cpu_thlet_switch_stats_sum, THLET_RUNNING_CORE);
  } else if (ioctl_num == THLET_IOCTL_CLEAR) {
    memset(&per_cpu(cpu_thlet_switch_stats_sum, THLET_RUNNING_CORE), 0, sizeof(struct thlet_switch_stats_sum));
  }
  return 0;
}

dev_t thlet_switch_helper_dev;
struct cdev *thlet_switch_helper_cdev;
struct class *thlet_class;

static int thlet_switch_helper_init(void) {
  printk("thlet_switch_helper initialized");

  thlet_switch_helper_dev = MKDEV(DEV_MAJOR, DEV_MINOR);
  register_chrdev_region(thlet_switch_helper_dev, 1, DEV_NAME);

  thlet_switch_helper_cdev = cdev_alloc();
  thlet_switch_helper_cdev->owner = THIS_MODULE;
  thlet_switch_helper_cdev->ops = &thlet_switch_helper_fops;
  cdev_init(thlet_switch_helper_cdev, &thlet_switch_helper_fops);
  cdev_add(thlet_switch_helper_cdev, thlet_switch_helper_dev, 1);

  thlet_class = class_create("thlet_class");
  device_create(thlet_class, NULL, thlet_switch_helper_dev, NULL, DEV_NAME);

  return 0;
}

static void thlet_switch_helper_exit(void) {
  if (stats) {
    ClearPageReserved(virt_to_page(stats));
    kfree(stats);
  }

  cdev_del(thlet_switch_helper_cdev);
  unregister_chrdev_region(thlet_switch_helper_dev, 1);

  device_destroy(thlet_class, thlet_switch_helper_dev);
  class_destroy(thlet_class);
  printk("thlet_switch_helper removed");
}

module_init(thlet_switch_helper_init);
module_exit(thlet_switch_helper_exit);
MODULE_LICENSE("GPL");