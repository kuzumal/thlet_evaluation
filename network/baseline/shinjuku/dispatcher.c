#include <linux/types.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/smp.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/thlet_rpc.h>
#include "common.h"
#include "dispatcher.h"
#include "worker.h"
#include "context.h"

DEFINE_PER_CPU(uint8_t, worker_active);
DEFINE_PER_CPU(uint64_t, shinjuku_timestamps);
DEFINE_PER_CPU(uint8_t, preempt_check);
DEFINE_PER_CPU(struct worker_response, worker_responses);
DEFINE_PER_CPU(struct dispatcher_request, dispatcher_requests);

DEFINE_KFIFO(tskq, struct worker_response, 4096);

static inline void timestamp_init(void) {
  int cpu;
  for_each_possible_cpu(cpu) {
    per_cpu(shinjuku_timestamps, cpu) = MAX_UINT64;
  }
}

static inline void preempt_check_init(void) {
  int cpu;
  for_each_possible_cpu(cpu) {
    per_cpu(preempt_check, cpu) = false;
  }
}

static inline void request_init(void) {
  int cpu;
  for_each_possible_cpu(cpu) {
    struct dispatcher_request *request = &per_cpu(dispatcher_requests, cpu);
    request->flag = WAITING;
    request->category = 7;
  }
}

static inline void preempt_worker(int i, uint64_t cur_time) {
  return;
  if (per_cpu(preempt_check, i) && (cur_time - per_cpu(shinjuku_timestamps, i) > PREEMPTION_DELAY)) {
    // Avoid preempting more times.
    per_cpu(preempt_check, i) = false;
    debug("[shinjuku-disp] preempt worker-%d req %d\n", i, per_cpu(dispatcher_requests.id, i));
    smp_call_function_single(i, (void * )(void *)worker_ipi_handler, NULL, 0);
  }
}

static inline void handle_finished(int i) {
  static uint64_t finished = 0;
  struct worker_response *response = &per_cpu(worker_responses, i);
  // debug("[shinjuku-disp] handling finished %lld\n", response->id);
  context_free(response->rnbl);
  #ifndef SHINJUKU_SIMULATION
  dev_consume_skb_any(response->mbuf);
  #endif
  per_cpu(preempt_check, i) = false;
  response->flag = PROCESSED;

#ifdef SHINJUKU_REPORT
  thlet_stats_add(finished ++);
  if (finished == 2000) {
    thlet_stats_report();
  }
#endif
}

static inline void handle_preempted(int i) {
  struct worker_response *response = &per_cpu(worker_responses, i);
  debug("[shinjuku-disp] handling preempted %lld\n", response->id);
  kfifo_in(&tskq, response, 1);
  per_cpu(preempt_check, i) = false;
  per_cpu(worker_responses, i).flag = PROCESSED;
}

static inline void dispatch_request(int worker, uint64_t cur_time) {
  // debug("[shinjuku-disp] dispatching request\n");
  static uint64_t dispatched = 0;
  struct dispatcher_request *request = &per_cpu(dispatcher_requests, worker);

  struct worker_response req;
  if (!kfifo_out(&tskq, &req, 1)) {
    return;
  }
  BUG_ON(request->flag != WAITING);

  per_cpu(worker_responses, worker).flag = RUNNING;
  request->rnbl = req.rnbl;
  request->mbuf = req.mbuf;
  request->type = req.type;
#ifdef SHINJUKU_SIMULATION
  request->id = req.id;
#endif
  request->category = req.category;
  request->timestamp = cur_time;
  per_cpu(shinjuku_timestamps, worker) = cur_time;
  per_cpu(preempt_check, worker) = true;
  request->flag = ACTIVE;

  // debug("[shinjuku-disp] dispatch packet %lld to worker-%d\n", request->id, worker);
}

static inline void handle_worker(int i, uint64_t cur_time) {
  struct worker_response *response = &per_cpu(worker_responses, i);
  if (response->flag != RUNNING) {
    if (response->flag == FINISHED) {
      handle_finished(i);
    } else if (response->flag == PREEMPTED) {
      handle_preempted(i);
    }
    dispatch_request(i, cur_time);
  } else
  preempt_worker(i, cur_time);
}

static inline void handle_networker(uint64_t cur_time) {
  void *data;
  uint32_t ret;
  kcontext_t *cont;
  uint32_t max_batch = 2;
  struct worker_response request;
  static uint64_t get = 0;

  while ((data = shinjuku_rpc_get()) && max_batch --) {
    ret = context_alloc(&cont);
    if (unlikely(ret)) {
      printk("[shinjuku-dispatcher] Cannot allocate context\n");
      continue;
    }
    // debug("[shinjuku-disp] get %lld\n", get);

    request.rnbl = cont;
    request.mbuf = data;
    request.type = 0;
#ifdef SHINJUKU_SIMULATION
    request.id = get ++;
#endif
    request.category = PACKET;
    request.timestamp = cur_time;
    kfifo_in(&tskq, &request, 1);
  }
}

#ifdef SHINJUKU_SIMULATION

#define SIM_PACKETS   5000
#define PACKETS_LEN   500
struct sk_buff shinjuku_skb[SIM_PACKETS];
struct iphdr shinjuku_packets[SIM_PACKETS][PACKETS_LEN];

int do_simulate(void* arg) {
#ifdef SHINJUKU_REPORT
  thlet_stats_start(false);
#endif
  for (int i = 0; i < SIM_PACKETS; i ++) {
    struct sk_buff *skb = &shinjuku_skb[i];
    struct iphdr *iph = (struct iphdr *)shinjuku_packets[i];
    skb->data = (unsigned char *)iph;
    skb->len = PACKETS_LEN;

    iph->version = 4;
    iph->ihl = 5;
    iph->protocol = IPPROTO_UDP;
    iph->tot_len = htons(PACKETS_LEN);

    struct udphdr *udph = (struct udphdr *)((unsigned char *)iph + (iph->ihl * 4));
    udph->source = htons(1234);
    udph->dest = htons(8888);
    uint16_t udp_total_len = PACKETS_LEN - (iph->ihl * 4);
    udph->len = htons(udp_total_len);
    unsigned char *payload = (unsigned char *)udph + sizeof(struct udphdr);
    Rpc *rpc = (Rpc *)payload;
    rpc->magic = 0x7777;
    rpc->type = i % 200 == 0 ? 10000 : 0;

    shinjuku_rpc_put(skb);
#ifdef SHINJUKU_REPORT
    thlet_stats_record(i);
#endif
  }

  return 0;
}
#endif

/**
 * do_dispatching - implements dispatcher core's main loop
 */
int do_dispatching(void *arg) {
  int i, ret;
  uint64_t cur_time;
  struct task_struct *worker[MAX_WORKERS];
  struct task_struct *simulator;
  uint32_t num_cpus = num_online_cpus();
  #ifdef SHINJUKU_SIMULATION
  // num_cpus = 3;
  #endif

  // We need at least 1 dispatcher and 1 worker, and 1 for the networker
  BUG_ON(num_cpus < 3);

  timestamp_init();
  preempt_check_init();
  request_init();
  ret = context_init();
  if (ret) {
    printk("[shinjuku-disp] context init fail\n");
    return 0;
  }

  for (i = 2; i < num_cpus; i++) {
    worker[i] = kthread_create(do_work, "shinjuku-worker", "shinjuku-worker");
    if (!IS_ERR(worker[i])) {
      kthread_bind(worker[i], i);
      wake_up_process(worker[i]);
    } else {
      printk(KERN_ERR "Failed to create Shinjuku worker thread for CPU %d\n", i);
      return PTR_ERR(worker[i]);
    }
  }

  #ifdef SHINJUKU_SIMULATION
  simulator = kthread_create(do_simulate, "shinjuku-worker", "shinjuku-worker");
  if (!IS_ERR(simulator)) {
    kthread_bind(simulator, 0);
    wake_up_process(simulator);
  } else {
    printk(KERN_ERR "Failed to create Shinjuku worker thread for CPU %d\n", i);
    return PTR_ERR(simulator);
  }
  #endif

  while(1) {
    cur_time = shinjuku_rdtsc();
    for (i = 2; i < num_cpus; i++)
      handle_worker(i, cur_time);
    handle_networker(cur_time);
  }

  return 0; // Never reached
}