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
DEFINE_PER_CPU(struct mempool, response_pool __attribute__((aligned(64))));

extern int getcontext_fast(kcontext_t *ucp);
extern int swapcontext_fast(kcontext_t *ouctx, kcontext_t *uctx);
extern int swapcontext_very_fast(kcontext_t *ouctx, kcontext_t *uctx);

void *worker_ipi_handler(void *arg) {
  local_irq_disable();
  uint32_t worker = smp_processor_id();
  kcontext_t *cont = per_cpu(ctx_worker, worker);
  kcontext_t *uctx_main = &per_cpu(ctx_main, worker);
  swapcontext_very_fast(cont, uctx_main);

  return NULL;
}

/**
 * generic_work - generic function acting as placeholder for application-level
 *                work
 * @msw: the top 32-bits of the pointer containing the data
 * @lsw: the bottom 32 bits of the pointer containing the data
 */
static void generic_work(uint64_t data, uint64_t lsw, uint64_t msw_id,
                         uint64_t lsw_id) {

  BUG_ON(data >= 100000);                      
  local_irq_enable();
  uint64_t i = 0;
  uint32_t worker = smp_processor_id();
  kcontext_t *cont = per_cpu(ctx_worker, worker);
  kcontext_t *uctx_main = &per_cpu(ctx_main, worker);

  do {
    asm volatile ("nop");
    i++;
  } while (i < data);

  local_irq_disable();
  per_cpu(finished, smp_processor_id()) = true;
  swapcontext_very_fast(cont, uctx_main);
}

static inline uint32_t init_worker(void) {
  uint32_t worker = smp_processor_id();
  per_cpu(worker_active, worker) = true;
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
  kcontext_t *cont = per_cpu(ctx_worker, worker);
  kcontext_t *uctx_main = &per_cpu(ctx_main, worker);
  parse_data(request->mbuf, &data);

  cont = request->rnbl;
  getcontext_fast(cont);
  make_kcontext(cont, (void (*)(void)) generic_work, data, 0, 0, 0);
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
  set_context_link(*cont, uctx_main);
  ret = swapcontext_fast(uctx_main, *cont);
  if (ret) {
    printk("[shinjuku-worker] Failed to swap to existing context\n");
  }
}

static inline void handle_request(uint32_t worker) {
  struct dispatcher_request *request = &per_cpu(dispatcher_requests, worker);
  while (request->flag == WAITING);
  // response->flag = RUNNING;
  // response->timestamp = shinjuku_rdtsc();
  request->flag = WAITING;
  if (request->category == PACKET)
    handle_new_packet(worker);
  else
    handle_context(worker);
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

void do_work(void) {
  uint32_t worker = init_worker();
  printk("[shinjuku-worker] do_work: Waiting for dispatcher work\n");

  while (true) {
    handle_request(worker);
    finish_request(worker);
  }
}