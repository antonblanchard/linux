#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <pthread.h>

#include "utils.h"

/* Number of times each thread should recieve the signal */
#define ITERATIONS 10
/*
 * Factor by which to multiply number of online CPUs for total number of
 * worker threads
 */
#define THREAD_FACTOR 8

__thread double darray[] = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0,
		     1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0,
		     2.1};

bool bad_context;
int running;
volatile int not_ready;
extern long preempt_fpu(double *darray, volatile int *not_ready, int *sentinal);

void signal_fpu_sig(int sig, siginfo_t *info, void *context)
{
	int i;
	ucontext_t *uc = context;
	mcontext_t *mc = &uc->uc_mcontext;

	/* Only the non volatiles were loaded up */
	for (i = 14; i < 32; i++) {
		if (mc->fp_regs[i] != darray[i - 14]) {
			bad_context = true;
			break;
		}
	}
}

void *signal_fpu_c(void *p)
{
	int i;
	long rc;
	struct sigaction act;
	act.sa_sigaction = signal_fpu_sig;
	act.sa_flags = SA_SIGINFO;
	rc = sigaction(SIGUSR1, &act, NULL);
	if (rc)
		return p;

	srand(pthread_self());
	for (i = 0; i < 21; i++)
		darray[i] = rand();

	rc = preempt_fpu(darray, &not_ready, &running);

	return (void *) rc;
}

int test_signal_fpu(void)
{
	int i, j, rc, threads;
	void *rc_p;
	pthread_t *tids;

	threads = sysconf(_SC_NPROCESSORS_ONLN) * THREAD_FACTOR;
	tids = malloc(threads * sizeof(pthread_t));
	FAIL_IF(!tids);

	running = true;
	not_ready = threads;
	for (i = 0; i < threads; i++) {
		rc = pthread_create(&tids[i], NULL, signal_fpu_c, NULL);
		FAIL_IF(rc);
	}

	setbuf(stdout, NULL);
	printf("\tWaiting for all workers to start...");
	while (not_ready);
	printf("done\n");

	printf("\tSending signals to all threads %d times...", ITERATIONS);
	for (i = 0; i < ITERATIONS; i++) {
		for (j = 0; j < threads; j++) {
			pthread_kill(tids[j], SIGUSR1);
		}
		sleep(1);
	}
	printf("done\n");

	printf("\tKilling workers...");
	running = 0;
	for (i = 0; i < threads; i++) {
		pthread_join(tids[i], &rc_p);

		/*
		 * Harness will say the fail was here, look at why signal_fpu
		 * returned
		 */
		if ((long) rc_p || bad_context)
			printf("oops\n");
		if (bad_context)
			fprintf(stderr, "\t!! bad_context is true\n");
		FAIL_IF((long) rc_p || bad_context);
	}
	printf("done\n");

	free(tids);
	return 0;
}

int main(int argc, char *argv[])
{
	return test_harness(test_signal_fpu, "fpu_signal");
}
