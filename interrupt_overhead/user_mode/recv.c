#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <linux/net_tstamp.h>
#include <linux/errqueue.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/user.h>
#define __USE_GNU

#include <pthread.h>
#include <sched.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <x86intrin.h>
#include "../kernel_mode/thlet_intr.h"

#define PORT 8888
#define MAX_EVENTS 10
#define BUF_SIZE 1024
#define NUM_ITERS 1000000
#define VEC_SIZE 100000

uint64_t t7a[VEC_SIZE];
uint64_t t8a[VEC_SIZE];

struct Stats {
  uint64_t pipe_flush;
  uint64_t hardirq;
  uint64_t hardirq2igbsoftirq;
  uint64_t igb_softirq;
  uint64_t net1;
  uint64_t net2;
  uint64_t net3;
  uint64_t deliver;
  uint64_t udp;
  uint64_t tcp;
  uint64_t igbsoftirq2sched;
  uint64_t sched_pre;
  uint64_t sched_pick;
  uint64_t sched_wakeup;
  uint64_t cs_pre;
  uint64_t cs_mm;
  uint64_t cs_reg;
} stats;

uint64_t stats_count;

void set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main() {
  int server_fd, epoll_fd, driver_fd;
  FILE *result_fd;
  struct sockaddr_in address;
  struct epoll_event ev, events[MAX_EVENTS];

  driver_fd = open("/dev/thlet_intr", O_RDWR);
  result_fd = fopen("result.txt", "w");

  if (driver_fd < 0 || result_fd == NULL) {
    perror("Failed to open");
    exit(1);
  }

  // map shared info from kernel
  pipe_ *info = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, driver_fd, 0);
  if (!info) {
    perror("mmap failed");
    close(driver_fd);
    exit(1);
  }

  if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  // int timestamp_flags = SOF_TIMESTAMPING_RX_SOFTWARE | 
  //                       SOF_TIMESTAMPING_SOFTWARE |
  //                       SOF_TIMESTAMPING_RAW_HARDWARE;
  // if (setsockopt(server_fd, SOL_SOCKET, SO_TIMESTAMPING, 
  //                 &timestamp_flags, sizeof(timestamp_flags)) < 0) {
  //   perror("setsockopt SO_TIMESTAMPING");
  // }

  // set_nonblocking(server_fd);

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  epoll_fd = epoll_create1(0);
  ev.events = EPOLLIN;
  ev.data.fd = server_fd;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);

  printf("Server listening on port %d, starting spinning loop...\n", PORT);

  uint64_t count = 0;
  uint64_t common_irq = 0, igb_irq = 0, igb_irq_msi = 0;
  uint64_t common_softirq = 0, igb_softirq = 0;
  uint64_t sched_pre = 0, sched_pick = 0, sched_wakeup = 0;
  uint64_t cs_pre = 0, cs_mm = 0, cs_reg = 0, cs_exit = 0;
  char buf[BUF_SIZE];
  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);

  if (ioctl(driver_fd, IOCTL_CLEAR, getpid()) < 0) {
    perror("IOCTL_CLEAR failed");
    exit(1);
  }

  if (ioctl(driver_fd, IOCTL_STAT_START, getpid()) < 0) {
    perror("IOCTL_STAT_START failed");
    exit(1);
  }
  volatile uint64_t t1, t2, t3, t4, t5, t6, t7, t8;

  while (count < NUM_ITERS) {
    int vc = 0, check = 0; 
    uint64_t t0 = _rdtsc();
    int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 0);

    while (nfds <= 0) {
      t1 = _rdtsc();
      t2 = _rdtsc();
      t3 = _rdtsc();
      t4 = _rdtsc();
      t5 = _rdtsc();
      t6 = _rdtsc();

      if (t2 - t1 > 1000) {
        t7 = t1;
        t8 = t2;
        check = 1;
      } else if (t3 - t2 > 1000) {
        t7 = t2;
        t8 = t3;
        check = 1;
      } else if (t4 - t3 > 1000) {
        t7 = t3;
        t8 = t4;
        check = 1;
      } else if (t5 - t4 > 1000) {
        t7 = t4;
        t8 = t5;
        check = 1;
      } else if (t6 - t5 > 1000) {
        t7 = t5;
        t8 = t6;
        check = 1;
      }

      if (check) {
        if (ioctl(driver_fd, IOCTL_GET_STATS, getpid()) < 0) {
          perror("IOCTL_GET_STATS failed");
          exit(1);
        }
        check = 0;
        t7a[vc] = t7;
        t8a[vc] = t8;
        vc = vc >= VEC_SIZE ? 0 : vc + 1;
        nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 0);
      }
    }

    if (nfds > 0) {
      uint64_t t_end = _rdtsc();
      if (ioctl(driver_fd, IOCTL_GET_STATS, getpid()) < 0) {
        perror("IOCTL_GET_STATS failed");
        exit(1);
      }

      ssize_t len = recvfrom(server_fd, buf, sizeof(buf), 0, 
                             (struct sockaddr *)&client_addr, &addr_len);
      
      if (len < 0) {
        perror("recvfrom failed");
        break;
      }

      count ++;

      // if (info->ps.count == 0) {
      //   if (ioctl(driver_fd, IOCTL_STAT_START, getpid()) < 0) {
      //     perror("IOCTL_STAT_START failed");
      //     exit(1);
      //   }
      //   printf("state :%llu\n", info->ts.state);
      //   printf("softirq %llu\n", info->ts.common_softirq_exit - info->ts.common_softirq_entry);
      //   printf("igb softirq %llu\n", info->ts.igb_softirq_exit - info->ts.igb_softirq_entry);
      //   printf("Still zero\n");
      //   continue;
      // }
      // printf("-------------\n");
      // printf("count: %lu\n", info->ps.count);
      // printf("Average common interrupt handling time: %lu cycles\n", info->ps.common_irq / info->ps.count);
      // printf("Average igb interrupt msi handling time: %lu cycles\n", info->ps.igb_intr_msi / info->ps.count);
      // printf("Average common softirq cost: %lu cycles\n", info->ps.softirq / info->ps.count);
      // printf("Average common irq to common softirq %lu cycles\n", info->ps.cirq2softirq / info->ps.count);
      // printf("Average common irq to igb softirq %lu cycles\n", info->ps.cirq2isoftirq / info->ps.count);
      // printf("Average igb softirq cost: %lu cycles\n", info->ps.igb_softirq / info->ps.count);
      // printf("Average common softirq to sched cost: %lu cycles\n", info->ps.softirq2sched / info->ps.count);
      // printf("Average igb softirq to sched cost: %lu cycles\n", info->ps.isoftirq2sched / info->ps.count);
      // printf("Average sched_pre cost: %lu cycles\n", info->ps.sched_pre / info->ps.count);
      // printf("Average sched_pick cost: %lu cycles\n", info->ps.sched_pick / info->ps.count);
      // printf("Average sched_wakeup cost: %lu cycles\n", info->ps.sched_wakeup / info->ps.count);
      // printf("Average cs_pre cost: %lu cycles\n", info->ps.cs_pre / info->ps.count);
      // printf("Average cs_mm cost: %lu cycles\n", info->ps.cs_mm / info->ps.count);
      // printf("Average cs_reg cost: %lu cycles\n", info->ps.cs_reg / info->ps.count);
      // printf("=============\n");
      printf("user end %lu\n", t_end);
      uint64_t pre = t0, state = -1, ex = 0, to_irq = 0;
      for (int i = 0; i < vc; i ++) {
        if (state == -1 && pre < info->ts.common_irq_entry && t7a[i] > info->ts.common_irq_entry) {
          state = 1;
          to_irq = info->ts.common_irq_entry - pre;
          printf("pre %lu | irq %lu ~ %lu | exit %lu\n", pre, info->ts.common_irq_entry, info->ts.cs_exit, t8a[i]);
          printf("end to end %lu\n", t8a[i] - t7a[i]);
          printf("to_irq %llu\n", info->ts.common_irq_entry - pre);
        } else if (state == 1 && t7a[i] < info->ts.cs_exit && info->ts.cs_exit < t8a[i]) {
          printf("pre %lu | exit %lu | user %lu\n", t7a[i], info->ts.cs_exit, t8a[i]);
          printf("exit to user %lu\n", t8a[i] - info->ts.cs_exit);
          break;
        }
        pre = t7a[i];
        ex = t8a[i];
      }
      if (state == -1) printf("Not found\n");
      // printf("=============\n");
      printf("user t0 %lu\n", t0);
      // printf("sudden common irq entry %lu exit %lu\n", info->ts.common_irq_entry, info->ts.common_irq_exit);
      // printf("sudden common softirq entry %lu exit %lu\n", info->ts.common_softirq_entry, info->ts.common_softirq_exit);
      // printf("sudden igb softirq entry %lu exit %lu\n", info->ts.igb_softirq_entry, info->ts.igb_softirq_exit);
      // printf("sudden igb tcp entry %lu\n", info->ts.tcp_entry);
      // printf("sudden igb udp entry %lu exit %lu\n", info->ts.udp_entry, info->ts.udp_exit);
      // printf("sudden net_skb1 entry %lu exit %lu\n", info->ts.net_skb1_entry, info->ts.net_skb1_exit);
      // printf("sudden net_skb2 entry %lu exit %lu\n", info->ts.net_skb2_entry, info->ts.net_skb2_exit);
      // printf("sudden net_skb3 entry %lu exit %lu\n", info->ts.net_skb3_entry, info->ts.net_skb3_exit);
      // printf("sudden sched entry %lu\n", info->ts.sched_entry);
      printf("=============\n");
      printf("state %lu\n", info->ts.state);
      printf("common irq %lu\n", info->ts.common_irq_exit - info->ts.common_irq_entry);
      printf("common softirq %lu\n", info->ts.common_softirq_exit - info->ts.common_softirq_entry);
      printf("igb softirq %lu\n", info->ts.igb_softirq_exit - info->ts.igb_softirq_entry);
      printf("igb tcp %lu\n", info->ts.tcp_exit - info->ts.tcp_entry);
      printf("igb udp %lu\n", info->ts.udp_exit - info->ts.udp_entry);
      printf("igb deliver %lu\n", info->ts.igb_intr_exit - info->ts.igb_intr_entry);
      printf("deliver->udp %lu udp->end %lu\n", info->ts.udp_entry - info->ts.igb_intr_entry, info->ts.igb_intr_exit - info->ts.udp_exit);
      printf("net_skb1 %lu\n", info->ts.net_skb1_exit - info->ts.net_skb1_entry);
      printf("net_skb2 %lu to end %lu\n", info->ts.net_skb2_exit - info->ts.net_skb2_entry, info->ts.net_skb1_exit - info->ts.net_skb2_exit);
      printf("net_skb3 %lu from net2 %lu to udp %lu\n", info->ts.net_skb3_exit - info->ts.net_skb3_entry,
        info->ts.net_skb3_entry - info->ts.net_skb2_exit, info->ts.igb_intr_entry - info->ts.net_skb3_exit);
      printf("sched entry %lu\n", info->ts.sched_entry);

      printf("igb softirq %lu\n", info->ts.igb_softirq_exit - info->ts.igb_softirq_entry);
      
      printf("-------------\n");

      // stats
      if (info->ts.igb_softirq_entry < info->ts.net_skb1_entry && info->ts.net_skb1_entry < info->ts.igb_softirq_exit) {
        // protocol
        stats.pipe_flush = to_irq < 2000 ? to_irq : 0;
        stats.hardirq = info->ts.common_irq_exit - info->ts.common_irq_entry;
        stats.hardirq2igbsoftirq = info->ts.igb_softirq_entry - info->ts.common_irq_exit;
        stats.net1 = info->ts.igb_softirq_entry < info->ts.net_skb1_entry 
                  && info->ts.net_skb1_entry < info->ts.igb_softirq_exit ?
                    info->ts.net_skb1_exit - info->ts.net_skb1_entry : 0;
        stats.net2 = info->ts.igb_softirq_entry < info->ts.net_skb2_entry 
                  && info->ts.net_skb2_entry < info->ts.igb_softirq_exit ?
                    info->ts.net_skb2_exit - info->ts.net_skb2_entry : 0;
        stats.net3 = info->ts.igb_softirq_entry < info->ts.net_skb3_entry 
                  && info->ts.net_skb3_entry < info->ts.igb_softirq_exit ?
                    info->ts.net_skb3_exit - info->ts.net_skb3_entry : 0;
        // using the reserve slots
        stats.deliver = info->ts.igb_intr_exit - info->ts.igb_intr_entry;
        stats.udp = info->ts.udp_exit - info->ts.udp_entry;
        stats.tcp = info->ts.tcp_exit - info->ts.tcp_entry;
        stats.igb_softirq = info->ts.igb_softirq_exit - info->ts.igb_softirq_entry;
        stats.igbsoftirq2sched = info->ts.sched_entry > info->ts.igb_softirq_exit ? info->ts.sched_entry - info->ts.igb_softirq_exit : 0;
        stats.sched_pre = info->ts.pick_entry - info->ts.sched_entry;
        stats.sched_pick = info->ts.pick_exit - info->ts.pick_entry;
        stats.sched_wakeup = info->ts.cs_entry - info->ts.pick_exit;
        stats.cs_pre = info->ts.cs_mm - info->ts.cs_entry;
        stats.cs_mm = info->ts.cs_reg - info->ts.cs_mm;
        stats.cs_reg = info->ts.cs_exit - info->ts.cs_reg;
        
        fprintf(result_fd, "{%lu %lu %lu | %lu %lu %lu %lu %lu %lu %lu | %lu %lu %lu %lu %lu %lu %lu}\n", 
          stats.pipe_flush, stats.hardirq, stats.hardirq2igbsoftirq, 
          stats.igb_softirq, stats.net1, stats.net2, stats.net3, stats.udp, stats.tcp, stats.deliver,
          stats.igbsoftirq2sched, 
          stats.sched_pre, 
          stats.sched_pick, stats.sched_wakeup, stats.cs_pre, stats.cs_mm, stats.cs_reg);
        fflush(result_fd);
      }

      if (ioctl(driver_fd, IOCTL_STAT_START, getpid()) < 0) {
        perror("IOCTL_STAT_START failed");
        exit(1);
      }

      if (ioctl(driver_fd, IOCTL_CLEAR, getpid()) < 0) {
        perror("IOCTL_CLEAR failed");
        exit(1);
      }
      
    }
  }

  // printf("Average common interrupt handling time: %lu cycles\n", info->ps.common_irq / info->ps.count);
  // printf("Average igb interrupt handling time: %lu cycles\n", info->ps.igb_intr / info->ps.count);
  // printf("Average igb interrupt msi handling time: %lu cycles\n", info->ps.igb_intr_msi / info->ps.count);
  // printf("Average common softirq cost: %lu cycles\n", info->ps.common_irq / info->ps.count);
  // printf("Average igb softirq cost: %lu cycles\n", info->ps.igb_softirq / info->ps.count);
  // printf("Average sched_pre cost: %lu cycles\n", info->ps.sched_pre / info->ps.count);
  // printf("Average sched_pick cost: %lu cycles\n", info->ps.sched_pick / info->ps.count);
  // printf("Average sched_wakeup cost: %lu cycles\n", info->ps.sched_wakeup / info->ps.count);
  // printf("Average cs_pre cost: %lu cycles\n", info->ps.cs_pre / info->ps.count);
  // printf("Average cs_mm cost: %lu cycles\n", info->ps.cs_mm / info->ps.count);
  // printf("Average cs_reg cost: %lu cycles\n", info->ps.cs_reg / info->ps.count);
  // printf("Average cs_exit cost: %lu cycles\n", cs_exit / count);
  

  munmap(info, PAGE_SIZE);
  fclose(result_fd);
  close(server_fd);
  close(driver_fd);
  return 0;
}