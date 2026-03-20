#include <linux/types.h>
#include <linux/percpu.h>
#include <linux/smp.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/printk.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/bug.h>
#include <linux/netdevice.h>
#include <asm/byteorder.h>
#include <linux/irqflags.h>
#include <linux/thlet_rpc.h>
#include "common.h"
#include "mempool.h"
#include "worker.h"
#include "context.h"

DEFINE_PER_CPU(kcontext_t, ctx_main);
DEFINE_PER_CPU(kcontext_t *, ctx_worker);
DEFINE_PER_CPU(uint8_t, finished);
DEFINE_PER_CPU(uint8_t, worker_preempted);
DEFINE_PER_CPU(struct mempool, response_pool __attribute__((aligned(64))));

extern volatile uint64_t *cpu_preempt_points[MAX_WORKERS];
uint64_t concord_preempt_now[MAX_WORKERS];
DEFINE_PER_CPU(uint8_t, active_req);

extern int getcontext_fast(kcontext_t *ucp);
extern int swapcontext_very_fast(kcontext_t *ouctx, kcontext_t *uctx);

/**
 * generic_work - generic function acting as placeholder for application-level
 *                work
 * @msw: the top 32-bits of the pointer containing the data
 * @lsw: the bottom 32 bits of the pointer containing the data
 */
static void generic_work(uint64_t data, uint64_t lsw, uint64_t msw_id,
                         uint64_t lsw_id) {

  BUG_ON(data >= 100000);                      
  uint64_t i = 0;
  uint32_t worker = smp_processor_id();
  kcontext_t *cont = per_cpu(ctx_worker, worker);
  kcontext_t *uctx_main = &per_cpu(ctx_main, worker);

  do {
    asm volatile ("nop");
    i ++;
  } while (i < data);

  per_cpu(finished, smp_processor_id()) = true;
  swapcontext_very_fast(cont, uctx_main);
}

static inline uint32_t init_worker(void) {
  uint32_t worker = smp_processor_id();
  per_cpu(worker_active, worker) = true;
  per_cpu(active_req, worker);
  for (int i = 0; i < JBSQ_LEN; i ++)
    per_cpu(worker_responses, worker).responses[i].flag = PROCESSED;
  return worker;
}

static inline void parse_data(void *mbuf, uint64_t *data) {
  struct sk_buff *skb = (struct sk_buff *) mbuf;

  struct iphdr *iph = (struct iphdr *)skb->data;
  struct udphdr *udph = (struct udphdr *)(skb->data + (iph->ihl * 4));
  unsigned char *payload = (unsigned char *)udph + sizeof(struct udphdr);
  uint32_t payload_len = ntohs(udph->len) - sizeof(struct udphdr);
  BUG_ON(payload_len < sizeof(uint64_t) * 2);
  Rpc *rpc = (Rpc *)payload;
  // we assume the type of Rpc is the spinning counts
  (*data) = rpc->type;
}

static inline void handle_new_packet(uint32_t worker) {
  int ret;
  uint64_t data;
  struct dispatcher_request *request = &(per_cpu(dispatcher_requests, worker).requests[per_cpu(active_req, worker)]);
  kcontext_t *cont = per_cpu(ctx_worker, worker);
  kcontext_t *uctx_main = &per_cpu(ctx_main, worker);
  parse_data(request->mbuf, &data);

  cont = request->rnbl;
  getcontext_fast(cont);
  make_kcontext(cont, (void (*)(void)) generic_work, data, 0, 0, 0);
  per_cpu(finished, worker) = false;
  ret = swapcontext_very_fast(uctx_main, cont);
  if (ret) {
    printk("[concord-worker] Failed to do swap into new context\n");
  }
}

static inline void handle_context(uint32_t worker) {
  int ret;
  per_cpu(finished, worker) = false;
  kcontext_t **cont = &per_cpu(ctx_worker, worker);
  kcontext_t *uctx_main = &per_cpu(ctx_main, worker);
  (*cont) = per_cpu(dispatcher_requests, worker).requests[per_cpu(active_req, worker)].rnbl;
  set_context_link(*cont, uctx_main);
  ret = swapcontext_very_fast(uctx_main, *cont);
  if (ret) {
    printk("[concord-worker] Failed to swap to existing context\n");
  }
}

static inline void handle_request(uint32_t worker) {
  struct dispatcher_request *request = &(per_cpu(dispatcher_requests, worker).requests[per_cpu(active_req, worker)]);
  while (request->flag != READY);
  per_cpu(preempt_check, worker).timestamp = concord_rdtsc();
  per_cpu(preempt_check, worker).check = true;
  if (request->category == PACKET)
    handle_new_packet(worker);
  else
    handle_context(worker);
}

static inline void finish_request(uint32_t worker) {
  struct dispatcher_request *request = &(per_cpu(dispatcher_requests, worker).requests[per_cpu(active_req, worker)]);
  struct worker_response *response = &(per_cpu(worker_responses, worker).responses[per_cpu(active_req, worker)]);
  kcontext_t *cont = per_cpu(ctx_worker, worker);

  response->timestamp = request->timestamp;
  response->type = request->type;
  response->mbuf = request->mbuf;
  response->rnbl = cont;
  response->category = CONTEXT;
  if (per_cpu(finished, worker)) {
    response->flag = FINISHED;
  } else {
    response->flag = PREEMPTED;
  }
  request->flag = DONE;
}


void noinline concord_func(void) {
  uint32_t worker = smp_processor_id();
  concord_preempt_now[worker] = 0;
  kcontext_t **cont = &per_cpu(ctx_worker, worker);
  kcontext_t *uctx_main = &per_cpu(ctx_main, worker);
  swapcontext_very_fast(*cont, uctx_main);
}

int do_work(void *arg) {
  uint32_t worker = init_worker();
  printk("[concord-worker] do_work: Waiting for dispatcher work\n");

  cpu_preempt_points[worker] = &concord_preempt_now[worker];
  while (true) {
    handle_request(worker);
    finish_request(worker);
    jbsq_get_next(&per_cpu(active_req, worker));
  }

  return 0;
}