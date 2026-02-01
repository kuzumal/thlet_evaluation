#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

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

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <x86intrin.h>
#include "../kernel_mode/thlet_intr.h"

#define PORT 8888
#define MAX_BUFFER 1024
#define TARGET_COUNT 1000

int main() {
  int sockfd, driver_fd;
  driver_fd = open("/dev/thlet_intr", O_RDWR);

  driver_fd = open("/dev/thlet_intr", O_RDWR);
  if (driver_fd < 0) {
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

  char buffer[MAX_BUFFER];
  struct sockaddr_in servaddr, cliaddr;
  socklen_t len;
  int count = 0;

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }

  int flags = fcntl(sockfd, F_GETFL, 0);
  fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

  memset(&servaddr, 0, sizeof(servaddr));
  memset(&cliaddr, 0, sizeof(cliaddr));

  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = INADDR_ANY;
  servaddr.sin_port = htons(PORT);

  if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    perror("Bind failed");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  printf("Listening...\n");

  if (ioctl(driver_fd, IOCTL_CLEAR, getpid()) < 0) {
    perror("IOCTL_CLEAR failed");
    exit(1);
  }

  if (ioctl(driver_fd, IOCTL_STAT_START, getpid()) < 0) {
    perror("IOCTL_STAT_START failed");
    exit(1);
  }

  while (1) {
    len = sizeof(cliaddr);
    uint64_t t0 = _rdtsc();

    ssize_t n; 
    while (1) {
      n = recvfrom(sockfd, (char *)buffer, MAX_BUFFER, 
                          MSG_DONTWAIT, (struct sockaddr *)&cliaddr, &len);
      if (n > 0) break;
    }
    uint64_t t1 = _rdtsc();

    printf("%d : %lu=============\n", count, t1 - t0);
    printf("%lu\n", t1 - info->ts.igb_softirq_exit);
    if (ioctl(driver_fd, IOCTL_GET_STATS, getpid()) < 0) {
      perror("IOCTL_GET_STATS failed");
      exit(1);
    }
    if (ioctl(driver_fd, IOCTL_STAT_START, getpid()) < 0) {
      perror("IOCTL_GET_STATS failed");
      exit(1);
    }
    // printf("pre state %lu\n", info->ps.pre.state);
    // printf("pre common irq entry %lu exit %lu\n", info->ps.pre.common_irq_entry, info->ps.pre.common_irq_exit);
    // printf("pre common softirq entry %lu exit %lu\n", info->ps.pre.common_softirq_entry, info->ps.pre.common_softirq_exit);
    // printf("pre igb softirq entry %lu exit %lu\n", info->ps.pre.igb_softirq_entry, info->ps.pre.igb_softirq_exit);
    // printf("pre igb tcp entry %lu\n", info->ps.pre.tcp_entry);
    // printf("pre igb udp entry %lu exit %lu\n", info->ps.pre.udp_entry, info->ps.pre.udp_exit);
    // printf("pre net_skb1 entry %lu exit %lu\n", info->ps.pre.net_skb1_entry, info->ps.pre.net_skb1_exit);
    // printf("pre net_skb2 entry %lu exit %lu\n", info->ps.pre.net_skb2_entry, info->ps.pre.net_skb2_exit);
    // printf("pre net_skb3 entry %lu exit %lu\n", info->ps.pre.net_skb3_entry, info->ps.pre.net_skb3_exit);
    // printf("pre sched entry %lu\n", info->ps.pre.sched_entry);
    
  }

  munmap(info, PAGE_SIZE);
  close(driver_fd);
  close(sockfd);
  return 0;
}