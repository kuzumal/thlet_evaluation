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

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <x86intrin.h>
#include "../kernel_mode/thlet_intr.h"


#define PORT 8888
#define MAX_EVENTS 10
#define BUF_SIZE 1024

void set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main() {
  int server_fd, epoll_fd;
  struct sockaddr_in address;
  struct epoll_event ev, events[MAX_EVENTS];

  int driver_fd = open("/dev/thlet_intr", O_RDWR);

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

  if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
      perror("socket failed");
      exit(EXIT_FAILURE);
  }

  int timestamp_flags = SOF_TIMESTAMPING_RX_SOFTWARE | 
                        SOF_TIMESTAMPING_SOFTWARE |
                        SOF_TIMESTAMPING_RAW_HARDWARE;
  if (setsockopt(server_fd, SOL_SOCKET, SO_TIMESTAMPING, 
                &timestamp_flags, sizeof(timestamp_flags)) < 0) {
    perror("setsockopt SO_TIMESTAMPING");
  }

  set_nonblocking(server_fd);

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

  if (ioctl(driver_fd, IOCTL_CLEAR, getpid()) < 0) {
    perror("IOCTL_CLEAR failed");
    exit(1);
  }

  if (ioctl(driver_fd, IOCTL_STAT_START, getpid()) < 0) {
    perror("IOCTL_STAT_START failed");
    exit(1);
  }

  printf("Server listening on port %d, starting spinning loop...\n", PORT);

  char buffer[BUF_SIZE];
  struct msghdr msg;
  struct iovec iov;
  char control_buf[512];

  while (1) {
    uint64_t t0 = _rdtsc();

    int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 0);

    if (nfds > 0) {
      asm volatile ("lfence" ::: "memory");

      uint64_t t1 = _rdtsc();

      if (ioctl(driver_fd, IOCTL_GET_STATS, getpid()) < 0) {
        perror("IOCTL_GET_STATS failed");
        exit(1);
      }

      printf("end to end %lu\n", t1 - t0);

      printf("%lu\n", t1 - info->ts.igb_softirq_exit);
      printf("igb soft %lu\n", info->ts.igb_softirq_exit - info->ts.igb_softirq_entry);

      if (ioctl(driver_fd, IOCTL_STAT_START, getpid()) < 0) {
        perror("IOCTL_STAT_START failed");
        exit(1);
      }
    }
  }

  munmap(info, PAGE_SIZE);
  close(server_fd);
  close(driver_fd);
  return 0;
}