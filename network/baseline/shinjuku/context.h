#ifndef _SHINJUKU_CONTEXT_H
#define _SHINJUKU_CONTEXT_H

#include <linux/types.h>
#include <linux/percpu.h>
#include "common.h"

#define O_REG_RA      0x00
#define O_REG_SP      0x08
#define O_REG_GP      0x10
#define O_REG_TP      0x18
#define O_REG_S0      0x20
#define O_REG_S1      0x28
#define O_REG_S2      0x30
#define O_REG_S3      0x38
#define O_REG_S4      0x40
#define O_REG_S5      0x48
#define O_REG_S6      0x50
#define O_REG_S7      0x58
#define O_REG_S8      0x60
#define O_REG_S9      0x68
#define O_REG_S10     0x70
#define O_REG_S11     0x78
#define O_REG_A0      0x80
#define O_REG_A1      0x88
#define O_REG_A2      0x90
#define O_REG_A3      0x98
#define O_REG_A4      0xA0
#define O_REG_A5      0xA8
#define O_REG_A6      0xB0
#define O_REG_A7      0xB8
#define O_REG_T0      0xC0
#define O_REG_T1      0xC8
#define O_REG_T2      0xD0
#define O_REG_T3      0xD8
#define O_REG_T4      0xE0
#define O_REG_T5      0xE8
#define O_REG_T6      0xF0
#define O_REG_SSTATUS 0xF8
#define KCTX_SIZE     0x100

int context_init(void);
int context_alloc(kcontext_t ** cont);
void context_free(kcontext_t * cont);
void set_context_link(kcontext_t * cont, kcontext_t * link);
void make_kcontext(kcontext_t *c, void (*func)(void), 
                   uint64_t arg0, uint64_t arg1, 
                   uint64_t arg2, uint64_t arg3);

#endif /* _SHINJUKU_CONTEXT_H */