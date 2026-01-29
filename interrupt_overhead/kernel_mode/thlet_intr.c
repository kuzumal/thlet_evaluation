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
#include <linux/sched.h>

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
  info->ts.intr = rdtsc();
  uint64_t until = info->ts.intr + 1000;
  int i;
  for (i = 0; i < 1000000; i++) {
    if (rdtsc() >= until) {
      break;
    }
  }
  wmb();
  info->ts.back = rdtsc();
  // set_tsk_need_resched(current);
  // set_preempt_need_resched();
  __this_cpu_write(cpu_thlet_stats.state, 2);
}

static long threadlet_ioctl32(struct file *file, unsigned ioctl_num, unsigned long ioctl_param) {
  unsigned long param = (unsigned long)((void *)(ioctl_param));

  threadlet_ioctl(file, ioctl_num, param);
  return 0;
}

static long threadlet_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param) {

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
  else if (ioctl_num == IOCTL_STAT_START) {
    __this_cpu_write(cpu_thlet_stats.state, 1);
  }
  else if (ioctl_num == IOCTL_GET_STATS_1) {
    info->ts.state = __this_cpu_read(cpu_thlet_stats.state);
    info->ts.common_irq_entry = __this_cpu_read(cpu_thlet_stats.common_irq_entry);
    info->ts.common_irq_exit = __this_cpu_read(cpu_thlet_stats.common_irq_exit);
    info->ts.e1000e_intr_entry = __this_cpu_read(cpu_thlet_stats.e1000e_intr_entry);
    info->ts.e1000e_intr_exit = __this_cpu_read(cpu_thlet_stats.e1000e_intr_exit);
    info->ts.igb_intr_entry = __this_cpu_read(cpu_thlet_stats.igb_intr_entry);
    info->ts.igb_intr_exit = __this_cpu_read(cpu_thlet_stats.igb_intr_exit);
    info->ts.igb_intr_msi_entry = __this_cpu_read(cpu_thlet_stats.igb_intr_msi_entry);
    info->ts.igb_intr_msi_exit = __this_cpu_read(cpu_thlet_stats.igb_intr_msi_exit);
    info->ts.common_softirq_entry = __this_cpu_read(cpu_thlet_stats.common_softirq_entry);
    info->ts.common_softirq_exit = __this_cpu_read(cpu_thlet_stats.common_softirq_exit);
    info->ts.tcp_entry = __this_cpu_read(cpu_thlet_stats.tcp_entry);
    info->ts.tcp_exit = __this_cpu_read(cpu_thlet_stats.tcp_exit);
    info->ts.udp_entry = __this_cpu_read(cpu_thlet_stats.udp_entry);
    info->ts.udp_exit = __this_cpu_read(cpu_thlet_stats.udp_exit);
    info->ts.e1000e_softirq_entry = __this_cpu_read(cpu_thlet_stats.e1000e_softirq_entry);
    info->ts.e1000e_softirq_exit = __this_cpu_read(cpu_thlet_stats.e1000e_softirq_exit);
    info->ts.e1000e_poll_entry = __this_cpu_read(cpu_thlet_stats.e1000e_poll_entry);
    info->ts.e1000e_poll_exit = __this_cpu_read(cpu_thlet_stats.e1000e_poll_exit);
    info->ts.igb_softirq_entry = __this_cpu_read(cpu_thlet_stats.igb_softirq_entry);
    info->ts.igb_softirq_exit = __this_cpu_read(cpu_thlet_stats.igb_softirq_exit);
    info->ts.sched_entry = __this_cpu_read(cpu_thlet_stats.sched_entry);
    info->ts.pick_entry = __this_cpu_read(cpu_thlet_stats.pick_entry);
    info->ts.pick_exit = __this_cpu_read(cpu_thlet_stats.pick_exit);
    info->ts.cs_entry = __this_cpu_read(cpu_thlet_stats.cs_entry);
    info->ts.cs_mm = __this_cpu_read(cpu_thlet_stats.cs_mm);
    info->ts.cs_reg = __this_cpu_read(cpu_thlet_stats.cs_reg);
    info->ts.cs_exit = __this_cpu_read(cpu_thlet_stats.cs_exit);
  }
  else if (ioctl_num == IOCTL_FINISH) {
    __this_cpu_write(cpu_thlet_stats.state, 0);
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

  printk(KERN_INFO "thlet_intr inject stat %d", __this_cpu_read(cpu_thlet_stats.state));

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
