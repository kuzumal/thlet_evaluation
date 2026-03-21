#ifndef SHINJUKU_COMMON_H
#define SHINJUKU_COMMON_H

#include <linux/types.h>

#define SHINJUKU_SIMULATION
#define SHINJUKU_REPORT
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
#define PREEMPTION_DELAY 15000
#define REG_NUMS    64

#ifndef SIE_SSIE /* Supervisor Software Interrupt Enable (IPI) */
#define SIE_SSIE (1UL << 1)  
#endif
#ifndef SIE_STIE /* Supervisor Timer Interrupt Enable */
#define SIE_STIE (1UL << 5) 
#endif
#ifndef SIE_SEIE /* Supervisor External Interrupt Enable */
#define SIE_SEIE (1UL << 9)  
#endif

#ifdef SHINJUKU_SIMULATION
    #define debug(fmt, ...) printk(fmt, ##__VA_ARGS__)
#else
    #define debug(fmt, ...) do { } while (0)
#endif

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
#ifdef SHINJUKU_SIMULATION
  uint16_t id;
  char make_it_64_bytes[14];
#else
  char make_it_64_bytes[30];
#endif
} __attribute__((packed, aligned(64)));

struct dispatcher_request {
  uint64_t flag;
  void * rnbl;
  void * mbuf;
  uint8_t type;
  uint8_t category;
  uint64_t timestamp;
#ifdef SHINJUKU_SIMULATION
  uint16_t id;
  char make_it_64_bytes[14];
#else
  char make_it_64_bytes[30];
#endif
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