#include <linux/types.h>
#include <linux/module.h>
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

DEFINE_KFIFO(tskq, struct worker_response, 128);

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

static inline void preempt_worker(int i, uint64_t cur_time) {
  if (per_cpu(preempt_check, i) && (cur_time - per_cpu(shinjuku_timestamps, i) > PREEMPTION_DELAY)) {
    // Avoid preempting more times.
    per_cpu(preempt_check, i) = false;
    smp_call_function_single(i, (void * )(void *)worker_ipi_handler, NULL, 1);
  }
}

static inline void handle_finished(int i) {
  struct worker_response *response = &per_cpu(worker_responses, i);
  context_free(response->rnbl);
  dev_consume_skb_any(response->mbuf);
  per_cpu(preempt_check, i) = false;
  response->flag = PROCESSED;
}

static inline void handle_preempted(int i) {
  struct worker_response *response = &per_cpu(worker_responses, i);
  kfifo_in(&tskq, response, 1);
  per_cpu(preempt_check, i) = false;
  per_cpu(worker_responses, i).flag = PROCESSED;
}

static inline void dispatch_request(int worker, uint64_t cur_time) {
  struct dispatcher_request *request = &per_cpu(dispatcher_requests, worker);

  struct worker_response req;
  if (!kfifo_out(&tskq, &req, 1)) {
    return;
  }
  per_cpu(worker_responses, worker).flag = RUNNING;
  request->rnbl = req.rnbl;
  request->mbuf = req.mbuf;
  request->type = req.type;
  request->category = req.category;
  request->timestamp = cur_time;
  per_cpu(shinjuku_timestamps, worker) = cur_time;
  per_cpu(preempt_check, worker) = true;
  request->flag = ACTIVE;
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
  while (data = shinjuku_rpc_get()) {
    ret = context_alloc(&cont);
    if (unlikely(ret)) {
      printk("[shinjuku-dispatcher] Cannot allocate context\n");
      continue;
    }

    struct worker_response request;
    request.rnbl = cont;
    request.mbuf = data;
    request.type = 0;
    request.category = PACKET;
    request.timestamp = cur_time;
    kfifo_in(&tskq, &request, 1);
  }
}

/**
 * do_dispatching - implements dispatcher core's main loop
 */
int do_dispatching(void *arg) {
  uint32_t num_cpus = (uint32_t) arg;
  int i;
  uint64_t cur_time;

  timestamp_init();
  preempt_check_init();

  while(1) {
    cur_time = shinjuku_rdtsc();
    for (i = 0; i < num_cpus - 1; i++)
      handle_worker(i, cur_time);
    // TODO(qxh): dequeue skb and enqueue to kfifo(with type)
    handle_networker(cur_time);
  }
}