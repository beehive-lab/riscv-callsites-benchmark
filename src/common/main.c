#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <time.h>

#ifndef USE_PERF
#define USE_PERF 1
#endif

#ifdef USE_PERF
/* code inspired by https://stackoverflow.com/questions/42088515/perf-event-open-how-to-monitoring-multiple-events 
 * part of it has been just copied!!!
 */

#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <string.h>

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

#define L1I_CACHE_REFILL 0x01
#define L1I_CACHE 0x14
#define L1D_CACHE_REFILL 0x03
#define L1D_CACHE 0x04
#define L2D_CACHE 0x16
#define L2D_CACHE_REFILL 0x17

struct read_format
{
	uint64_t nr;
	struct
	{
		uint64_t value;
		uint64_t id;
	} values[];
};
#endif

extern void test_setup();
extern void test_entry();
extern void test_end();

#define BILL 1000000000

/* NB Lifted from GNU manual */

/* Subtract the ‘struct timeval’ value y from x,
   storing the result in RESULT.
   Return 1 if the difference is negative, otherwise 0. */
int
sub_timespec (struct timespec *result, struct timespec *x, struct timespec *y)
{
	/* Perform the carry for the later subtraction by updating y. */
	if (x->tv_nsec < y->tv_nsec) {
		long nsec = (y->tv_nsec - x->tv_nsec) / BILL + 1;
		y->tv_nsec -= BILL * nsec;
		y->tv_sec += nsec;
	}
	if (x->tv_nsec - y->tv_nsec > BILL) {
		long nsec = (x->tv_nsec - y->tv_nsec) / BILL;
		y->tv_nsec += BILL * nsec;
		y->tv_sec -= nsec;
	}

	/* Compute the time remaining to wait.
   *      tv_usec is certainly positive. */
	result->tv_sec = x->tv_sec - y->tv_sec;
	result->tv_nsec = x->tv_nsec - y->tv_nsec;

	/* Return 1 if result is negative. */
	return x->tv_sec < y->tv_sec;
}

int main(int c, char **v)
{
#ifdef USE_PERF
	struct perf_event_attr pea;
	int fd_cycles, fd1, fd2, fd3, fd4, fd5, fd6;
	uint64_t id_cycles, id1, id2, id3, id4, id5, id6;
	uint64_t cycles, val1, val2, val3, val4, val5, val6;
	char buf[4096];
	struct read_format *rf = (struct read_format *)buf;
	int i;

	memset(&pea, 0, sizeof(struct perf_event_attr));
	pea.size = sizeof(struct perf_event_attr);
	pea.disabled = 1;
	pea.exclude_kernel = 1;
	pea.exclude_hv = 1;
	pea.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;

	/* Setup hardware counters */
	pea.type = PERF_TYPE_HARDWARE;
	pea.config = PERF_COUNT_HW_CPU_CYCLES;
	fd_cycles = syscall(__NR_perf_event_open, &pea, 0, -1, -1, 0);
	ioctl(fd_cycles, PERF_EVENT_IOC_ID, &id_cycles);

	/* Setup hardware counters for cache */
	pea.type = PERF_TYPE_RAW;
	pea.config = L1I_CACHE_REFILL;
	fd1 = syscall(__NR_perf_event_open, &pea, 0, -1, fd_cycles, 0);
	ioctl(fd1, PERF_EVENT_IOC_ID, &id1);

	pea.type = PERF_TYPE_RAW;
	pea.config = L1I_CACHE;
	fd2 = syscall(__NR_perf_event_open, &pea, 0, -1, fd_cycles, 0);
	ioctl(fd2, PERF_EVENT_IOC_ID, &id2);

	pea.type = PERF_TYPE_RAW;
	pea.config = L1D_CACHE_REFILL;
	fd3 = syscall(__NR_perf_event_open, &pea, 0, -1, fd_cycles, 0);
	ioctl(fd3, PERF_EVENT_IOC_ID, &id3);

	pea.type = PERF_TYPE_RAW;
	pea.config = L1D_CACHE;
	fd4 = syscall(__NR_perf_event_open, &pea, 0, -1, fd_cycles, 0);
	ioctl(fd4, PERF_EVENT_IOC_ID, &id4);

	pea.type = PERF_TYPE_RAW;
	pea.config = L2D_CACHE_REFILL;
	fd5 = syscall(__NR_perf_event_open, &pea, 0, -1, fd_cycles, 0);
	ioctl(fd5, PERF_EVENT_IOC_ID, &id5);

	pea.type = PERF_TYPE_RAW;
	pea.config = L2D_CACHE;
	fd6 = syscall(__NR_perf_event_open, &pea, 0, -1, fd_cycles, 0);
	ioctl(fd6, PERF_EVENT_IOC_ID, &id6);
#endif

	struct timespec then, now, elapsed;

	void * page = (void *)((uintptr_t)test_entry & ~0xfff);
	void * page_end = (void *)(((uintptr_t)test_end + 0xfff) & ~0xfff);
	uintptr_t len = (uintptr_t)page_end - (uintptr_t)page;
	assert(0 == mprotect(page, len, PROT_WRITE | PROT_EXEC));

	test_setup();

	/* warmup */
	test_entry();
	test_entry();
	test_entry();
	test_entry();

	/* measure */
	assert(0 == clock_gettime(CLOCK_MONOTONIC_RAW, &then));
#ifdef USE_PERF
	ioctl(fd_cycles, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
	ioctl(fd_cycles, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
#endif

	test_entry();
#ifdef USE_PERF
	ioctl(fd_cycles, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
#endif

	assert (0 == clock_gettime(CLOCK_MONOTONIC_RAW, &now));
	assert (0 == sub_timespec(&elapsed, &now, &then));
	assert (elapsed.tv_sec == 0);
#ifdef USE_PERF
	int rd = read(fd_cycles, buf, sizeof(buf));
	assert(rd != -1);
	for (i = 0; i < rf->nr; i++)
	{
		uint64_t id = rf->values[i].id;
		if (id == id_cycles)
			cycles = rf->values[i].value;
		else if (id == id1)
			val1 = rf->values[i].value;
		else if (id == id2)
			val2 = rf->values[i].value;
		else if (id == id3)
			val3 = rf->values[i].value;
		else if (id == id4)
			val4 = rf->values[i].value;
		else if (id == id5)
			val5 = rf->values[i].value;
		else if (id == id6)
			val6 = rf->values[i].value;
		else
			return 1;
	}
	printf("%09ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld\n",
		   elapsed.tv_nsec, cycles, val1, val2, val3, val4, val5, val6);
#else
	printf("%09ld\n", elapsed.tv_nsec);
#endif
}
