
#include "context.h"

.text
.align 4
.globl swapcontext_very_fast
.type swapcontext_very_fast, @function

swapcontext_very_fast:
    /* Save the preserved registers, the registers used for passing args,
        and the return address.  */
    sd ra, O_REG_RA(a0)
    sd sp, O_REG_SP(a0)
    sd gp, O_REG_GP(a0)
    sd tp, O_REG_TP(a0)
    
    sd s0,  O_REG_S0(a0)
    sd s1,  O_REG_S1(a0)
    sd s2,  O_REG_S2(a0)
    sd s3,  O_REG_S3(a0)
    sd s4,  O_REG_S4(a0)
    sd s5,  O_REG_S5(a0)
    sd s6,  O_REG_S6(a0)
    sd s7,  O_REG_S7(a0)
    sd s8,  O_REG_S8(a0)
    sd s9,  O_REG_S9(a0)
    sd s10, O_REG_S10(a0)
    sd s11, O_REG_S11(a0)

    sd a0, O_REG_A0(a0)
    sd a1, O_REG_A1(a0)
    sd a2, O_REG_A2(a0)
    sd a3, O_REG_A3(a0)
    sd a4, O_REG_A4(a0)
    sd a5, O_REG_A5(a0)
    sd a6, O_REG_A6(a0)
    sd a7, O_REG_A7(a0)

    sd t0, O_REG_T0(a0)
    sd t1, O_REG_T1(a0)
    sd t2, O_REG_T2(a0)
    sd t3, O_REG_T3(a0)
    sd t4, O_REG_T4(a0)
    sd t5, O_REG_T5(a0)
    sd t6, O_REG_T6(a0)
    
    csrr t0, sstatus
    sd   t0, O_REG_SSTATUS(a0)

    /* Load the new stack pointer and the preserved registers.  */
    ld t0, O_REG_SSTATUS(a1)
    csrw sstatus, t0
    ld sp, O_REG_SP(a1)

    ld s0,  O_REG_S0(a1)
    ld s1,  O_REG_S1(a1)
    ld s2,  O_REG_S2(a1)
    ld s3,  O_REG_S3(a1)
    ld s4,  O_REG_S4(a1)
    ld s5,  O_REG_S5(a1)
    ld s6,  O_REG_S6(a1)
    ld s7,  O_REG_S7(a1)
    ld s8,  O_REG_S8(a1)
    ld s9,  O_REG_S9(a1)
    ld s10, O_REG_S10(a1)
    ld s11, O_REG_S11(a1)

    ld ra, O_REG_RA(a1)

    ld a2, O_REG_A2(a1)
    ld a3, O_REG_A3(a1)
    ld t0, O_REG_T0(a1)
    ld t1, O_REG_T1(a1)
    ld t2, O_REG_T2(a1)
    ld t3, O_REG_T3(a1)
    ld t4, O_REG_T4(a1)
    ld t5, O_REG_T5(a1)
    ld t6, O_REG_T6(a1)

    ld a1, O_REG_A1(a1)

    /* Clear $a0 to indicate success.  */
    li a0, 0
    ret

.text
.align 4
.globl getcontext_fast
.type getcontext_fast, @function

getcontext_fast:
    sd ra, O_REG_RA(a0)
    sd sp, O_REG_SP(a0)

    sd gp, O_REG_GP(a0)
    sd tp, O_REG_TP(a0)

    sd s0,  O_REG_S0(a0)
    sd s1,  O_REG_S1(a0)
    sd s2,  O_REG_S2(a0)
    sd s3,  O_REG_S3(a0)
    sd s4,  O_REG_S4(a0)
    sd s5,  O_REG_S5(a0)
    sd s6,  O_REG_S6(a0)
    sd s7,  O_REG_S7(a0)
    sd s8,  O_REG_S8(a0)
    sd s9,  O_REG_S9(a0)
    sd s10, O_REG_S10(a0)
    sd s11, O_REG_S11(a0)

    sd a0, O_REG_A0(a0)
    sd a1, O_REG_A1(a0)
    sd a2, O_REG_A2(a0)
    sd a3, O_REG_A3(a0)
    sd a4, O_REG_A4(a0)
    sd a5, O_REG_A5(a0)
    sd a6, O_REG_A6(a0)
    sd a7, O_REG_A7(a0)

    sd t0, O_REG_T0(a0)
    sd t1, O_REG_T1(a0)
    sd t2, O_REG_T2(a0)
    sd t3, O_REG_T3(a0)
    sd t4, O_REG_T4(a0)
    sd t5, O_REG_T5(a0)
    sd t6, O_REG_T6(a0)

    csrr t0, sstatus
    sd   t0, O_REG_SSTATUS(a0)
    
    /* Clear $a0 to indicate success.  */
    li a0, 0

    ret