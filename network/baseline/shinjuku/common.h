#ifndef SHINJUKU_COMMON_H
#define SHINJUKU_COMMON_H

#include <linux/types.h>

#define MAX_WORKERS   64

#define WAITING     0x00
#define ACTIVE      0x01

#define RUNNING     0x00
#define FINISHED    0x01
#define PREEMPTED   0x02
#define PROCESSED   0x03

#define NOCONTENT   0x00
#define PACKET      0x01
#define CONTEXT     0x02

#define MAX_UINT64  0xFFFFFFFFFFFFFFFF
#define PREEMPTION_DELAY 5000
#define REG_NUMS    64
#define SHINJUKU_THREAD_STACK_SIZE 8192

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

struct dispatcher_request {
  uint64_t flag;
  void * rnbl;
  void * mbuf;
  uint8_t type;
  uint8_t category;
  uint64_t timestamp;
  char make_it_64_bytes[30];
} __attribute__((packed, aligned(64)));

static inline uint64_t shinjuku_rdtsc(void) {
  uint64_t val;
  asm volatile ("rdcycle %0" : "=r" (val));
  return val;
}

DECLARE_PER_CPU(uint8_t, worker_active);
DECLARE_PER_CPU(uint64_t, shinjuku_timestamps);
DECLARE_PER_CPU(uint8_t, preempt_check);
DECLARE_PER_CPU(struct worker_response, worker_responses);
DECLARE_PER_CPU(struct dispatcher_request, dispatcher_requests);

#endif /* SHINJUKU_COMMON_H */