#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include "const.h"

#define SERVER_IP "172.16.0.2"
#define RPC_MAGIC 0x7777

typedef struct {
  struct timespec send_time;
  struct timespec recv_time;
  double latency_ms;
  int received;
} LatencyRecord;

LatencyRecord *records;
int g_max_rpc;
double g_target_pps;
int sockfd;

double get_elapsed_ms(struct timespec start, struct timespec end) {
  return (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
}

void *receive_thread(void *arg) {
  struct sockaddr_in from_addr;
  socklen_t addr_len = sizeof(from_addr);
  Rpc resp;
  struct timespec now;

  while (1) {
    ssize_t len = recvfrom(sockfd, &resp, sizeof(Rpc), 0, (struct sockaddr*)&from_addr, &addr_len);
    if (len > 0) {
      clock_gettime(CLOCK_MONOTONIC, &now);
      uint32_t id = be32toh(resp.id);
      if (id < g_max_rpc) {
        records[id].recv_time = now;
        records[id].latency_ms = get_elapsed_ms(records[id].send_time, now);
        records[id].received = 1;
      }
    }
  }
  return NULL;
}

int compare_latency(const void *a, const void *b) {
  double diff = ((LatencyRecord *)a)->latency_ms - ((LatencyRecord *)b)->latency_ms;
  return (diff > 0) - (diff < 0);
}

int main(int argc, char **argv) {
  if (argc < 3) {
    printf("Usage: %s <total_rpc> <target_pps>\n", argv[1]);
    return 1;
  }

  g_max_rpc = atoi(argv[1]);
  g_target_pps = atof(argv[2]);
  records = calloc(g_max_rpc, sizeof(LatencyRecord));

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in serv_addr = {
    .sin_family = AF_INET,
    .sin_port = htons(PORT),
  };
  inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

  pthread_t recv_tid;
  pthread_create(&recv_tid, NULL, receive_thread, NULL);

  Rpc rpc = { .magic = htobe64(RPC_MAGIC), .type = htobe32(Put) };
  strncpy(rpc.key, "test", KEY_SIZE);

  printf("Starting test: %d RPCs at %.2f PPS\n", g_max_rpc, g_target_pps);

  struct timespec start_test, current_send;
  clock_gettime(CLOCK_MONOTONIC, &start_test);

  // --- 发送循环：速率控制 ---
  double interval_ns = 1e9 / g_target_pps; 
  for (int i = 0; i < g_max_rpc; i++) {
    rpc.id = htobe32(i);
    
    clock_gettime(CLOCK_MONOTONIC, &records[i].send_time);
    sendto(sockfd, &rpc, sizeof(Rpc), 0, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

    // 计算下一次发送应该在什么时间点，实现精准控速
    struct timespec next_send = start_test;
    long long total_ns = (long long)((i + 1) * interval_ns);
    next_send.tv_sec += total_ns / 1000000000L;
    next_send.tv_nsec += total_ns % 1000000000L;
    if (next_send.tv_nsec >= 1000000000L) {
      next_send.tv_sec ++;
      next_send.tv_nsec -= 1000000000L;
    }
    
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_send, NULL);
  }

  // 等待最后一批回包
  printf("Waiting for responses...\n");
  sleep(2); 

  // --- 统计分析 ---
  int received_count = 0;
  for (int i = 0; i < g_max_rpc; i++) {
    if (records[i].received) received_count++;
  }

  qsort(records, g_max_rpc, sizeof(LatencyRecord), compare_latency);

  if (received_count > 0) {
    printf("\n--- Results ---\n");
    printf("Success Rate: %.2f%% (%d/%d)\n", (double)received_count/g_max_rpc*100, received_count, g_max_rpc);
    printf("P50 Latency:  %.3f ms\n", records[(int)(received_count * 0.50)].latency_ms);
    printf("P90 Latency:  %.3f ms\n", records[(int)(received_count * 0.90)].latency_ms);
    printf("P99 Latency:  %.3f ms\n", records[(int)(received_count * 0.99)].latency_ms);
  }

  close(sockfd);
  free(records);
  return 0;
}