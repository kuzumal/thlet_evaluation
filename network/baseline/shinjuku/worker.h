#ifndef __SHINJUKU_WORKER_H
#define __SHINJUKU_WORKER_H

#include <linux/types.h>
#include <linux/percpu.h>
#include <linux/smp.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include "common.h"

DECLARE_PER_CPU(kcontext_t, ctx_main);
DECLARE_PER_CPU(kcontext_t *, ctx_worker);
DECLARE_PER_CPU(uint8_t, finished);
DECLARE_PER_CPU(struct mempool, response_pool __attribute__((aligned(64))));

void *worker_ipi_handler(void *arg);
int do_work(void *arg);

#endif /* __SHINJUKU_WORKER_H */