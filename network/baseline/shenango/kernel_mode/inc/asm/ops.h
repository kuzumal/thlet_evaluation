/*
 * ops.h - useful riscv instructions
 */

#pragma once

#include <base/types.h>

static inline void cpu_relax(void)
{
	asm volatile("pause");
}

static inline uint64_t shenango_rdtsc(void) {
  uint64_t val;
  asm volatile ("rdcycle %0" : "=r" (val));
  return val;
}

static inline uint64_t __mm_crc32_u64(uint64_t crc, uint64_t val)
{
	asm("crc32q %1, %0" : "+r" (crc) : "rm" (val));
	return crc;
}
