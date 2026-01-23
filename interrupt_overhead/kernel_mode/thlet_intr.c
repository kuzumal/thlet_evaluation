#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/printk.h>
#include <linux/mm.h>
#include <linux/pid.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>

#include <linux/kernel.h>
#include <linux/percpu.h>
#include <asm/irq_vectors.h>

#include <asm/apic.h>

#include <asm/io.h> //virt_to_phys
#include "thlet_intr.h"


MODULE_LICENSE("GPL");

static int threadlet_mmap(struct file *filp, struct vm_area_struct *vma);
static int threadlet_open(struct inode *threadlet_inode, struct file *threadlet_file);
static int threadlet_release(struct inode *threadlet_inode, struct file *threadlet_file);
static ssize_t threadlet_read(struct file *p_file, char *u_buffer, size_t count, loff_t *ppos);
static ssize_t threadlet_write(struct file *p_file, const char *u_buffer, size_t count, loff_t *ppos);
static long threadlet_ioctl(struct file *file, unsigned ioctl_num, unsigned long ioctl_param);
static long threadlet_ioctl32(struct file *file, unsigned ioctl_num, unsigned long ioctl_param);
static irqreturn_t thlet_intr_handler(int irq, void *dev_id);

static u64 ipi_ts = 0;
pipe_ *info = NULL;
dev_t thlet_intr_dev;
struct cdev *thlet_intr_cdev;

struct file_operations thlet_intr_fops = {
  .owner = THIS_MODULE,
  .read = threadlet_read,
  .unlocked_ioctl = threadlet_ioctl,
  .compat_ioctl = threadlet_ioctl32,
  .write = threadlet_write,
  .open = threadlet_open,
  .release = threadlet_release,
  .mmap = threadlet_mmap,
};

static int threadlet_mmap(struct file *filp, struct vm_area_struct *vma) {
  info = kzalloc(PAGE_SIZE, GFP_KERNEL);
  if (!info) return -ENOMEM;
  SetPageReserved(virt_to_page(info));

  unsigned long pfn = virt_to_phys(info) >> PAGE_SHIFT;
  unsigned long size = vma->vm_end - vma->vm_start;

  if (remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot))
    return -EAGAIN;
  return 0;
}

static int threadlet_open(struct inode *p_inode, struct file *p_file) {
  return 0;
}

static int threadlet_release(struct inode *p_inode, struct file *p_file) {
  return 0;
}

void handler(void *arg) {
  ipi_ts = rdtsc();
  info->ts.intr = ipi_ts;
  while (ipi_ts < rdtsc() + 2000)
    ;
}

static long threadlet_ioctl32(struct file *file, unsigned ioctl_num, unsigned long ioctl_param) {
  unsigned long param = (unsigned long)((void *)(ioctl_param));

  threadlet_ioctl(file, ioctl_num, param);
  return 0;
}

static long threadlet_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param) {
  printk(KERN_INFO "thlet_intr: ioctl_num = %u, ioctl_param = %u\n", ioctl_num, ioctl_param);

  if (ioctl_param == 0) {
    printk(KERN_ERR "thlet_intr: ioctl_param is NULL\n");
    return -EINVAL;
  }

  // sendipi as a ioctl
  if (ioctl_num == IOCTL_SENDIPI) {
    smp_call_function_single(RECV_CORE, &handler, NULL, 1);
  }
  else if (ioctl_num == IOCTL_TEST) {
    printk(KERN_INFO "thlet_intr: TEST IOCTL called\n");
  }
  else { // unknown command
    return 1;
  }
  return 1;
}

static ssize_t threadlet_read(struct file *p_file, char *u_buffer, size_t count, loff_t *ppos) {
  return 0;
}

static ssize_t threadlet_write(struct file *p_file, const char *u_buffer, size_t count, loff_t *ppos) {
  return 0;
}

static int threadlet_init(void) {
  printk(KERN_INFO "thlet_intr initialized");

  thlet_intr_dev = MKDEV(DEV_MAJOR, DEV_MINOR);
  register_chrdev_region(thlet_intr_dev, 1, DEV_NAME);

  thlet_intr_cdev = cdev_alloc();
  thlet_intr_cdev->owner = THIS_MODULE;
  thlet_intr_cdev->ops = &thlet_intr_fops;
  cdev_init(thlet_intr_cdev, &thlet_intr_fops);
  cdev_add(thlet_intr_cdev, thlet_intr_dev, 1);

  return 0;
}

static void threadlet_exit(void) {

  if (info) {
    ClearPageReserved(virt_to_page(info));
    kfree(info);
  }

  cdev_del(thlet_intr_cdev);
  unregister_chrdev_region(thlet_intr_dev, 1);

  printk(KERN_INFO "thlet_intr removed");
}

module_init(threadlet_init);
module_exit(threadlet_exit);
