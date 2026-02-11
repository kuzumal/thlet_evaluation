#include <stdio.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <string.h>
#include "const.h"
#include "hash.h"

#include <stdlib.h>
#include <thread>
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

#include <bits/stdc++.h>

#define MAX_THREAD 8
#define MAX_RPC 100000
#define MAX_RECORD 1000
char global_buffer[MAX_RPC][sizeof(Rpc)];
Hash table;

// uint64_t records[MAX_THREAD];
// uint64_t stats[MAX_THREAD][MAX_RECORD + 10];
// uint64_t total;
// std::vector<uint64_t> latencies;
// std::mutex lmtx;
// std::chrono::steady_clock::time_point start;
// bool started = false;

void init() {
  hash_init(&table);
}

void destroy() {
  hash_free(&table);
}

class Stats {
public:
  std::vector<uint64_t> latencies;
  std::mutex mtx;
  std::atomic<uint64_t> total_requests{0};
  std::chrono::steady_clock::time_point start_time, end_time;
  bool started;

  Stats() {
    latencies.reserve(MAX_RPC);
    started = false;
  }

  void start() {
    if (started) return;
    started = true;
    start_time = std::chrono::steady_clock::now();
  }

  void add(uint64_t diff) {
    std::lock_guard<std::mutex> lock(mtx);
    end_time = std::chrono::steady_clock::now();
    latencies.push_back(diff);
    total_requests ++;
  }

  void report() {
    std::chrono::duration<double> elapsed = end_time - start_time;
    
    std::lock_guard<std::mutex> lock(mtx);
    if (latencies.empty()) return;

    std::sort(latencies.begin(), latencies.end());

    size_t n = latencies.size();
    uint64_t p50 = latencies[n * 0.50];
    uint64_t p99 = latencies[n * 0.99];
    double throughput = total_requests / elapsed.count();

    printf("Total Requests: %lu\n", total_requests.load());
    printf("Time Elapsed:   %.2f seconds\n", elapsed.count());
    printf("Throughput:     %.2f rps\n", throughput);
    printf("P50 Latency:    %lu cycles\n", p50);
    printf("P99 Latency:    %lu cycles\n", p99);
  }
};

Stats g_stats;

class ThreadPool {
public:
  ThreadPool(size_t threads) : stop(false) {
      for(size_t i = 0; i < threads; i ++)
        workers.emplace_back([this, i] {
          for(;;) {
            Rpc *rpc = NULL; 
            {
              std::unique_lock<std::mutex> lock(this->queue_mutex);
              this->condition.wait(lock, [this]{ return this->stop || !this->tasks.empty(); });
              if(this->stop && this->tasks.empty()) return;
              
              rpc = this->tasks.front();
              this->tasks.pop();
            }
            uint64_t cost = this->handle_rpc(rpc);
            if (cost) g_stats.add(cost);
          }
        });
  }

  void enqueue(Rpc* rpc) {
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      tasks.push(rpc);
    }
    condition.notify_one();
  }

  ~ThreadPool() {
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      stop = true;
    }
    condition.notify_all();
    for(std::thread &worker: workers) worker.join();
  }

private:
  std::vector<std::thread> workers;
  std::queue<Rpc *> tasks;
  std::mutex queue_mutex;
  std::condition_variable condition;
  bool stop;

  uint64_t handle_rpc(Rpc *rpc) {
    if (!rpc) return 0;
    char value[VALUE_SIZE];
    
    uint64_t driver_ts = be64toh(rpc->timestamp);
    // printf("Received RPC: Magic=0x%lx, Driver Timestamp=%lu cycle diff %lu cycle\n", 
    //         be64toh(rpc->magic), driver_ts, now - driver_ts);

    if (rpc->type == Get) {
      get(rpc->key, value, &table);
    } else if (rpc->type == Put) {
      put(rpc->key, rpc->value, &table);
    }
    uint64_t now = _rdtsc();
    return now - driver_ts;
  }

  // void report(uint64_t cost, int thread_id) {
  //   // printf("Received RPC cycle diff %lu cycle\n", cost);
  //   stats[thread_id][records[thread_id] ++] = cost;
  //   // printf("THREAD #%d receive %d\n", thread_id, records[thread_id]);
  //   if (records[thread_id] == MAX_RECORD) {
  //     printf("REPORT!\n");
  //     records[thread_id] = 0;
  //     std::lock_guard<std::mutex> lock(lmtx);
  //     for (int i = 0; i < MAX_RECORD; i ++) {
  //       latencies.push_back(stats[thread_id][i]);
  //     }
  //     total += MAX_RECORD;
  //     if (total == MAX_RECORD) {
  //       auto end_time = std::chrono::steady_clock::now();
  //       std::chrono::duration<double> elapsed = end_time - start;
  //       std::sort(latencies.begin(), latencies.end());
  //       uint64_t p50 = latencies[total * 0.50];
  //       uint64_t p99 = latencies[total * 0.99];
  //       double throughput = total / elapsed.count();

  //       printf("Total Requests: %lu\n", total);
  //       printf("Time Elapsed:   %.2f seconds\n", elapsed.count());
  //       printf("Throughput:     %.2f rps\n", throughput);
  //       printf("P50 Latency:    %lu cycles\n", p50);
  //       printf("P99 Latency:    %lu cycles\n", p99);
  //     }
  //   }
  // }
};

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

  char *buffer = NULL;
  struct sockaddr_in cliaddr;
  socklen_t len = sizeof(cliaddr);
  int pheader = 0;
  ThreadPool pool(MAX_THREAD);

  while (pheader < MAX_RPC) {
    buffer = global_buffer[pheader];
    int n = recvfrom(sockfd, buffer, sizeof(Rpc), 0, (struct sockaddr *)&cliaddr, &len);
    if (n < 0) continue;
    g_stats.start();

    Rpc *rpc = (Rpc *)buffer;
    pheader ++;
    
    if (be64toh(rpc->magic) == 0x7777) {
      pool.enqueue(rpc);
    }
  }

  sleep(2);
  g_stats.report();

  close(sockfd);
  destroy();
  return 0;
}