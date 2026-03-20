#include <linux/types.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/smp.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/thlet_rpc.h>
#include "common.h"
#include "dispatcher.h"
#include "worker.h"
#include "context.h"

DEFINE_PER_CPU(uint8_t, worker_active);
DEFINE_PER_CPU(struct jbsq_preemption, preempt_check);
DEFINE_PER_CPU(struct jbsq_worker_response, worker_responses);
DEFINE_PER_CPU(struct jbsq_dispatcher_request, dispatcher_requests);
uint8_t idle_list[MAX_WORKERS];
uint8_t idle_list_head;
struct worker_state dispatcher_states[MAX_WORKERS];
uint64_t time_slice = PREEMPTION_DELAY * CPU_FREQ_GHZ;
uint64_t epoch_slack;
volatile uint64_t *cpu_preempt_points[MAX_WORKERS] = {NULL};

DEFINE_KFIFO(tskq, struct worker_response, 128);

static inline void preempt_check_init(void) {
  int cpu;
  for_each_possible_cpu(cpu) {
    per_cpu(preempt_check, cpu).timestamp = MAX_UINT64;
    per_cpu(preempt_check, cpu).check = false;
  }
}

static void dispatcher_states_init(void) {
	int cpu;
	for_each_possible_cpu(cpu) {
		dispatcher_states[cpu].next_push = 0;
		dispatcher_states[cpu].next_pop = 0;
		dispatcher_states[cpu].occupancy = 0;
    idle_list[cpu] = cpu;
	}
  idle_list_head = 0;
}

static void requests_init(void) {
	int cpu;
	for_each_possible_cpu(cpu) {
		for(uint8_t j = 0; j < JBSQ_LEN; j++) {
  		per_cpu(dispatcher_requests, cpu).requests[j].flag = INACTIVE;
    }
	}
}

static inline void concord_preempt_worker(int i, uint64_t cur_time) {
  uint64_t time_remaining = cur_time - per_cpu(preempt_check, i).timestamp;

	if (likely(time_remaining < time_slice)) {
		epoch_slack = epoch_slack < time_remaining ? epoch_slack : time_remaining;
	} else if (per_cpu(preempt_check, i).check) {
    // Avoid preempting more times.
    BUG_ON(!cpu_preempt_points[i]);
    *(cpu_preempt_points[i]) = 1;
    per_cpu(preempt_check, i).check = false;
	}
}

static inline void handle_finished(int i, uint64_t active) {
  struct worker_response *response = &(per_cpu(worker_responses, i).responses[active]);
  context_free(response->rnbl);
  dev_consume_skb_any(response->mbuf);
  response->flag = PROCESSED;
}

static inline void handle_preempted(int i, uint64_t active) {
  struct worker_response *response = &(per_cpu(worker_responses, i).responses[active]);
  kfifo_in(&tskq, response, 1);
  response->flag = PROCESSED;
}

static inline void dispatch_requests(uint64_t cur_time) {
  uint32_t num_cpus = num_online_cpus();
  uint32_t num_workers = num_cpus - 2;
  int idle;

  struct worker_response req;
	while (1) {
		if (likely(idle_list_head < num_workers)) {
      idle = idle_list[idle_list_head];
      idle_list_head ++;
      if (!kfifo_out(&tskq, &req, 1)) {
        idle_list_head --;
        return;
      }
    } else {
      for (idle = 0; idle < num_workers; idle ++) {
        if(dispatcher_states[idle].occupancy == 1) break;
      }
      if (idle == num_workers) return;
      if (!kfifo_out(&tskq, &req, 1)) return;
    }

		uint8_t active_req = dispatcher_states[idle].next_push;
    struct dispatcher_request *request = &(per_cpu(dispatcher_requests, idle).requests[active_req]);
		request->rnbl = req.rnbl;
		request->mbuf = req.mbuf;
		request->type = req.type;
		request->category = req.category;
		request->timestamp = req.timestamp;
		request->flag = READY;

		jbsq_get_next(&(dispatcher_states[idle].next_push));
		dispatcher_states[idle].occupancy ++;
	}
}


static inline void handle_worker(int i, uint64_t cur_time) {
  concord_preempt_worker(i, cur_time);

  uint64_t active = dispatcher_states[i].next_pop;
  struct worker_response *response = &(per_cpu(worker_responses, i).responses[active]);
  struct dispatcher_request *request = &(per_cpu(dispatcher_requests, i).requests[active]);
  
  if (request->flag != READY) {
    if (response->flag == FINISHED) {
      handle_finished(i, active);
      jbsq_get_next(&(dispatcher_states[i].next_pop));
      dispatcher_states[i].occupancy --;
      if(dispatcher_states[i].occupancy == 0){
        idle_list_head--;
        idle_list[idle_list_head] = i;
      }
    } else if (response->flag == PREEMPTED) {
      handle_preempted(i, active);
      jbsq_get_next(&(dispatcher_states[i].next_pop));
      dispatcher_states[i].occupancy --;
      if(dispatcher_states[i].occupancy == 0){
        idle_list_head--;
        idle_list[idle_list_head] = i;
      }
    }
  }
}

static inline void handle_networker(uint64_t cur_time) {
  void *data;
  uint32_t ret;
  kcontext_t *cont;
  uint32_t max_batch = 16;
  struct worker_response request;

  while ((data = shinjuku_rpc_get()) && max_batch --) {
    ret = context_alloc(&cont);
    if (unlikely(ret)) {
      printk("[concord-dispatcher] Cannot allocate context\n");
      continue;
    }

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
  int i;
  uint64_t cur_time;
  struct task_struct *worker;
  uint32_t num_cpus = num_online_cpus();
  epoch_slack = PREEMPTION_DELAY;

  // We need at least 1 dispatcher and 1 worker, and 1 for the networker
  BUG_ON(num_cpus < 3);

  for (i = 2; i < num_cpus; i++) {
    worker = kthread_create(do_work, "concord-worker", "concord-worker");
    if (!IS_ERR(worker)) {
      kthread_bind(worker, i);
      wake_up_process(worker);
    } else {
      printk(KERN_ERR "Failed to create Concord worker thread for CPU %d\n", i);
      return PTR_ERR(worker);
    }
  }

  preempt_check_init();
  dispatcher_states_init();
  requests_init();

  while (1) {
    cur_time = concord_rdtsc();
    for (i = 0; i < num_cpus - 2; i++)
      handle_worker(i, cur_time);
    handle_networker(cur_time);
    dispatch_requests(cur_time);
  }

  return 0; // Never reached
}