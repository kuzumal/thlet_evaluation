/*
 * time.c - timekeeping utilities
 */

#include <time.h>

#include <base/time.h>
#include <base/log.h>
#include <base/init.h>

#include "init_internal.h"

int cycles_per_us __aligned(CACHE_LINE_SIZE);
uint64_t start_tsc;

/**
 * __timer_delay_us - spins the CPU for the specified delay
 * @us: the delay in microseconds
 */
void __time_delay_us(uint64_t us)
{
	uint64_t cycles = us * cycles_per_us;
	unsigned long start = shenango_rdtsc();

	while (shenango_rdtsc() - start < cycles)
		cpu_relax();
}

static int time_calibrate_tsc(void)
{
	/* we asssume that 1 ns == 1 cycle */
	cycles_per_us = 1000;
	log_info("time: detected %d ticks / us", cycles_per_us);

	/* record the start time of the binary */
	start_tsc = shenango_rdtsc();

	return -1;
}

/**
 * time_init - global time initialization
 *
 * Returns 0 if successful, otherwise fail.
 */
int time_init(void)
{
	return time_calibrate_tsc();
}
