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
#include <asm/csr.h>
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

extern int getcontext_fast(kcontext_t *ucp);
extern int swapcontext_very_fast(kcontext_t *ouctx, kcontext_t *uctx);

static inline uint64_t enable_only_ipi(void) {
  uint64_t old_sie;
  asm volatile("csrr %0, sie" : "=r"(old_sie));
  uint64_t mask = SIE_STIE | SIE_SEIE;
  asm volatile("csrc sie, %0" : : "r"(mask));
  local_irq_enable();

  return old_sie;
}

void *worker_ipi_handler(void *arg) {
  per_cpu(worker_preempted, smp_processor_id()) = true;
  // debug("[shinjuku-worker-%d] Into ipi\n", smp_processor_id());

  return NULL;
}

uint64_t worker_finish;

/**
 * generic_work - generic function acting as placeholder for application-level
 *                work
 * @msw: the top 32-bits of the pointer containing the data
 * @lsw: the bottom 32 bits of the pointer containing the data
 */
static void generic_work(uint64_t a0, uint64_t data, uint64_t a2,
                         uint64_t a3) {
  BUG_ON(data >= 100000);
  enable_only_ipi();
  uint64_t i = 0;
  uint32_t worker = smp_processor_id();
  kcontext_t *cont = per_cpu(ctx_worker, worker);
  kcontext_t *uctx_main = &per_cpu(ctx_main, worker);

  do {
    asm volatile ("nop");
    if (per_cpu(worker_preempted, worker)) {
      // debug("[shinjuku-worker-%d] Being preempied\n", worker);
      per_cpu(worker_preempted, worker) = false;
      local_irq_disable();
      if (swapcontext_very_fast(cont, uctx_main)) {
        debug("[shinjuku-worker-%d] switch fail\n", worker);
      }
    }
  } while (i ++ < data);

  // debug("[shinjuku-worker-%d] exiting generic_work, finished %llu\n", worker, worker_finish ++);
  local_irq_disable();
  per_cpu(finished, smp_processor_id()) = true;
  if (swapcontext_very_fast(cont, uctx_main)) {
    debug("[shinjuku-worker-%d] switch fail\n", worker);
  }
}

static inline uint32_t init_worker(void) {
  uint32_t worker = smp_processor_id();
  per_cpu(worker_active, worker) = true;
  per_cpu(worker_preempted, worker) = false;
  per_cpu(worker_responses, worker).flag = PROCESSED;
  local_irq_disable();
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
  struct dispatcher_request *request = &per_cpu(dispatcher_requests, worker);
  per_cpu(ctx_worker, worker) = request->rnbl;
  kcontext_t *cont = request->rnbl;
  kcontext_t *uctx_main = &per_cpu(ctx_main, worker);
  parse_data(request->mbuf, &data);

  getcontext_fast(cont);
  make_kcontext(cont, (void (*)(void)) generic_work, 0, data, 3, 4);
  per_cpu(finished, worker) = false;
  ret = swapcontext_very_fast(uctx_main, cont);
  if (ret) {
    printk("[shinjuku-worker] Failed to do swap into new context\n");
  }
}

static inline void handle_context(uint32_t worker) {
  int ret;
  kcontext_t **cont = &per_cpu(ctx_worker, worker);
  kcontext_t *uctx_main = &per_cpu(ctx_main, worker);
  per_cpu(finished, worker) = false;
  (*cont) = per_cpu(dispatcher_requests, worker).rnbl;
  BUG_ON((*cont) == NULL);
  set_context_link(*cont, uctx_main);
  ret = swapcontext_very_fast(uctx_main, *cont);
  if (ret) {
    printk("[shinjuku-worker] Failed to swap to existing context\n");
  }
}

static inline void handle_request(uint32_t worker) {
  struct dispatcher_request *request = &per_cpu(dispatcher_requests, worker);
  while (request->flag == WAITING) rmb();

  // debug("[shinjuku-worker-%d] Get work %d, type: %d, %s\n", worker, request->flag, request->category,
  //   request->category == CONTEXT ? "CONTEXT" : request->category == PACKET ? "PACKET" : "UNKNOWN");
  request->flag = WAITING;
  if (request->category == PACKET)
    handle_new_packet(worker);
  else if (request->category == CONTEXT)
    handle_context(worker);
  else BUG_ON(1);
}

static inline void finish_request(uint32_t worker) {
  struct dispatcher_request *request = &per_cpu(dispatcher_requests, worker);
  struct worker_response *response = &per_cpu(worker_responses, worker);
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
}

int do_work(void *arg) {
  uint32_t worker = init_worker();
  printk("[shinjuku-worker-%d] do_work: Waiting for dispatcher work\n", worker);

  while (true) {
    handle_request(worker);
    finish_request(worker);
  }

  return 0;
}