#ifndef THLET_INTR_H
#define THLET_INTR_H
#include <linux/sched.h>

#define DEV_MAJOR 250
#define DEV_MINOR 0
#define DEV_NAME "thlet_intr"
#define RECV_CORE 25

#define IOCTL_NOACTION _IO(DEV_MAJOR, 0)
#define IOCTL_SENDIPI _IO(DEV_MAJOR, 1)
#define IOCTL_REGISTER_HANDLER _IO(DEV_MAJOR, 2)
#define IOCTL_TEST _IO(DEV_MAJOR, 3)
#define IOCTL_STAT_START _IO(DEV_MAJOR, 4)
#define IOCTL_GET_STATS _IO(DEV_MAJOR, 5)
#define IOCTL_CLEAR _IO(DEV_MAJOR, 6)
#define IOCTL_FINISH _IO(DEV_MAJOR, 7)

struct thlet_stats_user {
  uint64_t state;
  uint64_t common_irq_entry;
  uint64_t common_irq_exit;
  uint64_t e1000e_intr_entry;
  uint64_t e1000e_intr_exit;
  uint64_t igb_intr_entry;
  uint64_t igb_intr_exit;
  uint64_t igb_intr_msi_entry;
  uint64_t igb_intr_msi_exit;
  uint64_t common_softirq_entry;
  uint64_t common_softirq_exit;
  uint64_t tcp_entry;
  uint64_t tcp_exit;
  uint64_t udp_entry;
  uint64_t udp_exit;
  uint64_t net_skb1_entry;
  uint64_t net_skb1_exit;
  uint64_t net_skb2_entry;
  uint64_t net_skb2_exit;
  uint64_t net_skb3_entry;
  uint64_t net_skb3_exit;
  uint64_t e1000e_softirq_entry;
  uint64_t e1000e_softirq_exit;
  uint64_t e1000e_poll_entry;
  uint64_t e1000e_poll_exit;
  uint64_t igb_softirq_entry;
  uint64_t igb_softirq_exit;
  uint64_t sched_entry;
  uint64_t pick_entry;
  uint64_t pick_exit;
  uint64_t cs_entry;
  uint64_t cs_mm;
  uint64_t cs_reg;
  uint64_t cs_exit;
};

typedef struct {
  struct {
    int64_t intr;
    int64_t back;

    uint64_t state;
    uint64_t common_irq_entry;
    uint64_t common_irq_exit;
    uint64_t e1000e_intr_entry;
    uint64_t e1000e_intr_exit;
    uint64_t igb_intr_entry;
    uint64_t igb_intr_exit;
    uint64_t igb_intr_msi_entry;
    uint64_t igb_intr_msi_exit;
    uint64_t common_softirq_entry;
    uint64_t common_softirq_exit;
    uint64_t tcp_entry;
    uint64_t tcp_exit;
    uint64_t udp_entry;
    uint64_t udp_exit;
    uint64_t net_skb1_entry;
    uint64_t net_skb1_exit;
    uint64_t net_skb2_entry;
    uint64_t net_skb2_exit;
    uint64_t net_skb3_entry;
    uint64_t net_skb3_exit;
    uint64_t e1000e_softirq_entry;
    uint64_t e1000e_softirq_exit;
    uint64_t e1000e_poll_entry;
    uint64_t e1000e_poll_exit;
    uint64_t igb_softirq_entry;
    uint64_t igb_softirq_exit;
    uint64_t sched_entry;
    uint64_t pick_entry;
    uint64_t pick_exit;
    uint64_t cs_entry;
    uint64_t cs_mm;
    uint64_t cs_reg;
    uint64_t cs_exit;
  } ts;

  struct {
    uint64_t count;
    uint64_t common_irq;
    uint64_t igb_intr;
    uint64_t igb_intr_msi;
    uint64_t cirq2softirq;
    uint64_t iirq2softirq;
    uint64_t cirq2isoftirq;
    uint64_t iirq2isoftirq;
    uint64_t softirq;
    uint64_t igb_softirq;
    uint64_t softirq2sched;
    uint64_t isoftirq2sched;
    uint64_t sched_pre;
    uint64_t sched_pick;
    uint64_t sched_wakeup;
    uint64_t cs_pre;
    uint64_t cs_mm;
    uint64_t cs_reg;

    struct thlet_stats_user pre;
  } ps;
} pipe_;

#endif