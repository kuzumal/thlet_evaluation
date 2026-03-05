#include <stdio.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <string.h>
#include "const.h"
#include "hash.h"

#include <stdlib.h>
#include <unistd.h>
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

Hash table;

bool handle_rpc(Rpc *rpc, struct sockaddr_in *client_addr, int sockfd) {
  char value[VALUE_SIZE];
  
  uint64_t driver_ts = be64toh(rpc->timestamp);
  uint64_t now = _rdtsc();
  printf("Received RPC: Magic=0x%lx, Driver Timestamp=%lu cycle diff %lu cycle\n", 
          be64toh(rpc->magic), driver_ts, now - driver_ts);

  if (rpc->type == Get) {
    get(rpc->key, value, &table);
    sendto(sockfd, rpc, sizeof(Rpc), 0, (struct sockaddr*)client_addr, sizeof(*client_addr));
    return false;
  } else if (rpc->type == Put) {
    put(rpc->key, rpc->value, &table);
    sendto(sockfd, rpc, sizeof(Rpc), 0, (struct sockaddr*)client_addr, sizeof(*client_addr));
    return false;
  }
  return true;
}

void init() {
  hash_init(&table);
}

void destroy() {
  hash_free(&table);
}

int main () {
  init();

  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
      perror("Socket creation failed");
      return -1;
  }

  int opt = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in servaddr;
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = INADDR_ANY;
  servaddr.sin_port = htons(PORT);

  if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    perror("Bind failed");
    return -1;
  }

  printf("UDP RPC Server started on port %d...\n", PORT);

  char buffer[sizeof(Rpc)];
  struct sockaddr_in cliaddr;
  socklen_t len = sizeof(cliaddr);

  while (true) {
    int n = recvfrom(sockfd, buffer, sizeof(Rpc), 0, (struct sockaddr *)&cliaddr, &len);
    if (n < 0) continue;

    Rpc *rpc = (Rpc *)buffer;
    
    if (be64toh(rpc->magic) == 0x7777) {
      if (handle_rpc(rpc, &cliaddr, sockfd)) {
        break;
      }
    } else {
      printf("Invalid Magic: 0x%lx\n", be64toh(rpc->magic));
    }
  }

  close(sockfd);
  destroy();
  return 0;
}