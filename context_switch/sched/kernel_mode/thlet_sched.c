#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include "thlet_sched.h"
#include <linux/thlet_sched_stats.h>

static int thlet_sched_mmap(struct file *filp, struct vm_area_struct *vma);
static int thlet_sched_open(struct inode *threadlet_inode, struct file *threadlet_file);
static int thlet_sched_release(struct inode *threadlet_inode, struct file *threadlet_file);
static ssize_t thlet_sched_read(struct file *p_file, char *u_buffer, size_t count, loff_t *ppos);
static ssize_t thlet_sched_write(struct file *p_file, const char *u_buffer, size_t count, loff_t *ppos);
static long thlet_sched_ioctl(struct file *file, unsigned ioctl_num, unsigned long ioctl_param);
static long thlet_sched_ioctl32(struct file *file, unsigned ioctl_num, unsigned long ioctl_param);

struct file_operations thlet_sched_fops = {
  .owner = THIS_MODULE,
  .read = thlet_sched_read,
  .unlocked_ioctl = thlet_sched_ioctl,
  .compat_ioctl = thlet_sched_ioctl32,
  .write = thlet_sched_write,
  .open = thlet_sched_open,
  .release = thlet_sched_release,
  .mmap = thlet_sched_mmap,
};

ThletSchedStats *stats = NULL;

static int thlet_sched_mmap(struct file *filp, struct vm_area_struct *vma) {
  stats = kzalloc(PAGE_SIZE, GFP_KERNEL);
  if (!stats) return -ENOMEM;
  SetPageReserved(virt_to_page(stats));

  unsigned long pfn = virt_to_phys(stats) >> PAGE_SHIFT;
  unsigned long size = vma->vm_end - vma->vm_start;

  if (remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot))
    return -EAGAIN;
  return 0;
}

static int thlet_sched_open(struct inode *p_inode, struct file *p_file) {
  return 0;
}

static int thlet_sched_release(struct inode *p_inode, struct file *p_file) {
  return 0;
}

static long thlet_sched_ioctl32(struct file *file, unsigned ioctl_num, unsigned long ioctl_param) {
  unsigned long param = (unsigned long)((void *)(ioctl_param));

  thlet_sched_ioctl(file, ioctl_num, param);
  return 0;
}

static ssize_t thlet_sched_read(struct file *p_file, char *u_buffer, size_t count, loff_t *ppos) {
  return 0;
}

static ssize_t thlet_sched_write(struct file *p_file, const char *u_buffer, size_t count, loff_t *ppos) {
  return 0;
}

static long thlet_sched_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param) {
  thlet_sched_stats_report();
  return 0;
}

dev_t thlet_sched_dev;
struct cdev *thlet_sched_cdev;

static int thlet_sched_init(void) {
  printk("thlet_sched initialized");

  thlet_sched_dev = MKDEV(DEV_MAJOR, DEV_MINOR);
  register_chrdev_region(thlet_sched_dev, 1, DEV_NAME);

  thlet_sched_cdev = cdev_alloc();
  thlet_sched_cdev->owner = THIS_MODULE;
  thlet_sched_cdev->ops = &thlet_sched_fops;
  cdev_init(thlet_sched_cdev, &thlet_sched_fops);
  cdev_add(thlet_sched_cdev, thlet_sched_dev, 1);

  return 0;
}

static void thlet_sched_exit(void) {
  if (stats) {
    ClearPageReserved(virt_to_page(stats));
    kfree(stats);
  }

  cdev_del(thlet_sched_cdev);
  unregister_chrdev_region(thlet_sched_dev, 1);

  printk("thlet_sched removed");
}

module_init(thlet_sched_init);
module_exit(thlet_sched_exit);
MODULE_LICENSE("GPL");