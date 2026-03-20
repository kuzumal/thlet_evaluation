#include <linux/types.h>
#include <linux/smp.h>
#include <linux/percpu.h>
#include <linux/module.h>
#include "common.h"
#include "context.h"
#include "mempool.h"

struct mempool_datastore context_datastore;
struct mempool context_pool __attribute((aligned(64)));
struct mempool_datastore stack_datastore;
struct mempool stack_pool __attribute((aligned(64)));

extern int getcontext_fast(kcontext_t *ucp);

/**
 * context_alloc - allocates a kcontext_t and its stack
 * @cont: pointer to the pointer of the allocated context
 *
 * Returns 0 on success, -1 if failure.
 */
int context_alloc(kcontext_t ** cont)
{
    (*cont) = mempool_alloc(&context_pool);
    if (unlikely(!(*cont)))
        return -1;

    void * stack = mempool_alloc(&stack_pool);
    if (unlikely(!stack)) {
        mempool_free(&context_pool, (*cont));
        return -1;
    }

    (*cont)->stack_base = stack;
    return 0;
}

/**
 * context_free - frees a context and the associated stack
 * @c: the context
 */
void context_free(kcontext_t *c)
{
    mempool_free(&stack_pool, c->stack_base);
    mempool_free(&context_pool, c);
}

/**
 * set_context_link - sets the return context of a kcontext_t
 * @c: the context
 * @uc_link: the return context of c
 */
void set_context_link(kcontext_t *c, kcontext_t *uc_link) {
  /* implement me */
}

void make_kcontext(kcontext_t *c, void (*func)(void), 
                   uint64_t arg0, uint64_t arg1, 
                   uint64_t arg2, uint64_t arg3)
{
    uintptr_t stack_top = (uintptr_t)c->stack_base + CONCORD_THREAD_STACK_SIZE;
    
    c->regs[O_REG_SP / 8] = (uint64_t)(stack_top & ~0xFUL);
    c->regs[O_REG_RA / 8] = (uint64_t)func;

    c->regs[O_REG_A0 / 8] = arg0;
    c->regs[O_REG_A1 / 8] = arg1;
    c->regs[O_REG_A2 / 8] = arg2;
    c->regs[O_REG_A3 / 8] = arg3;

    uint64_t current_sstatus;
    asm volatile("csrr %0, sstatus" : "=r"(current_sstatus));
    c->regs[O_REG_SSTATUS / 8] = current_sstatus;
    
    uint64_t current_gp;
    asm volatile("mv %0, gp" : "=r"(current_gp));
    c->regs[O_REG_GP / 8] = current_gp;
}