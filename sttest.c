/*
 * SINGL-THREAD TEST FOR LEAKED.H (program should crash)
 */

#define LEAKED_IMPLEMENTATION
#include "leaked.h"

int main(void)
{
	leaked_init();

	void* a = malloc(32);
	void* b = malloc(64);
	free(a);
	free(a);			 // invalid free
	free((void*)0x1234); // (fake pointer)

	void* c = malloc(128); // leaked
	void* d = malloc(256); // leaked

	*(volatile int*)0 = 1; // crash

	return 0;
}
