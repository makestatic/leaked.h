/*
 * MULTI-THREAD TEST FOR LEAKED.H (program should crash)
 */

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#define LEAKED_IMPLEMENTATION
#define LEAKED_THREAD_SAFE
#include "leaked.h"

void* worker(void* arg)
{
	(void)arg;
	leaked_init();

	void* a = malloc(32);
	void* b = malloc(64);
	free(a);
	free(a);	 // invalid free
	return NULL; // b leaked
}

void* crash_thread(void* arg)
{
	(void)arg;
	usleep(500000);
	*(volatile int*)0 = 1; // crash
	return NULL;
}

int main(void)
{
	pthread_t t1, t2;
	pthread_create(&t1, NULL, worker, NULL);
	pthread_create(&t2, NULL, crash_thread, NULL);
	pthread_join(t1, NULL);
	pthread_join(t2, NULL);

	void* c = malloc(128); // main thread leak
	return 0;
}
