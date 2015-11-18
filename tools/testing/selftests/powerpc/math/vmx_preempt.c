#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <pthread.h>

#include "utils.h"

/* Time to wait for workers to get preempted (seconds) */
#define PREEMPT_TIME 20
/*
 * Factor by which to multiply number of online CPUs for total number of
 * worker threads
 */
#define THREAD_FACTOR 8

typedef int v4si __attribute__ ((vector_size (16)));
__thread v4si varray[] = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10,11,12},
	{13,14,15,16},{17,18,19,20},{21,22,23,24},
	{25,26,27,28},{29,30,31,32},{33,34,35,36},
	{37,38,39,40},{41,42,43,44},{45,46,47,48}};

volatile int not_ready;
int running;

extern void preempt_vmx(v4si *varray, volatile int *not_ready, int *sentinal);

void *preempt_vmx_c(void *p)
{
	int i, j;
	srand(pthread_self());
	for (i = 0; i < 12; i++)
		for (j = 0; j < 4; j++)
			varray[i][j] = rand();

	/* Test fails if it ever returns */
	preempt_vmx(varray, &not_ready, &running);
	return p;
}

int test_preempt_vmx(void)
{
	int i, rc, threads;
	pthread_t *tids;

	threads = sysconf(_SC_NPROCESSORS_ONLN) * THREAD_FACTOR;
	tids = malloc(threads * sizeof(pthread_t));
	FAIL_IF(!tids);

	running = true;
	not_ready = threads;
	for (i = 0; i < threads; i++) {
		rc = pthread_create(&tids[i], NULL, preempt_vmx_c, NULL);
		FAIL_IF(rc);
	}

	setbuf(stdout, NULL);
	/* Not really nessesary but nice to wait for every thread to start */
	printf("\tWaiting for all workers to start...");
	while(not_ready);
	printf("done\n");

	printf("\tWaiting for %d seconds to let some workers get preempted...", PREEMPT_TIME);
	sleep(PREEMPT_TIME);
	printf("done\n");

	printf("\tKilling workers...");
	running = 0;
	for (i = 0; i < threads; i++) {
		void *rc_p;
		pthread_join(tids[i], &rc_p);

		/*
		 * Harness will say the fail was here, look at why preempt_vmx
		 * returned
		 */
		if ((long) rc_p)
			printf("oops\n");
		FAIL_IF((long) rc_p);
	}
	printf("done\n");

	return 0;
}

int main(int argc, char *argv[])
{
	return test_harness(test_preempt_vmx, "vmx_preempt");
}
