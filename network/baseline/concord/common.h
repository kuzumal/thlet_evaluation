#ifndef CONCORD_COMMON_H
#define CONCORD_COMMON_H

#include <linux/types.h>

#define MAX_WORKERS   64

#define INACTIVE    0x00
#define READY       0x01
#define DONE        0x02 

#define RUNNING     0x00
#define FINISHED    0x01
#define PREEMPTED   0x02
#define PROCESSED   0x03

#define NOCONTENT   0x00
#define PACKET      0x01
#define CONTEXT     0x02

#define TYPE_REQ 1
#define TYPE_RES 0

#define MAX_UINT64  0xFFFFFFFFFFFFFFFF
#define PREEMPTION_DELAY 5000
#define CPU_FREQ_GHZ     1
#define REG_NUMS    64
#define CONCORD_THREAD_STACK_SIZE 8192

#define JBSQ_LEN 2

typedef struct {
  uint64_t regs[REG_NUMS];
  void *stack_base;
} kcontext_t;

struct worker_response {
  uint64_t flag;
  void * rnbl;
  void * mbuf;
  uint64_t timestamp;
  uint8_t type;
  uint8_t category;
  char make_it_64_bytes[30];
} __attribute__((packed, aligned(64)));

struct jbsq_worker_response {
  struct worker_response responses[JBSQ_LEN];
}__attribute__((packed, aligned(64)));

struct dispatcher_request {
  uint64_t flag;
  void * rnbl;
  void * mbuf;
  uint8_t type;
  uint8_t category;
  uint64_t timestamp;
  char make_it_64_bytes[30];
} __attribute__((packed, aligned(64)));

struct jbsq_dispatcher_request {
  struct dispatcher_request requests[JBSQ_LEN];
}__attribute__((packed, aligned(64)));

struct jbsq_preemption {
  uint64_t timestamp;
  uint8_t check;
  char make_it_64_bytes[55];
} __attribute__((packed, aligned(64)));

struct worker_state {
  uint8_t next_push;
  uint8_t next_pop;
  uint8_t occupancy;
} __attribute__((packed));

static inline uint64_t concord_rdtsc(void) {
  uint64_t val;
  asm volatile ("rdcycle %0" : "=r" (val));
  return val;
}

static inline void jbsq_get_next(uint8_t* iter){
  *iter = *iter^1; // This is for JBSQ_LEN = 2
}

DECLARE_PER_CPU(uint8_t, worker_active);
DECLARE_PER_CPU(struct jbsq_preemption, preempt_check);
DECLARE_PER_CPU(struct jbsq_worker_response, worker_responses);
DECLARE_PER_CPU(struct jbsq_dispatcher_request, dispatcher_requests);

#endif /* CONCORD_COMMON_H */